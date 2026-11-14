{
  *****************************************************************************
   Programs    : LuaToExe and LuaToExew
   Author      : NEUTS JL
   License     : GPL (GNU General Public License)
   Date        : 10/02/2025
   Version     : V1.2.0
   Date        : 10/11/2027
   Version     : V1.3.0  // Add LuaToExeW
   Date        : 14/11/2027
   Version     : V1.3.1  // Add icon program

   Description : This console and windows program builds an executable from lua
                 files.
                 This executable will be autonomous, it will include all the
                 resources of the application. This program checks, compiles each
                 LUA unit, searches for "require" and dependencies,
                 it automatically includes the necessary modules. It can also
                 integrate other types of files: DLL, Exe, images... The work is
                 simplified by adding pre-compilation directives. See help.

   This program is free software: you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the Free
   Software Foundation, either version 3 of the License, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along with
   this program. If not, see <https://www.gnu.org/licenses/>.
  *****************************************************************************
}
program luatoexew;

{$mode objfpc}{$H+}


uses
  {$IFDEF UNIX}
    cthreads,
  {$ENDIF}
  {$IFDEF WINDOWS}
    Windows,
  {$ENDIF}
  Classes,
  SysUtils,
  StrUtils,
  fileutil,
  CustApp,
  process,
  {$IFDEF WINDOWS}
    uexeicon,
  {$ENDIF}
  uresourceexe;

Const
  KVersion='V1.3.1';

{$IFDEF WINDOWS}
  {$IFDEF WIN64}
    {$IFDEF LUATOEXEW}
      {$R luatoexew-win64.res}
    {$ELSE}
      {$R luatoexe-win64.res}
    {$ENDIF}
  {$ELSE}
    {$IFDEF LUATOEXEW}
      {$R luatoexew-win32.res}
    {$ELSE}
      {$R luatoexe-win32.res}
    {$ENDIF}
  {$ENDIF}
{$ENDIF}

type
  TMyApplication = class(TCustomApplication)
  private
    FDebug: boolean;
    FQuiet: boolean;
    RXReader: TResourceExeReader;
    procedure SaveResourceToFile(AResName, AFileName: string);
    procedure RunLua;
    procedure BuildExe;
    procedure ExtractLua;
    procedure ShowVersion;
    procedure ShowHelp; virtual;
  protected
    procedure DoRun; override;
  public
    constructor Create(TheOwner: TComponent); override;
    destructor Destroy; override;
  end;

function GetTmpDir: string;
var
  Base: string;
  n: integer;
begin
  Base := IncludeTrailingPathDelimiter(GetTempDir) +
          IntToStr(GetProcessID)+FormatDateTime('yyyymmddhhnnsszzz', Now);
  n := 0;
  repeat
    if n = 0 then
      Result := Base
    else
      Result := Base + IntToStr(n);
    if CreateDir(Result) then
      break;
    Inc(n);
    if n > 1000 then
      Raise Exception.Create('Cannot create temporary directory : ' + Base);
  until False;
  Result := IncludeTrailingPathDelimiter(Result);
end;

function ResToPath(const ResName: string): string;
begin
  Result := StringReplace(ResName, '/', PathDelim, [rfReplaceAll]);
end;

function LUAPath(Path:string):string;
begin
  Result:=StringReplace(Path,'\','/',[rfReplaceAll]);
end;

{$IFDEF LUATOEXEW}
// Change the PE "Subsystem" field of an executable to WINDOWS_GUI (2) so that
// Windows does not open a console window when it starts. The appended Lua
// resources are after the end of the image and are not affected.
procedure SetGuiSubsystem(const AFileName: string);
const
  IMAGE_SUBSYSTEM_WINDOWS_GUI = 2;
  IMAGE_SUBSYSTEM_WINDOWS_CUI = 3;
var
  F: TFileStream;
  PEOffset: LongWord;
  Signature: LongWord;
  Subsystem: Word;
begin
  F := TFileStream.Create(AFileName, fmOpenReadWrite or fmShareExclusive);
  try
    F.Position := $3C;
    F.ReadBuffer(PEOffset, SizeOf(PEOffset));
    F.Position := PEOffset;
    F.ReadBuffer(Signature, SizeOf(Signature));
    if Signature <> $00004550 then // 'PE'#0#0
      Raise Exception.Create('Invalid PE file : ' + AFileName);
    // Optional header = signature (4) + file header (20); Subsystem is at
    // offset 68 of the optional header for both PE32 and PE32+.
    F.Position := PEOffset + 4 + 20 + 68;
    F.ReadBuffer(Subsystem, SizeOf(Subsystem));
    if Subsystem = IMAGE_SUBSYSTEM_WINDOWS_CUI then
    begin
      Subsystem := IMAGE_SUBSYSTEM_WINDOWS_GUI;
      F.Position := PEOffset + 4 + 20 + 68;
      F.WriteBuffer(Subsystem, SizeOf(Subsystem));
    end;
  finally
    F.Free;
  end;
end;
{$ENDIF}

procedure TMyApplication.SaveResourceToFile(AResName, AFileName: string);
var
  S: TResourceStream;
  F: TFileStream;
begin
  S := TResourceStream.Create(HInstance, AResName, RT_RCDATA);
  try
    F := TFileStream.Create(AFileName, fmCreate);
    try
      F.CopyFrom(S, S.Size);
    finally
      F.Free;
    end;
  finally
    S.Free;
  end;
end;


procedure TMyApplication.RunLua;
var
  DataPath, ResFile: string;
  FFiles,FFile: TStringList;
  AProcess:TProcess;
  i:integer;
begin
  DataPath := GetTmpDir;
  FFiles := TStringList.Create;
  FFile := TStringList.Create;
  AProcess:=TPRocess.Create(Nil);
  try
    SetCurrentDir(DataPath);
    RXReader.List(FFiles);
    FFile.Add('_DATA_PATH='''+LuaPath(DataPath)+'''');
    FFile.Add('_APP_PATH='''+LuaPath(ExtractFilePath(ParamStr(0)))+'''');
    FFile.Add('_APP_FILE='''+LuaPath(ParamStr(0))+'''');
    FFile.Add('_APP_NAME='''+ExtractFileName(ChangeFileExt(ParamStr(0),'')+''''));
    {$IFDEF LUATOEXEW}
    // Exe built with --#icon : wlua.exe contains the icon as resource
    // IUP_ICON. IUP does not use it by itself : every iup.dialog without an
    // icon gets it (title bar, alt+tab, taskbar).
    if FindResource(HInstance, 'IUP_ICON', PChar(PtrUInt(14))) <> 0 then
    begin
      FFile.Add('_APP_ICON=''IUP_ICON''');
      FFile.Add('do');
      FFile.Add('  local _require = require');
      FFile.Add('  function require(name)');
      FFile.Add('    local m = _require(name)');
      FFile.Add('    if name == ''iuplua'' and type(m) == ''table'' and not m.__LuaToExeIcon then');
      FFile.Add('      m.__LuaToExeIcon = true');
      FFile.Add('      local dialog = m.dialog');
      FFile.Add('      m.dialog = function(t, ...)');
      FFile.Add('        if type(t) == ''table'' and t.icon == nil then t.icon = _APP_ICON end');
      FFile.Add('        return dialog(t, ...)');
      FFile.Add('      end');
      FFile.Add('    end');
      FFile.Add('    return m');
      FFile.Add('  end');
      FFile.Add('end');
    end
    else
      FFile.Add('_APP_ICON=nil');
    {$ENDIF}
    FFile.Add('require('''+ChangeFileExt(FFiles[0],'')+''')');
    FFile.SaveToFile('__ini__.lua');
    for i := 0 to FFiles.Count - 1 do
    begin
      ResFile := DataPath + ResToPath(FFiles[i]);
      ForceDirectories(ExtractFilePath(ResFile));
      RXReader.SaveToFile(FFiles[i], ResFile);
    end;
    SaveResourceToFile('lua54','Lua54.dll');
    {$IFDEF LUATOEXEW}
      SaveResourceToFile('iup','iup.dll');
      SaveResourceToFile('iuplua','iuplua.dll');
      SaveResourceToFile('iuplua54','iuplua54.dll');
      SaveResourceToFile('lua.exe','lua.exe.manifest');
      SaveResourceToFile('wlua','wLua.exe');
      AProcess.Executable := DataPath + 'wLua.exe';
    {$ELSE}
      SaveResourceToFile('lua','Lua.exe');
      AProcess.Executable := DataPath + 'Lua.exe';
    {$ENDIF}
    AProcess.CurrentDirectory := DataPath;
    AProcess.Parameters.Add('__ini__.lua');
    for i:=1 to ParamCount do
      AProcess.Parameters.Add(ParamStr(i));
    AProcess.Options := [poWaitOnExit];
    AProcess.Execute;
    System.ExitCode := AProcess.ExitStatus;
  finally
    SetCurrentDir('/');
    DeleteDirectory(DataPath,False);
    AProcess.Free;
    FFiles.Free;
    FFile.Free;
  end;
end;

procedure TMyApplication.BuildExe;
var
  // Resource names : paths relative to Root, with '/' separators
  // ('main.lua', 'lib/utils.lua', 'images/logo.bmp')
  FilesToAdd: TStringList;
  RXBuilder: TResourceExeBuilder;
  TmpDir, Compiler, OutFile: string;
  Root: string;
  IconFile: string;

  function ResolvePath(const AFile: string): string;
  begin
    if (ExtractFileDrive(AFile) <> '') or
       ((AFile <> '') and (AFile[1] in ['\', '/'])) then
      Result := ExpandFileName(AFile)
    else
      Result := ExpandFileName(Root + AFile);
  end;

  function ToResName(const FullName, Origin: string): string;
  begin
    Result := ExtractRelativePath(Root, FullName);
    if (Copy(Result, 1, 2) = '..') or (ExtractFileDrive(Result) <> '') then
      Raise Exception.Create(FullName + ' (' + Origin + ') is outside the main ' +
        'script directory ' + Root + ' : move it into this directory');
    Result := StringReplace(Result, '\', '/', [rfReplaceAll]);
  end;

  function FullPath(const ResName: string): string;
  begin
    Result := Root + ResToPath(ResName);
  end;

  procedure AddFile(const FullName, Origin: string);
  var
    ResName: string;
  begin
    if not FileExists(FullName) then
      Raise Exception.Create(FullName + ' not found (' + Origin + ')');
    ResName := ToResName(FullName, Origin);
    if FilesToAdd.IndexOf(ResName) = -1 then
      FilesToAdd.Add(ResName);
  end;

  procedure GetFilesFromParams(StartParam: integer);
  var
    i, io: integer;
    Files: TSearchRec;
    LuaFile, Dir: string;
  begin
    for i := StartParam to ParamCount do
    begin
      LuaFile := ParamStr(i);
      if ExtractFileExt(LuaFile) = '' then
        LuaFile := LuaFile + '.lua';
      Dir := ExtractFilePath(ExpandFileName(LuaFile));
      io := FindFirst(LuaFile, faAnyfile, Files);
      if io <> 0 then
        Raise(Exception.Create(LuaFile + ' not found'));
      while io = 0 do
      begin
        if ((Files.Attr and faDirectory) = 0) then
        begin
          if Root = '' then
            Root := Dir;
          AddFile(Dir + Files.Name, 'command line');
        end;
        io := FindNext(Files);
      end;
      FindClose(Files);
    end;
    if FilesToAdd.Count = 0 then
      Raise Exception.Create('No file to add');
  end;

  // require('a.b') : looks for a/b.lua then a/b/init.lua (like Lua's
  // package.path ".\?.lua;.\?\init.lua"). A module not found is a Lua
  // standard or C module (iuplua...) provided by the runtime.
  procedure AddModule(const Module, Origin: string);
  var
    P: string;
  begin
    P := ResolvePath(StringReplace(Module, '.', PathDelim, [rfReplaceAll]));
    if FileExists(P + '.lua') then
      AddFile(P + '.lua', Origin)
    else if FileExists(P + PathDelim + 'init.lua') then
      AddFile(P + PathDelim + 'init.lua', Origin)
    else if not FQuiet then
      writeln('Extern module   : ' + Module + ' (' + Origin + ')');
  end;

  // --#include "file"
  // --#icon "file.ico"
  procedure DoDirective(const Comment, Origin: string);
  var
    Directive, Arg: string;
    p: integer;
  begin
    p := 1;
    while (p <= Length(Comment)) and not (Comment[p] in [' ', #9, '"', '''']) do
      Inc(p);
    Directive := LowerCase(Copy(Comment, 1, p - 1));
    Arg := Trim(Copy(Comment, p, MaxInt));
    if (Length(Arg) >= 2) and (Arg[1] in ['"', '''']) and (Arg[Length(Arg)] = Arg[1]) then
      Arg := Copy(Arg, 2, Length(Arg) - 2);
    Arg := Trim(Arg);
    if Directive = '#include' then
    begin
      if Arg = '' then
        Raise Exception.Create('Missing file name in --#include (' + Origin + ')');
      if not FileExists(ResolvePath(Arg)) then
        Raise Exception.Create(Arg + ' : File not found in include directive (' + Origin + ')');
      AddFile(ResolvePath(Arg), Origin);
    end
    else if Directive = '#icon' then
    begin
      {$IFDEF WINDOWS}
      if Arg = '' then
        Raise Exception.Create('Missing file name in --#icon (' + Origin + ')');
      Arg := ResolvePath(Arg);
      if not FileExists(Arg) then
        Raise Exception.Create(Arg + ' : File not found in icon directive (' + Origin + ')');
      if LowerCase(ExtractFileExt(Arg)) <> '.ico' then
        Raise Exception.Create(Arg + ' : the icon must be a .ico file (' + Origin + ')');
      if (IconFile <> '') and not SameFileName(IconFile, Arg) then
        Raise Exception.Create('Only one --#icon is allowed (' + Origin + ')');
      IconFile := Arg;
      {$ENDIF}
    end;
  end;

  // Scans a Lua source : finds require "..." and --# directives, skipping
  // strings and comments ("-- require x" or "s = 'require'" are ignored).
  procedure ScanLua(const ResName: string);
  var
    S, Word, Module: string;
    i, Len, Start: integer;

    function LineAt(Pos: integer): string;
    var
      k, n: integer;
    begin
      n := 1;
      for k := 1 to Pos - 1 do
        if S[k] = #10 then
          Inc(n);
      Result := ResName + ' line ' + IntToStr(n);
    end;

    // At S[i]='[' : returns the level of a long bracket [==[, or -1
    function LongBracketLevel(At: integer): integer;
    var
      j: integer;
    begin
      Result := -1;
      if (At > Len) or (S[At] <> '[') then
        exit;
      j := At + 1;
      while (j <= Len) and (S[j] = '=') do
        Inc(j);
      if (j <= Len) and (S[j] = '[') then
        Result := j - At - 1;
    end;

    // Skips a long bracket starting at i, returns its content
    function SkipLongBracket(Level: integer): string;
    var
      Close: string;
      p: integer;
    begin
      Close := ']' + StringOfChar('=', Level) + ']';
      Inc(i, Level + 2);
      p := PosEx(Close, S, i);
      if p = 0 then
      begin
        Result := Copy(S, i, MaxInt);
        i := Len + 1;
      end
      else
      begin
        Result := Copy(S, i, p - i);
        i := p + Length(Close);
      end;
    end;

    // Skips a quoted string starting at i, returns its (raw) content
    function SkipShortString: string;
    var
      Quote: char;
      St: integer;
    begin
      Quote := S[i];
      Inc(i);
      St := i;
      while (i <= Len) and (S[i] <> Quote) and (S[i] <> #10) do
      begin
        if S[i] = '\' then
          Inc(i);
        Inc(i);
      end;
      Result := Copy(S, St, i - St);
      Inc(i);
    end;

    procedure SkipSpaces;
    begin
      while (i <= Len) and (S[i] in [' ', #9, #13, #10]) do
        Inc(i);
    end;

  var
    Lvl: integer;
    Comment: string;
    FS: TStringList;
  begin
    FS := TStringList.Create;
    try
      FS.LoadFromFile(FullPath(ResName));
      S := FS.Text;
    finally
      FS.Free;
    end;
    Len := Length(S);
    i := 1;
    while i <= Len do
    begin
      if (S[i] = '-') and (i < Len) and (S[i + 1] = '-') then
      begin
        Start := i;
        Inc(i, 2);
        Lvl := LongBracketLevel(i);
        if Lvl >= 0 then
          SkipLongBracket(Lvl)                // --[[ block comment ]]
        else
        begin
          Start := i;                          // -- line comment
          while (i <= Len) and (S[i] <> #10) do
            Inc(i);
          Comment := TrimRight(Copy(S, Start, i - Start));
          if (Comment <> '') and (Comment[1] = '#') then
            DoDirective(Comment, LineAt(Start));
        end;
      end
      else if S[i] in ['"', ''''] then
        SkipShortString
      else if (S[i] = '[') and (LongBracketLevel(i) >= 0) then
        SkipLongBracket(LongBracketLevel(i))
      else if S[i] in ['A'..'Z', 'a'..'z', '_'] then
      begin
        Start := i;
        while (i <= Len) and (S[i] in ['A'..'Z', 'a'..'z', '_', '0'..'9']) do
          Inc(i);
        Word := Copy(S, Start, i - Start);
        // "require", but not "obj.require" or "obj:require"
        if (Word = 'require') and ((Start = 1) or not (S[Start - 1] in ['.', ':'])) then
        begin
          SkipSpaces;
          if (i <= Len) and (S[i] = '(') then
          begin
            Inc(i);
            SkipSpaces;
          end;
          Module := '';
          if (i <= Len) and (S[i] in ['"', '''']) then
            Module := SkipShortString
          else if (i <= Len) and (LongBracketLevel(i) >= 0) then
            Module := SkipLongBracket(LongBracketLevel(i));
          Module := Trim(Module);
          // require(variable) : cannot be resolved, use --#include
          if Module <> '' then
            AddModule(Module, LineAt(Start));
        end;
      end
      else if S[i] in ['0'..'9'] then
      begin
        while (i <= Len) and (S[i] in ['0'..'9', 'A'..'Z', 'a'..'z', '.', '_']) do
          Inc(i);
      end
      else
        Inc(i);
    end;
  end;

  procedure GetFilesByRequire;
  var
    i: integer;
  begin
    i := 0;
    while i < FilesToAdd.Count do
    begin
      if LowerCase(ExtractFileExt(FilesToAdd[i])) = '.lua' then
        ScanLua(FilesToAdd[i]);
      Inc(i);
    end;
  end;

  procedure CompilLua(const ResName, OutputFile: string);
  var
    AProcess: TProcess;
    StdErr: TStringStream;
    Dummy: TMemoryStream;

    procedure Drain;
    var
      n: integer;
    begin
      n := AProcess.Output.NumBytesAvailable;
      if n > 0 then
        Dummy.CopyFrom(AProcess.Output, n);
      n := AProcess.Stderr.NumBytesAvailable;
      if n > 0 then
        StdErr.CopyFrom(AProcess.Stderr, n);
    end;

  begin
    AProcess := TProcess.Create(nil);
    StdErr := TStringStream.Create('');
    Dummy := TMemoryStream.Create;
    try
      AProcess.Executable := Compiler;
      // Run in Root with a relative name : error messages show
      // "lib/utils.lua:12: ..." instead of a temporary path
      AProcess.CurrentDirectory := Root;
      if not FDebug then
        AProcess.Parameters.Add('-s');
      AProcess.Parameters.Add('-o');
      AProcess.Parameters.Add(OutputFile);
      AProcess.Parameters.Add(ResToPath(ResName));
      // No poWaitOnExit : the pipes are read while luac runs, otherwise a
      // long error output fills the pipe and both processes wait forever.
      AProcess.Options := [poUsePipes, poNoConsole];
      AProcess.Execute;
      while AProcess.Running do
      begin
        Drain;
        Sleep(10);
      end;
      Drain;
      if (AProcess.ExitStatus <> 0) or (StdErr.Size > 0) then
      begin
        if Trim(StdErr.DataString) <> '' then
          Raise Exception.Create(Trim(StringReplace(StdErr.DataString,
            Compiler + ': ', '', [rfReplaceAll, rfIgnoreCase])))
        else
          Raise Exception.Create('Error in lua compil : ' + IntToStr(AProcess.ExitStatus));
      end;
    finally
      AProcess.Free;
      StdErr.Free;
      Dummy.Free;
    end;
  end;

  procedure AddFilesIntoExe;
  var
    i: integer;
    TmpCompiledFile, BaseExe, Interpreter: string;
  begin
    if not FQuiet and FDebug then
      writeln('Adding Debug informations');
    for i := 0 to FilesToAdd.Count - 1 do
    begin
      if LowerCase(ExtractFileExt(FilesToAdd[i])) <> '.lua' then
      begin
        if not FQuiet then
          writeln('Add DATA file   : ' + FilesToAdd[i]);
        RXBuilder.AddFromFile(FilesToAdd[i], FullPath(FilesToAdd[i]));
      end
      else
      begin
        TmpCompiledFile := TmpDir + 'build' + PathDelim + ResToPath(FilesToAdd[i]);
        ForceDirectories(ExtractFilePath(TmpCompiledFile));
        if not FQuiet then
          writeln('Compil LUA file : ' + FilesToAdd[i]);
        CompilLua(FilesToAdd[i], TmpCompiledFile);
        RXBuilder.AddFromFile(FilesToAdd[i], TmpCompiledFile);
      end;
    end;
    if ExtractFileExt(OutFile) = '' then
      OutFile := ChangeFileExt(OutFile, '.exe');
    BaseExe := ExeName;
    {$IFDEF WINDOWS}
    // The icon is set on a copy of the launcher, before the scripts are
    // appended : updating resources would drop the data after the image.
    if IconFile <> '' then
    begin
      if not FQuiet then
        writeln('Set icon        : ' + ExtractFileName(IconFile));
      BaseExe := TmpDir + 'base.exe';
      if not CopyFile(ExeName, BaseExe) then
        Raise Exception.Create('Cannot copy ' + ExeName);
      SetExeIcon(BaseExe, IconFile);
      // The window belongs to the interpreter (wlua.exe / lua.exe) started by
      // the launcher : it gets the icon too (title bar, taskbar, IUP dialogs).
      {$IFDEF LUATOEXEW}
        Interpreter := 'wlua';
      {$ELSE}
        Interpreter := 'lua';
      {$ENDIF}
      SaveResourceToFile(Interpreter, TmpDir + Interpreter + '.exe');
      SetExeIcon(TmpDir + Interpreter + '.exe', IconFile);
      ReplaceRCData(BaseExe, Interpreter, TmpDir + Interpreter + '.exe');
    end;
    {$ENDIF}
    RXBuilder.ApplyToExe(BaseExe, OutFile);
    if not FileExists(OutFile) then
      Raise(Exception.Create('Error on generate exe file : '+OutFile));
    {$IFDEF LUATOEXEW}
      // The generated exe runs wlua.exe : no console window behind it.
      SetGuiSubsystem(OutFile);
    {$ENDIF}
    if not FQuiet then
      writeln('Exe file is generated on ' + OutFile);
  end;

var
  StartParam: integer;
begin
  StartParam := 1;
  if HasOption('q', 'quiet') then
    Inc(StartParam);
  if HasOption('d', 'debug') then
    Inc(StartParam);
  if HasOption('o', 'output') then
    Inc(StartParam, 2);
  OutFile := GetOptionValue('o', 'output');
  if ParamStr(StartParam) = '' then
  begin
    ShowVersion;
    writeln('Usage error see -h or --help');
    exit;
  end;
  Root := '';
  IconFile := '';
  TmpDir:=GetTmpDir;
  Compiler:=TmpDir+'luac.exe';
  RXBuilder := TResourceExeBuilder.Create;
  FilesToAdd := TStringList.Create;
  try
    SaveResourceToFile('luac',Compiler);
    GetFilesFromParams(StartParam);
    if OutFile = '' then
      OutFile := ChangeFileExt(FullPath(FilesToAdd[0]), '.exe');
    GetFilesByRequire;
    AddFilesIntoExe;
  finally
    DeleteDirectory(TmpDir,False);
    FilesToAdd.Free;
    RXBuilder.Free;
  end;
end;

// Extract the embedded Lua runtime (exe, dll...) into a directory :
// the current directory, or the one given after -e / --extract.
procedure TMyApplication.ExtractLua;
var
  Dir: string;
  i: integer;

  procedure Extract(AResName, AFileName: string);
  begin
    SaveResourceToFile(AResName, Dir + AFileName);
    if not FQuiet then
      writeln('Extract : ' + Dir + AFileName);
  end;

begin
  Dir := '';
  for i := 1 to ParamCount do
    if (ParamStr(i) <> '') and (ParamStr(i)[1] <> '-') then
    begin
      Dir := ParamStr(i);
      break;
    end;
  if Dir = '' then
    Dir := GetCurrentDir;
  Dir := IncludeTrailingPathDelimiter(ExpandFileName(Dir));
  if not ForceDirectories(Dir) then
    Raise(Exception.Create('Cannot create directory : ' + Dir));
  Extract('lua54', 'lua54.dll');
  Extract('luac', 'luac.exe');
  {$IFDEF LUATOEXEW}
    Extract('wlua', 'wlua.exe');
    Extract('lua.exe', 'wlua.exe.manifest');
    Extract('iup', 'iup.dll');
    Extract('iuplua', 'iuplua.dll');
    Extract('iuplua54', 'iuplua54.dll');
  {$ELSE}
    Extract('lua', 'lua.exe');
  {$ENDIF}
end;

procedure TMyApplication.DoRun;
var
  ErrorMsg: string;
begin
  try
    if RXReader.Count > 0 then
      RunLua
    else
    begin
      ErrorMsg := CheckOptions('o,q,d,h,v,e', ['output', 'quiet', 'debug', 'help', 'version', 'extract']);
      if ErrorMsg <> '' then
        writeln(ErrorMsg)
      else if HasOption('h', 'help') then
        ShowHelp
      else if HasOption('v', 'version') then
        ShowVersion
      else if HasOption('e', 'extract') then
      begin
        if HasOption('q', 'quiet') then
          FQuiet := True;
        ExtractLua;
      end
      else
      begin
        if HasOption('d', 'debug') then
          FDebug := True;
        if HasOption('q', 'quiet') then
          FQuiet := True;
        BuildExe;
      end;
    end;
  except
    on e: Exception do
    begin
      System.ExitCode := 1;
      {$IFDEF LUATOEXEW}
      // Generated exe (GUI, no console) : show the error in a message box.
      if RXReader.Count > 0 then
        MessageBox(0, PChar('Error : ' + e.message), PChar(Title),
          MB_OK or MB_ICONERROR)
      else
      {$ENDIF}
      begin
        Writeln;
        Writeln('Error : '+e.message);
      end;
    end;
  end;
  Terminate;
end;

constructor TMyApplication.Create(TheOwner: TComponent);
begin
  inherited Create(TheOwner);
  FQuiet := False;
  FDebug := False;
  StopOnException := True;
  RXReader := TResourceExeReader.Create;
end;

destructor TMyApplication.Destroy;
begin
  RXReader.Free;
  inherited Destroy;
end;

procedure TMyApplication.ShowVersion;
begin
  if not FQuiet then
  begin
    writeln('LuaToExe version '+KVersion);
    writeln('Copyright (c) 2008-2027, Neuts JL (http://www.neuts.fr)');
    writeln;
  end;
end;

procedure TMyApplication.ShowHelp;
begin
  FQuiet := False;
  ShowVersion;
  writeln('Embedded a lua script(s) in an executable without external LUA DLL (one file)');
  writeln('Usage: LuaToExe options script1 scriptN...');
  writeln;
  writeln('Scripts :');
  writeln('  The first script is the main file. File scripts to embed into the ');
  writeln('  the executable , (mask ?,* are accepted)');
  writeln;
  writeln('Options:');
  writeln('  -o fileout, --output fileout Set output executable file. Default is <script1>.exe');
  writeln('  -q,         --quiet          Be quiet, don''t output anything except on error.');
  writeln('  -d,         --debug          This option allows to display error messages');
  writeln('                               with call references. It is better to omit it in');
  writeln('                               production, it makes the code less decipherable.');
  writeln('  -e [dir],   --extract [dir]  Extract the embedded Lua runtime (exe, dll)');
  writeln('                               into dir (default : current directory)');
  writeln('  -h,         --help           Display this help');
  writeln('  -v,         --version        Display version information');
  writeln;
  writeln('In LUA file, for use :');
  writeln('require("lib.utils")           lib/utils.lua (or lib/utils/init.lua) is');
  writeln('                               added automatically');
  writeln('--#include "datafile"          Import data file in executable');
  writeln('--#icon "file.ico"             Icon of the executable');
  writeln('Paths are relative to the directory of the main script and are kept');
  writeln('in the executable (lib/utils.lua -> _DATA_PATH..''lib/utils.lua'')');
  writeln('_DATA_PATH                     Data path');
  writeln('_APP_PATH                      Executable path');
  writeln('_APP_FILE                      Executable file');
  writeln('_APP_NAME                      Executable name');
  {$IFDEF LUATOEXEW}
  writeln('_APP_ICON                      ''IUP_ICON'' if --#icon is used, else nil');
  {$ENDIF}
  writeln('argv[0]                        Parameter 0 value');
  writeln('argv[n]                        Parameter n value');
end;


// Device that intercepts Ctrl+C from a Lua script without stopping the
// main application. This allows to clean up temporary files before closing.
function ConsoleCtrlHandler(CtrlType: DWORD): LongBool; stdcall;
begin
  if CtrlType = CTRL_C_EVENT then
  begin
    writeln;
    writeln('Break for LUA program');
    Result := True;
  end
  else
    Result := False;
end;

var
  Application: TMyApplication;
begin
  {$IFDEF UNIX}
     fpSignal(SIGINT, @SignalCtrlHandler);
  {$ENDIF}
  {$IFDEF WINDOWS}
    SetConsoleCtrlHandler(@ConsoleCtrlHandler, True);
  {$ENDIF}
  Application := TMyApplication.Create(nil);
  Application.Title := ExtractFileName(ChangeFileExt(ParamStr(0),''));
  Application.Run;
  Application.Free;
end.
