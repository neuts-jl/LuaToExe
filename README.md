# LuaToExe

Builds an executable containing all the files of a LUA application
This executable will be autonomous, it will include all the
resources of the application. This program checks, compiles each
LUA unit, searches for "require" and dependencies,
it automatically includes the necessary modules. It can also
integrate other types of files: DLL, Exe, images... The work is
simplified by adding pre-compilation directives. 
Compiled with  [xxxx](https://github.com/neuts-jl/xxxx) 

## Command line help :
  LuaToExe version V1.2.0
  Copyright (c) 2008-2025, Neuts JL (http://www.neuts.fr)

  Embedded a lua script(s) in an executable without external LUA DLL (one file)
  Usage: LuaToExe options script1 scriptN...

  Scripts :
    The first script is the main file. File scripts to embed into the
    the executable , (mask ?,* are accepted)

  Options:
    -o fileout, --output fileout Set output executable file. Default is <script1>.exe
    -q,         --quiet          Be quiet, don't output anything except on error.
    -d,         --debug          This option allows to display error messages
                                with call references. It is better to omit it in
                                production, it makes the code less decipherable.
    -h,         --help           Display this help
    -v,         --version        Display version information

  In LUA file, for use :
  --#include "datafile"          Import data file in executable
  _DATA_PATH                     Data path
  _APP_PATH                      Executable path
  _APP_FILE                      Executable file
  _APP_NAME                      Executable name
  argv[0]                        Parameter 0 value
  argv[n]                        Parameter n value

## demonstration :

LuatoExe demo-form

See [lualibconsole](https://github.com/neuts-jl/lualibconsole) 


