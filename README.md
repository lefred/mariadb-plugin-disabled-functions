# mariadb-plugin-disabled-functions

![mariabd-plugin-disabled-functions](logo/disabled_func.png)

`disabled_functions` is a MariaDB daemon plugin that disables selected native
built-in SQL functions at server startup.

The functions to disable are configured with the
`disabled_functions_functions` system variable:

```ini
[mariadb]
plugin_load_add=disabled_functions
disabled_functions_functions=SLEEP,COLUMN_LIST,LOAD_FILE
```

The list can be comma or whitespace separated. Function names are matched
case-insensitively.

```sql
SELECT plugin_name, plugin_type, plugin_library,
       plugin_description, plugin_author
  FROM information_schema.PLUGINS
 WHERE plugin_name = 'disabled_functions';
```

Result:

```text
+--------------------+-------------+-----------------------+------------------------------------------------+---------------+
| plugin_name        | plugin_type | plugin_library        | plugin_description                             | plugin_author |
+--------------------+-------------+-----------------------+------------------------------------------------+---------------+
| disabled_functions | DAEMON      | disabled_functions.so | Disables selected native built-in SQL functions | lefred        |
+--------------------+-------------+-----------------------+------------------------------------------------+---------------+
```

## Build

Link or copy this directory into the MariaDB `plugin` directory as
`disabled_functions`, then configure MariaDB with the plugin enabled:

```bash
cmake -S /path/to/MariaDB-server \
  -B /path/to/build \
  -DPLUGIN_DISABLED_FUNCTIONS=DYNAMIC

cmake --build /path/to/build --target disabled_functions
```

## Install

The plugin should be loaded at server startup because it modifies the native
function registry during initialization:

```ini
[mariadb]
plugin_load_add=disabled_functions
disabled_functions_functions=SLEEP,COLUMN_LIST
```

The equivalent command-line options are:

```bash
mariadbd \
  --plugin-load-add=disabled_functions \
  --disabled-functions-functions=SLEEP,COLUMN_LIST
```

## Usage

Start MariaDB with a disable list:

```ini
[mariadb]
plugin_load_add=disabled_functions
disabled_functions_functions=SLEEP,COLUMN_LIST
```

Functions listed in `disabled_functions_functions` are no longer resolved as
native built-in functions:

```sql
SELECT SLEEP(0);
-- ERROR 1305 (42000): FUNCTION test.SLEEP does not exist

SELECT COLUMN_LIST(COLUMN_CREATE('a', 1));
-- ERROR 1305 (42000): FUNCTION test.COLUMN_LIST does not exist
```

Functions not listed continue to work normally:

```sql
SELECT ABS(-2);
-- 2
```

The configured list can be inspected with:

```sql
SELECT @@disabled_functions_functions;
```

## Limitations

`disabled_functions_functions` is read-only. The disable list is applied only
when the plugin is initialized during server startup.

Unloading the plugin does not restore disabled functions. They remain disabled
until the server is restarted without the plugin or with a different disable
list.

Only native built-in functions registered in MariaDB's native function registry
can be disabled by this plugin. Stored functions, UDFs, syntax-level constructs,
operators, and keywords are outside its scope.

## Test

```bash
cd /path/to/build/mysql-test
./mtr --suite=disabled_functions basic
```
