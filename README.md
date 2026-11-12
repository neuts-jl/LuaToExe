# LuaToExe

Builds an executable containing all the files of a LUA application
This executable will be autonomous, it will include all the
resources of the application. This program checks, compiles each
LUA unit, searches for "require" and dependencies,
it automatically includes the necessary modules. It can also
integrate other types of files: DLL, Exe, images... The work is
simplified by adding pre-compilation directives. 

## Dependencies
Compiled under LAZARUS with my lib [RuntimeResources](https://github.com/neuts-jl/RuntimeResources)

## Binaries 
[![SourceForge](https://img.shields.io/badge/SourceForge-Download-orange)](https://sourceforge.net/projects/luatoexe/files/latest/download)

## Demonstration :
Command line : LuatoExe demo-form

See [lualibconsole](https://github.com/neuts-jl/lualibconsole) 


## Command line help :
```
      LuaToExe version V1.3.0
      Copyright (c) 2008-2027, Neuts JL (http://www.neuts.fr)

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
        -e [dir],   --extract [dir]  Extract the embedded Lua runtime (exe, dll)
                                     into dir (default : current directory)
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
```

## LuaToExeW : Windows applications (IUP)

`luatoexew` is the Windows (GUI) version of LuaToExe. It is built from the
same source (`main.inc`) with the `LUATOEXEW` define (project `luatoexew.lpi`)
and takes the same command line.

Differences with `LuaToExe` :

| | LuaToExe | LuaToExeW |
|---|---|---|
| Lua interpreter | `lua.exe` (console) | `wlua.exe` (no console) |
| Embedded libraries | `lua54.dll` | `lua54.dll`, `iup.dll`, `iuplua.dll`, `iuplua54.dll` |
| Visual style | | `manifest` : Windows common controls v6 (modern buttons) and DPI aware |
| Generated exe | console program | Windows program : **no console window behind it** |

LuaToExeW changes the PE *Subsystem* field of the generated executable from
console (3) to Windows GUI (2). Windows then starts it without opening a
console. LuaToExeW itself stays a console program, so its messages are still
displayed when it builds an exe. In a generated exe, launcher errors are shown
in a message box.

### Developing a Windows application

The GUI library is [IUP](https://www.tecgraf.puc-rio.br/iup/) (Tecgraf),
loaded with `require("iuplua")`.

1. Get the runtime in your development directory :
   ```
   luatoexew -e
   ```
2. Write and test the script without a console :
   ```
   wlua testwin.lua
   ```
   or with a console, to see `print` and error messages :
   ```
   lua testwin.lua
   ```
3. Build the executable :
   ```
   luatoexew testwin.lua
   ```

Example `testwin.lua` : a window with an image, an OK button that shows a
message and a Close button.

```lua
--#include "testwin.bmp"

local iup = require("iuplua")
iup.SetGlobal("UTF8MODE", "YES")

-- Embedded files are extracted into _DATA_PATH. When the script is
-- run directly, _DATA_PATH is nil : the current directory is used.
local DATA_PATH = _DATA_PATH or ""

local image = loadBmp(DATA_PATH .. "testwin.bmp")   -- see testwin.lua

local okButton = iup.button{
  title = "OK",
  action = function() iup.Message("Message", "You clicked OK.") end,
}
local closeButton = iup.button{
  title = "Close",
  action = function() return iup.CLOSE end,
}

local dialog = iup.dialog{
  iup.hbox{
    iup.label{image = image},
    iup.vbox{
      iup.label{title = "Hello! Click OK to show a message."},
      iup.fill{},
      iup.hbox{iup.fill{}, okButton, closeButton; gap = 10},
    },
    margin = "20x20", gap = 20,
  },
  title = "Test window",
  defaultenter = okButton,
  defaultesc = closeButton,
}
dialog:showxy(iup.CENTER, iup.CENTER)
iup.MainLoop()
```

Notes :
- IUP without `iupim.dll` cannot read image files : `testwin.lua` contains a
  small pure Lua loader for uncompressed 24 or 32 bit BMP files
  (`iup.imagergb`).
- Files added with `--#include` are in `_DATA_PATH` (temporary directory,
  deleted when the program ends). Files the program writes must go to
  `_APP_PATH` (next to the exe).
- `wlua.exe` has no console : `print` and `io.write` display nothing. Use
  IUP controls (`iup.Message`, a multiline `iup.text`...) instead.

## Extract the Lua runtime : -e, --extract

```
LuaToExe  -e [dir]
luatoexew --extract [dir]
```

Writes the Lua runtime embedded in LuaToExe into `dir` (the current directory
by default; it is created if needed). This gives the exact interpreter and
libraries that the generated executables use, to develop and test scripts
before building them.

| LuaToExe | LuaToExeW |
|---|---|
| `lua54.dll` | `lua54.dll` |
| `luac.exe` | `luac.exe` |
| `lua.exe` | `wlua.exe` |
| | `wlua.exe.manifest` |
| | `iup.dll`, `iuplua.dll`, `iuplua54.dll` |

Existing files are overwritten. With `-q`, nothing is displayed.


