cmake_minimum_required(VERSION 3.12)
find_package(mariadb-plugin REQUIRED)
MARIADB_ADD_PLUGIN(disabled_functions disabled_functions.cc
  MODULE_ONLY
  RECOMPILE_FOR_EMBEDDED
  AUTHOR lefred
  VERSION 0.2.0
  DESCRIPTION "Disables selected native built-in SQL functions")
include(CPack)
