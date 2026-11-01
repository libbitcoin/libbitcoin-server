; Inno Setup script for libbitcoin-server, defined on the iscc command line:
; AppVersion, Architecture (x64os|arm64), SourceDir, OutputDir, OutputBase,
; Selector, Baseline, BaselineRequires, Optimized, OptimizedRequires.

[Setup]
AppId={{6F1C0B7E-3D2A-4C5B-9E8F-1A2B3C4D5E6F}
AppName=Libbitcoin Server
AppVersion={#AppVersion}
AppPublisher=libbitcoin
AppPublisherURL=https://github.com/libbitcoin/libbitcoin-server
DefaultDirName={autopf}\Libbitcoin Server
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed={#Architecture}
ArchitecturesInstallIn64BitMode={#Architecture}
ChangesEnvironment=yes
OutputDir={#OutputDir}
OutputBaseFilename={#OutputBase}
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=Libbitcoin Server

[Tasks]
Name: "modifypath"; Description: "Add the installation directory to the system PATH"

[Files]
Source: "{#SourceDir}\{#Selector}"; Flags: dontcopy
Source: "{#SourceDir}\{#Baseline}"; Flags: dontcopy
Source: "{#SourceDir}\{#Optimized}"; Flags: dontcopy

[Registry]
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; \
    ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; \
    Tasks: modifypath; Check: NotInPath(ExpandConstant('{app}'))

[UninstallDelete]
Type: files; Name: "{app}\bs.exe"
Type: dirifempty; Name: "{app}"

[Code]
const
  EnvironmentKey = 'SYSTEM\CurrentControlSet\Control\Session Manager\Environment';

var
  Variant: String;

function Supports(const Features: TArrayOfString; const Required: String): Boolean;
var
  Remaining, Token: String;
  Index, Position: Integer;
  Found: Boolean;
begin
  Result := True;
  Remaining := Trim(Required);
  while Remaining <> '' do
  begin
    Position := Pos(' ', Remaining);
    if Position = 0 then
    begin
      Token := Remaining;
      Remaining := '';
    end
    else
    begin
      Token := Copy(Remaining, 1, Position - 1);
      Remaining := Trim(Copy(Remaining, Position + 1, MaxInt));
    end;

    Found := False;
    for Index := 0 to GetArrayLength(Features) - 1 do
      if Trim(Features[Index]) = Token then
        Found := True;

    if not Found then
    begin
      Result := False;
      Exit;
    end;
  end;
end;

function InitializeSetup(): Boolean;
var
  Selector, Output: String;
  Features: TArrayOfString;
  Code: Integer;
begin
  Result := False;
  ExtractTemporaryFile('{#Selector}');
  Selector := ExpandConstant('{tmp}\{#Selector}');
  Output := ExpandConstant('{tmp}\features.txt');

  if not Exec(ExpandConstant('{cmd}'), '/C ""' + Selector + '" > "' + Output + '""',
    '', SW_HIDE, ewWaitUntilTerminated, Code) or (Code <> 0) or
    not LoadStringsFromFile(Output, Features) then
  begin
    SuppressibleMsgBox('Unable to determine processor features.', mbCriticalError, MB_OK, IDOK);
    Exit;
  end;

  if Supports(Features, '{#OptimizedRequires}') then
    Variant := '{#Optimized}'
  else if Supports(Features, '{#BaselineRequires}') then
    Variant := '{#Baseline}'
  else
  begin
    SuppressibleMsgBox('This processor supports no packaged variant of bs.', mbCriticalError, MB_OK, IDOK);
    Exit;
  end;

  Log('Selected variant: ' + Variant);
  Result := True;
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    ExtractTemporaryFile(Variant);
    ForceDirectories(ExpandConstant('{app}'));
    if not FileCopy(ExpandConstant('{tmp}\') + Variant, ExpandConstant('{app}\bs.exe'), False) then
      RaiseException('Unable to install ' + Variant + '.');
  end;
end;

function NotInPath(const Directory: String): Boolean;
var
  Path: String;
begin
  if not RegQueryStringValue(HKLM, EnvironmentKey, 'Path', Path) then
    Result := True
  else
    Result := Pos(';' + Uppercase(Directory) + ';', ';' + Uppercase(Path) + ';') = 0;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Path, Directory: String;
  Position: Integer;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    Directory := ExpandConstant('{app}');
    if RegQueryStringValue(HKLM, EnvironmentKey, 'Path', Path) then
    begin
      Path := ';' + Path + ';';
      Position := Pos(';' + Uppercase(Directory) + ';', Uppercase(Path));
      if Position > 0 then
      begin
        Delete(Path, Position, Length(Directory) + 1);
        RegWriteExpandStringValue(HKLM, EnvironmentKey, 'Path', Copy(Path, 2, Length(Path) - 2));
      end;
    end;
  end;
end;
