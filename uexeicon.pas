{
  *****************************************************************************
   Unit        : uexeicon
   Author      : NEUTS JL
   License     : GPL (GNU General Public License)

   Description : Icon of a Windows executable (used by LuaToExe / LuaToExew
                 for the --#icon directive).
                 - SetExeIcon    : replaces all the icons of an exe by the ones
                                   of a .ico file (MAINICON and IUP_ICON).
                 - ReplaceRCData : replaces the content of an RCDATA resource
                                   of an exe by a file.
                 Windows only.

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
unit uexeicon;

{$mode objfpc}{$H+}

interface

// Replaces all the icons of ExeFile by the ones of IcoFile. The same icons are
// stored as MAINICON (icon of the file in Explorer) and IUP_ICON (default
// icon of the IUP dialogs). ExeFile must not be compressed with UPX.
procedure SetExeIcon(const ExeFile, IcoFile: string);

// Replaces the content of an RCDATA resource of an exe by a file
// (all the languages of this resource are replaced).
procedure ReplaceRCData(const ExeFile, ResName, DataFile: string);

implementation

uses
  Windows, Classes, SysUtils;

type
  TIcoHeader = packed record
    idReserved, idType, idCount: Word;
  end;
  TIcoDirEntry = packed record
    bWidth, bHeight, bColorCount, bReserved: Byte;
    wPlanes, wBitCount: Word;
    dwBytesInRes, dwImageOffset: DWord;
  end;
  TGrpIconDirEntry = packed record
    bWidth, bHeight, bColorCount, bReserved: Byte;
    wPlanes, wBitCount: Word;
    dwBytesInRes: DWord;
    nID: Word;
  end;

const
  ResTypeIcon = 3;
  ResTypeRCData = 10;
  ResTypeGroupIcon = 14;

function EnumResNamesProc(hModule: HMODULE; lpType: PChar; lpName: PChar;
  lParam: PtrInt): LongBool; stdcall;
begin
  if PtrUInt(lpName) shr 16 = 0 then
    TStrings(lParam).Add('#' + IntToStr(PtrUInt(lpName)))
  else
    TStrings(lParam).Add(StrPas(lpName));
  Result := True;
end;

function EnumResLangsProc(hModule: HMODULE; lpType: PChar; lpName: PChar;
  wLanguage: Word; lParam: PtrInt): LongBool; stdcall;
begin
  TStrings(lParam).Add(IntToStr(wLanguage));
  Result := True;
end;

// '#12' -> MAKEINTRESOURCE(12), else the name itself
function ResNamePtr(const Name: string): PChar;
begin
  if (Name <> '') and (Name[1] = '#') then
    Result := PChar(PtrUInt(StrToInt(Copy(Name, 2, MaxInt))))
  else
    Result := PChar(Name);
end;

procedure SetExeIcon(const ExeFile, IcoFile: string);
var
  Ico: TMemoryStream;
  Header: TIcoHeader;
  Entries: array of TIcoDirEntry;
  Group: TMemoryStream;
  GrpEntry: TGrpIconDirEntry;
  Existing, Names, Langs: TStringList;
  hLib: HMODULE;
  hUpd: THandle;
  ResType, i, j, k: integer;
  ok: boolean;
  Exe: TFileStream;
  Head: AnsiString;
begin
  Ico := TMemoryStream.Create;
  Group := TMemoryStream.Create;
  Existing := TStringList.Create;
  Names := TStringList.Create;
  Langs := TStringList.Create;
  try
    // Read and check the .ico file
    Ico.LoadFromFile(IcoFile);
    if Ico.Size < SizeOf(Header) then
      Raise Exception.Create(IcoFile + ' : not an icon file');
    Ico.ReadBuffer(Header, SizeOf(Header));
    if (Header.idReserved <> 0) or (Header.idType <> 1) or (Header.idCount = 0) or
       (Ico.Size < SizeOf(Header) + Header.idCount * SizeOf(TIcoDirEntry)) then
      Raise Exception.Create(IcoFile + ' : not an icon file');
    SetLength(Entries, Header.idCount);
    for i := 0 to Header.idCount - 1 do
    begin
      Ico.ReadBuffer(Entries[i], SizeOf(TIcoDirEntry));
      if (Entries[i].dwBytesInRes = 0) or
         (Int64(Entries[i].dwImageOffset) + Entries[i].dwBytesInRes > Ico.Size) then
        Raise Exception.Create(IcoFile + ' : corrupted icon file');
    end;

    // A UPX packed exe does not survive a resource update (it crashes at
    // start) : the UPX section names are in the first bytes of the file.
    Exe := TFileStream.Create(ExeFile, fmOpenRead or fmShareDenyWrite);
    try
      if Exe.Size < 4096 then
        SetLength(Head, Exe.Size)
      else
        SetLength(Head, 4096);
      if Head <> '' then
        Exe.ReadBuffer(Head[1], Length(Head));
    finally
      Exe.Free;
    end;
    if Pos('UPX0', Head) > 0 then
      Raise Exception.Create('--#icon : ' + ExtractFileName(ParamStr(0)) +
        ' is compressed with UPX, the icon cannot be set. Use an uncompressed ' +
        ExtractFileName(ParamStr(0)) + ' (upx -d ' + ExtractFileName(ParamStr(0)) + ')');

    // List the icons already in the exe ('type|name|lang'), to remove them
    hLib := LoadLibraryEx(PChar(ExeFile), 0, LOAD_LIBRARY_AS_DATAFILE);
    if hLib = 0 then
      Raise Exception.Create('Cannot read resources of ' + ExeFile);
    try
      for k := 0 to 1 do
      begin
        if k = 0 then
          ResType := ResTypeGroupIcon
        else
          ResType := ResTypeIcon;
        Names.Clear;
        EnumResourceNames(hLib, PChar(PtrUInt(ResType)), @EnumResNamesProc, PtrInt(Names));
        for i := 0 to Names.Count - 1 do
        begin
          Langs.Clear;
          EnumResourceLanguages(hLib, PChar(PtrUInt(ResType)), ResNamePtr(Names[i]),
            @EnumResLangsProc, PtrInt(Langs));
          for j := 0 to Langs.Count - 1 do
            Existing.Add(IntToStr(ResType) + '|' + Names[i] + '|' + Langs[j]);
        end;
      end;
    finally
      FreeLibrary(hLib);
    end;

    // Group directory : same as the .ico directory, offsets replaced by ids
    Group.WriteBuffer(Header, SizeOf(Header));
    for i := 0 to Header.idCount - 1 do
    begin
      Move(Entries[i], GrpEntry, 12); // all fields up to dwBytesInRes
      GrpEntry.nID := i + 1;
      Group.WriteBuffer(GrpEntry, SizeOf(GrpEntry));
    end;

    hUpd := BeginUpdateResource(PChar(ExeFile), False);
    if hUpd = 0 then
      Raise Exception.Create('Cannot update resources of ' + ExeFile);
    ok := False;
    try
      for i := 0 to Existing.Count - 1 do
      begin
        Names.Delimiter := '|';
        Names.StrictDelimiter := True;
        Names.DelimitedText := Existing[i];
        UpdateResource(hUpd, PChar(PtrUInt(StrToInt(Names[0]))), ResNamePtr(Names[1]),
          StrToInt(Names[2]), nil, 0);
      end;
      for i := 0 to Header.idCount - 1 do
        if not UpdateResource(hUpd, PChar(PtrUInt(ResTypeIcon)), PChar(PtrUInt(i + 1)), 0,
          PByte(Ico.Memory) + Entries[i].dwImageOffset, Entries[i].dwBytesInRes) then
          Raise Exception.Create('Cannot add icon in ' + ExeFile);
      // MAINICON : icon of the file (Explorer). IUP_ICON : the same icons, used
      // by IUP (iup.dll) as the default icon of its dialogs (title bar, taskbar)
      if not UpdateResource(hUpd, PChar(PtrUInt(ResTypeGroupIcon)), 'IUP_ICON', 0,
        Group.Memory, Group.Size) or
         not UpdateResource(hUpd, PChar(PtrUInt(ResTypeGroupIcon)), 'MAINICON', 0,
        Group.Memory, Group.Size) then
        Raise Exception.Create('Cannot add icon in ' + ExeFile);
      ok := True;
    finally
      if not EndUpdateResource(hUpd, not ok) and ok then
        Raise Exception.Create('Cannot write icon in ' + ExeFile);
    end;
  finally
    Ico.Free;
    Group.Free;
    Existing.Free;
    Names.Free;
    Langs.Free;
  end;
end;

procedure ReplaceRCData(const ExeFile, ResName, DataFile: string);
var
  Data: TMemoryStream;
  Langs: TStringList;
  hLib: HMODULE;
  hUpd: THandle;
  i: integer;
  ok: boolean;
begin
  Data := TMemoryStream.Create;
  Langs := TStringList.Create;
  try
    Data.LoadFromFile(DataFile);
    hLib := LoadLibraryEx(PChar(ExeFile), 0, LOAD_LIBRARY_AS_DATAFILE);
    if hLib = 0 then
      Raise Exception.Create('Cannot read resources of ' + ExeFile);
    try
      EnumResourceLanguages(hLib, PChar(PtrUInt(ResTypeRCData)), PChar(UpperCase(ResName)),
        @EnumResLangsProc, PtrInt(Langs));
    finally
      FreeLibrary(hLib);
    end;
    if Langs.Count = 0 then
      Raise Exception.Create('Resource ' + ResName + ' not found in ' + ExeFile);
    hUpd := BeginUpdateResource(PChar(ExeFile), False);
    if hUpd = 0 then
      Raise Exception.Create('Cannot update resources of ' + ExeFile);
    ok := False;
    try
      for i := 0 to Langs.Count - 1 do
        if not UpdateResource(hUpd, PChar(PtrUInt(ResTypeRCData)), PChar(UpperCase(ResName)),
          StrToInt(Langs[i]), Data.Memory, Data.Size) then
          Raise Exception.Create('Cannot update resource ' + ResName + ' of ' + ExeFile);
      ok := True;
    finally
      if not EndUpdateResource(hUpd, not ok) and ok then
        Raise Exception.Create('Cannot write resources of ' + ExeFile);
    end;
  finally
    Data.Free;
    Langs.Free;
  end;
end;

end.
