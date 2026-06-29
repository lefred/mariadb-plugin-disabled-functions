/* Copyright (c) 2026, lefred (Frédéric Descamps)
   Copyright (c) 2026, MariaDB Foundation.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1335  USA */

#include "mariadb.h"
#include "sql_priv.h"
#include "mysql/plugin.h"
#include "my_sys.h"
#include "mysqld_error.h"

#include <ctype.h>
#include <string.h>

class Create_func;

struct Native_func_registry
{
  LEX_CSTRING name;
  Create_func *builder;
};

class Native_functions_hash
{
public:
  bool append(const Native_func_registry array[], size_t count);
  bool remove(const Native_func_registry array[], size_t count);
};

class Native_func_registry_array
{
  const Native_func_registry *m_elements;
  size_t m_count;
public:
  const Native_func_registry *elements() const { return m_elements; }
  size_t count() const { return m_count; }
};

extern Native_func_registry_array native_func_registry_array;
extern Native_func_registry_array native_func_registry_array_geom;
extern Native_func_registry_array oracle_func_registry_array;
extern Native_functions_hash native_functions_hash;
extern Native_functions_hash native_functions_hash_oracle;

struct st_mysql_daemon disabled_functions_plugin=
{ MYSQL_DAEMON_INTERFACE_VERSION };

static char *disabled_functions_names;

static MYSQL_SYSVAR_STR(list, disabled_functions_names,
  PLUGIN_VAR_RQCMDARG | PLUGIN_VAR_READONLY,
  "Comma or whitespace separated list of native built-in SQL functions to "
  "disable at startup, for example COLUMN_LIST,LOAD_FILE,SLEEP",
  NULL, NULL, "");

static struct st_mysql_sys_var *disabled_functions_system_variables[]=
{
  MYSQL_SYSVAR(list),
  NULL
};

struct Removed_function
{
  Native_functions_hash *hash;
  const Native_func_registry *entry;
};

static Removed_function removed_functions[1024];
static uint removed_functions_count;

static bool remove_from_hash(Native_functions_hash *hash,
                             const Native_func_registry *entry);

static bool same_function_name(const LEX_CSTRING &name, const char *token,
                               size_t token_length)
{
  if (name.length != token_length)
    return false;

  for (size_t i= 0; i < token_length; i++)
  {
    if (toupper(static_cast<uchar>(name.str[i])) !=
        toupper(static_cast<uchar>(token[i])))
      return false;
  }
  return true;
}

static bool remove_function_entry(const Native_func_registry *entry,
                                  bool *removed)
{
  uint before= removed_functions_count;

  if (!remove_from_hash(&native_functions_hash, entry))
    *removed= *removed || removed_functions_count != before;

  before= removed_functions_count;
  if (!remove_from_hash(&native_functions_hash_oracle, entry))
    *removed= *removed || removed_functions_count != before;

  return false;
}

static bool remove_from_hash(Native_functions_hash *hash,
                             const Native_func_registry *entry)
{
  if (hash->remove(entry, 1))
    return false;

  if (removed_functions_count == array_elements(removed_functions))
  {
    my_printf_error(ER_UNKNOWN_ERROR,
                    "disabled_functions: too many disabled function entries",
                    ME_ERROR_LOG_ONLY);
    return true;
  }

  removed_functions[removed_functions_count].hash= hash;
  removed_functions[removed_functions_count].entry= entry;
  removed_functions_count++;
  return false;
}

static bool disable_functions_from_array(const Native_func_registry_array &array,
                                         const char *token,
                                         size_t token_length,
                                         bool *known, bool *removed)
{
  for (size_t i= 0; i < array.count(); i++)
  {
    const Native_func_registry *entry= array.elements() + i;
    if (!same_function_name(entry->name, token, token_length))
      continue;

    *known= true;
    if (remove_function_entry(entry, removed))
      return true;
  }
  return false;
}

static bool disable_function(const char *token, size_t token_length)
{
  bool known= false;
  bool removed= false;

  if (disable_functions_from_array(native_func_registry_array, token,
                                   token_length, &known, &removed) ||
      disable_functions_from_array(native_func_registry_array_geom, token,
                                   token_length, &known, &removed) ||
      disable_functions_from_array(oracle_func_registry_array, token,
                                   token_length, &known, &removed))
    return true;

  if (!known)
  {
    my_printf_error(ER_UNKNOWN_ERROR,
                    "disabled_functions: unknown native function '%.*s'",
                    ME_ERROR_LOG_ONLY, static_cast<int>(token_length), token);
    return true;
  }

  if (!removed)
  {
    my_printf_error(ER_UNKNOWN_ERROR,
                    "disabled_functions: function '%.*s' was not registered",
                    ME_ERROR_LOG_ONLY, static_cast<int>(token_length), token);
    return true;
  }

  my_printf_error(ER_UNKNOWN_ERROR, "disabled_functions: disabled %.*s",
                  ME_ERROR_LOG_ONLY | ME_NOTE,
                  static_cast<int>(token_length), token);
  return false;
}

static bool parse_and_disable_functions()
{
  const char *list= disabled_functions_names;

  if (!list || !list[0])
    return false;

  while (*list)
  {
    const char *start;
    size_t length;

    while (*list && (isspace(static_cast<uchar>(*list)) || *list == ','))
      list++;

    start= list;
    while (*list && !isspace(static_cast<uchar>(*list)) && *list != ',')
      list++;

    length= static_cast<size_t>(list - start);
    if (length && disable_function(start, length))
      return true;
  }

  return false;
}

static int disabled_functions_plugin_init(void *p __attribute__((unused)))
{
  removed_functions_count= 0;
  return parse_and_disable_functions() ? 1 : 0;
}

static int disabled_functions_plugin_deinit(void *p __attribute__((unused)))
{
  /*
    The native function hashes are startup/shutdown data structures. During
    server shutdown, plugin deinit may run after item_create_cleanup(), so
    restoring deleted entries is not safe here. The sysvar is read-only, so
    disabling functions is intentionally a startup-only operation.
  */
  removed_functions_count= 0;
  return 0;
}

maria_declare_plugin(disabled_functions)
{
  MYSQL_DAEMON_PLUGIN,
  &disabled_functions_plugin,
  "disabled_functions",
  "lefred",
  "Disables selected native built-in SQL functions",
  PLUGIN_LICENSE_GPL,
  disabled_functions_plugin_init,
  disabled_functions_plugin_deinit,
  0x0100,
  NULL,
  disabled_functions_system_variables,
  "1.0",
  MariaDB_PLUGIN_MATURITY_ALPHA
}
maria_declare_plugin_end;
