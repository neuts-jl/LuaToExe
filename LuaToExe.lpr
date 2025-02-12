{
  *****************************************************************************
   Program     : LuaToExe
   Author      : NEUTS JL
   License     : GPL (GNU General Public License)
   Date        : 10/02/2025
   Version     : V1.2.0

   Description : This console program builds an executable from lua files.
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
program LuaToExe;

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
  fileutil,
  CustApp,
  process,
  uresourceexe;

Const
  KVersion='V1.2.0';

{$IFDEF WINDOWS}
  {$IFDEF WIN64}
    {$R luatoexe-win64.res}
  {$ELSE}
    {$R luatoexe-win32.res}
    {$ENDIF}
{$ENDIF}
{$IFDEF UNIX}
  {$R luatoexe-linux64.res}
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
    procedure ShowVersion;
    procedure ShowHelp; virtual;
  protected
    procedure DoRun; override;
  public
    constructor Create(TheOwner: TComponent); override;
    destructor Destroy; override;
  end;

function GetTmpDir: string;
begin
  Result := IncludeTrailingPathDelimiter(GetTempDir) +
            FormatDateTime('yyyymmddhhnnsszzz', Now) + '\';
  ForceDirectories(Result);
end;

function LUAPath(Path:string):string;
begin
  Result:=StringReplace(Path,'\','/',[rfReplaceAll]);
end;

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
  DataPath: string;
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
    FFile.Add('require('''+ChangeFileExt(FFiles[0],'')+''')');
    FFile.SaveToFile('__ini__.lua');
    RXReader.SaveToDir(DataPath);
    SaveResourceToFile('lua','Lua.exe');
    SaveResourceToFile('lua54','Lua54.dll');
    AProcess.Executable := 'lua.exe';
    AProcess.Parameters.Add('__ini__.lua');
    for i:=1 to ParamCount do
      AProcess.Parameters.Add(ParamStr(i));
    AProcess.Options := [poWaitOnExit];
    AProcess.Execute;
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
  FilesToAdd: TStringList;
  FFile: TStringList;
  RXBuilder: TResourceExeBuilder;
  TmpDir, Compiler,OutFile: string;

  procedure GetFilesFromParams(StartParam: integer);
  var
    i, io: integer;
    Files: TSearchRec;
    LuaFile: string;
  begin
    for i := StartParam to ParamCount do
    begin
      LuaFile := ParamStr(i);
      if ExtractFileExt(LuaFile) = '' then
        LuaFile := LuaFile + '.lua';
      io := FindFirst(LuaFile, faAnyfile, Files);
      if io <> 0 then
        Raise(Exception.Create(LuaFile + ' not found'));
      while io = 0 do
      begin
        if ((Files.Attr and faDirectory) = 0) and
          (Files.Name <> '.') and (Files.Name <> '..') and
          (FilesToAdd.IndexOf(Files.Name) = -1) then
          FilesToAdd.Add(Files.Name);
        io := FindNext(Files);
      end;
      FindClose(Files);
    end;
  end;

  procedure GetFilesByRequire;
  var
    i, j: integer;
    LuaFile: string;
  begin
    for i := 0 to FilesToAdd.Count - 1 do
    begin
      if ExtractFileExt(FilesToAdd[i]) = '' then
        FilesToAdd[i] := FilesToAdd[i] + '.lua';
      if not FileExists(FilesToAdd[i]) then
        Raise(Exception.Create(FilesToAdd[i] + ' not found'));
      if LowerCase(ExtractFileExt(FilesToAdd[i])) = '.lua' then
      begin
        FFile.LoadFromFile(FilesToAdd[i]);
        for j := 0 to FFile.Count - 1 do
        begin
          LuaFile := Trim(FFile[j]);
          if Pos('require', LuaFile) = 1 then
          begin
            Delete(LuaFile, 1, 7);
            LuaFile:=StringReplace(LuaFile,'''','',[rfReplaceAll]);
            LuaFile:=StringReplace(LuaFile,'"','',[rfReplaceAll]);
            LuaFile:=StringReplace(LuaFile,'(','',[rfReplaceAll]);
            LuaFile:=StringReplace(LuaFile,')','',[rfReplaceAll]);
            LuaFile:=Trim(LuaFile);
            if ExtractFileExt(LuaFile) = '' then
              LuaFile := LuaFile + '.lua';
            if not FileExists(LuaFile) then
              Raise(Exception.Create(LuaFile+' : File not found in require statement of file '+FilesToAdd[i]));
            if FilesToAdd.IndexOf(LuaFile) = -1 then
              FilesToAdd.Add(LuaFile);
          end
          else if Pos('--#include', LuaFile) = 1 then
          begin
            Delete(LuaFile, 1, 10);
            LuaFile:=StringReplace(LuaFile,'''','',[rfReplaceAll]);
            LuaFile:=StringReplace(LuaFile,'"','',[rfReplaceAll]);
            LuaFile:=Trim(LuaFile);
            if not FileExists(LuaFile) then
            Raise(Exception.Create(LuaFile+' : File not found in include directive of file '+FilesToAdd[i]));
            if FilesToAdd.IndexOf(LuaFile) = -1 then
              FilesToAdd.Add(LuaFile);
          end;
        end;
      end;
    end;
  end;

  procedure CompilLua(SourceFile, OutputFile: string);
  var
    AProcess: TProcess;
    StdOutStream, StdErrStream: TStringStream;
  begin
    AProcess := TProcess.Create(nil);
    StdOutStream := TStringStream.Create('');
    StdErrStream := TStringStream.Create('');
    try
      AProcess.Executable := Compiler;
      if not FDebug then
        AProcess.Parameters.Add('-s');
      AProcess.Parameters.Add('-o');
      AProcess.Parameters.Add(OutputFile);
      AProcess.Parameters.Add(SourceFile);
      AProcess.Options := [poUsePipes, poWaitOnExit];
      AProcess.Execute;
      StdOutStream.CopyFrom(AProcess.Output, AProcess.Output.NumBytesAvailable);
      StdErrStream.CopyFrom(AProcess.Stderr, AProcess.Stderr.NumBytesAvailable);
      if StdErrStream.Size > 0 then
        Raise(Exception.Create(StringReplace(StdErrStream.DataString,TmpDir,'',[rfReplaceAll])));
      if AProcess.ExitCode<>0 then
        Raise(Exception.Create('Error in lua compil :'+IntToStr(AProcess.ExitCode)));
    finally
      AProcess.Free;
      StdOutStream.Free;
      StdErrStream.Free;
    end;
  end;

  procedure AddFilesIntoExe;
  var
    i: integer;
    TmpCompiledFile, FileName: string;
  begin
    if not FQuiet and FDebug then
      writeln('Adding Debug informations');
    for i := 0 to FilesToAdd.Count - 1 do
    begin
      if ExtractFileExt(FilesToAdd[i]) = '' then
        FilesToAdd[i] := FilesToAdd[i] + '.lua';
      if not FileExists(FilesToAdd[i]) then
        Raise(Exception.Create(FilesToAdd[i] + ' not found'));
      FileName:= ExtractFileName(FilesToAdd[i]);
      if LowerCase(ExtractFileExt(FilesToAdd[i])) <> '.lua' then
      begin
        if not FQuiet then
          writeln('Add DATA file   : ' +FileName);
        RXBuilder.AddFromFile(FileName, FilesToAdd[i]);
      end
      else
      begin
        TmpCompiledFile := TmpDir+FileName;
        if not FQuiet then
          writeln('Compil LUA file : ' + FileName);
        CompilLua(FilesToAdd[i],TmpCompiledFile);
        RXBuilder.AddFromFile(FileName, TmpCompiledFile);
      end;
    end;
    if ExtractFileExt(OutFile) = '' then
      OutFile := ChangeFileExt(OutFile, '.exe');
    RXBuilder.ApplyToExe(ExeName, OutFile);
    if not FileExists(OutFile) then
      Raise(Exception.Create('Error on generate exe file : '+OutFile));
    if not FQuiet then
      writeln('Exe file is generated on ' + OutFile);
  end;

var
  StartParam, old: integer;
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
  TmpDir:=GetTmpDir;
  Compiler:=TmpDir+'luac.exe';
  SaveResourceToFile('luac',Compiler);
  RXBuilder := TResourceExeBuilder.Create;
  FFile := TStringList.Create;
  FilesToAdd := TStringList.Create;
  try
    GetFilesFromParams(StartParam);
    if OutFile = '' then
      OutFile := ChangeFileExt(FilesToAdd[0], '.exe');
    repeat
      Old := FilesToAdd.Count;
      GetFilesByRequire;
    until Old = FilesToAdd.Count;
    AddFilesIntoExe;
  finally
    DeleteDirectory(TmpDir,False);
    FilesToAdd.Free;
    RXBuilder.Free;
    FFile.Free;
  end;
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
      ErrorMsg := CheckOptions('o,q,d,h,v', ['output', 'quiet', 'debug', 'help', 'version']);
      if ErrorMsg <> '' then
        writeln(ErrorMsg)
      else if HasOption('h', 'help') then
        ShowHelp
      else if HasOption('v', 'version') then
        ShowVersion
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
      Writeln;
      Writeln('Error : '+e.message);
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
    writeln('Copyright (c) 2008-2025, Neuts JL (http://www.neuts.fr)');
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
  writeln('  -h,         --help           Display this help');
  writeln('  -v,         --version        Display version information');
  writeln;
  writeln('In LUA file, for use :');
  writeln('--#include "datafile"          Import data file in executable');
  writeln('_DATA_PATH                     Data path');
  writeln('_APP_PATH                      Executable path');
  writeln('_APP_FILE                      Executable file');
  writeln('_APP_NAME                      Executable name');
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
