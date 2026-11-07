#if !Defined(SourceDir) || !Defined(OutputDir) || !Defined(AppVersion)
  #error Supply SourceDir, OutputDir and AppVersion
#endif
#ifndef TestBuild
  #define TestBuild "0"
#endif
[Setup]
#if TestBuild == "1"
AppId={{2537CE11-D67F-4819-A15D-E5EE38503A11}
#else
AppId={{B4985203-78C0-4A56-8621-795168DF7B26}
#endif
AppName=Road Rash 3DO Audio Port
AppVersion={#AppVersion}
AppPublisher=P0NYSLAYSTATION
AppPublisherURL=https://github.com/P0NYSLAYSTATION/road-rash
DefaultDirName={localappdata}\Programs\Road Rash 3DO Audio Port
UsePreviousAppDir=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.18362
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
#if TestBuild == "1"
OutputBaseFilename=RoadRash-3DO-Setup-{#AppVersion}-test
#else
OutputBaseFilename=RoadRash-3DO-Setup-{#AppVersion}
#endif
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\rr-game.exe
CloseApplications=yes

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

#if TestBuild != "1"
[Icons]
Name: "{autoprograms}\Road Rash 3DO Audio Port"; Filename: "{app}\rr-game.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Road Rash 3DO Audio Port"; Filename: "{app}\rr-game.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\rr-game.exe"; Description: "Launch Road Rash"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
#endif

[Code]
var
  PCPage, AudioPage: TInputQueryWizardPage;
  ModePage: TInputOptionWizardPage;
  ImportComplete: Boolean;

function ConfigValue(const Key, Default: String): String;
var Lines: TArrayOfString; I: Integer; Prefix: String;
begin
  Result := Default; Prefix := Key + '=';
  if LoadStringsFromFile(AddBackslash(WizardDirValue)+'rr-port.ini', Lines) then
    for I := 0 to GetArrayLength(Lines)-1 do
      if Pos(Prefix, Lines[I]) = 1 then Result := Copy(Lines[I], Length(Prefix)+1, MaxInt);
end;

function AudioRoot: String;
begin
  Result := ConfigValue('3do_audio', '3do_audio');
  if not ((Pos(':', Result) > 0) or (Copy(Result,1,2) = '\\')) then
    Result := AddBackslash(ExpandConstant('{app}')) + Result;
end;

function BankReady(const Folder: String): Boolean;
var I: Integer; Name: String;
begin
  Result := False; if Folder = '' then Exit;
  for I := 1 to 19 do begin
    if I < 10 then Name := 'resource0' + IntToStr(I) + '.wav'
    else Name := 'resource' + IntToStr(I) + '.wav';
    if not FileExists(AddBackslash(Folder)+Name) then Exit;
  end;
  Result := True;
end;

function GameRoot(const Source: String): String;
begin
  Result := Source;
  if FileExists(AddBackslash(Source)+'ROADRASH\ROADRASH.EXE') then
    Result := AddBackslash(Source)+'ROADRASH';
end;

function ValidGame(const Source: String): Boolean;
var Root: String;
begin
  Root := GameRoot(Source);
  Result := FileExists(AddBackslash(Root)+'ROADRASH.EXE') and
    FileExists(AddBackslash(Root)+'DATA\BIKESPEC.RSC');
end;

procedure BrowsePCFolder(Sender: TObject);
var Folder: String;
begin
  Folder := PCPage.Values[0];
  if BrowseForFolder('Select a PC installation or original CD drive', Folder, False) then
    PCPage.Values[0] := Folder;
end;

procedure BrowsePCImage(Sender: TObject);
var FileName: String;
begin
  FileName := PCPage.Values[0];
  if GetOpenFileName('Select the original PC installation CD image', FileName, '',
      'CD image (*.iso)|*.iso|All files (*.*)|*.*', 'iso') then PCPage.Values[0] := FileName;
end;

procedure BrowseAudio(Sender: TObject);
var FileName: String;
begin
  FileName := AudioPage.Values[0];
  if GetOpenFileName('Select the 3DO disc archive containing BIN/CUE', FileName, '',
      'Disc archives (*.zip;*.7z;*.rar)|*.zip;*.7z;*.rar|All files (*.*)|*.*', 'zip') then
    AudioPage.Values[0] := FileName;
end;

procedure AddBrowse(Page: TInputQueryWizardPage; Caption: String; Index: Integer; Handler: TNotifyEvent);
var Button: TNewButton;
begin
  Button := TNewButton.Create(WizardForm); Button.Parent := Page.Surface;
  Button.Caption := Caption; Button.Width := ScaleX(110); Button.Height := ScaleY(25);
  Button.Left := Page.SurfaceWidth - Button.Width;
  Button.Top := Page.Edits[0].Top + Index * ScaleY(33);
  Button.OnClick := Handler;
  Page.Edits[0].Width := Button.Left - ScaleX(8);
end;

procedure InitializeWizard;
begin
  PCPage := CreateInputQueryPage(wpSelectDir, 'Original PC game files',
    'Select an installation folder, original CD drive, or CD image',
    'Setup imports the game assets without running the old installer. Existing installed assets can be reused when updating.');
  PCPage.Add('PC installation, CD folder/drive, or ISO file:', False);
  PCPage.Values[0] := ExpandConstant('{param:PCASSETS|}');
  if (PCPage.Values[0] = '') and ValidGame(AddBackslash(WizardDirValue)+'game') then
    PCPage.Values[0] := AddBackslash(WizardDirValue)+'game';
  AddBrowse(PCPage, 'Folder / CD...', 0, @BrowsePCFolder);
  AddBrowse(PCPage, 'CD image...', 1, @BrowsePCImage);
  AudioPage := CreateInputQueryPage(PCPage.ID, 'Optional 3DO disc',
    'Select an archive containing your 3DO BIN/CUE image',
    'Setup extracts and converts the nonlicensed audio automatically, offline. ZIP, 7z and RAR archives are supported. Leave blank for PC audio, or to keep existing 3DO audio during an update.');
  AudioPage.Add('3DO disc archive (optional):', False);
  AudioPage.Values[0] := ExpandConstant('{param:THREEDOARCHIVE|}');
  AddBrowse(AudioPage, 'Archive...', 0, @BrowseAudio);
  ModePage := CreateInputOptionPage(AudioPage.ID, 'Audio mode', 'Choose game audio processing',
    '3DO mode uses the extracted sample bank. If no 3DO disc archive or existing sample bank is available, PC audio is selected automatically.', True, False);
  ModePage.Add('3DO audio when available'); ModePage.Add('Original PC audio');
  if ExpandConstant('{param:AUDIOMODE|}') = 'pc' then ModePage.SelectedValueIndex := 1
  else if ExpandConstant('{param:AUDIOMODE|}') = '3do' then ModePage.SelectedValueIndex := 0
  else if ValidGame(AddBackslash(WizardDirValue)+'game') and (ConfigValue('audio','pc') = 'pc') then
    ModePage.SelectedValueIndex := 1
  else ModePage.SelectedValueIndex := 0;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if (PCPage.Values[0] = '') and ValidGame(ExpandConstant('{app}\game')) then
    PCPage.Values[0] := ExpandConstant('{app}\game');
  if not (ValidGame(PCPage.Values[0]) or FileExists(PCPage.Values[0])) then
    Result := 'Select the original PC installation, CD drive or ISO. Silent setup accepts /PCASSETS="path"; an existing installation can be updated without supplying assets again.';
  if (AudioPage.Values[0] <> '') and not FileExists(AudioPage.Values[0]) then
    Result := 'The selected 3DO archive does not exist.';
end;

function SetFileAttributesW(FileName: String; Attributes: LongWord): Boolean;
  external 'SetFileAttributesW@kernel32.dll stdcall';

function CopyAsset(const Source, Dest: String): Boolean;
begin
  { CD files may carry ReadOnly into the imported installation. }
  if FileExists(Dest) then SetFileAttributesW(Dest,128);
  Result := CopyFile(Source,Dest,False);
  if Result then SetFileAttributesW(Dest,128);
end;

procedure CopyTree(const Source, Dest: String);
var F: TFindRec; FromName, ToName: String;
begin
  if not DirExists(Source) then Exit;
  if CompareText(ExpandFileName(Source), ExpandFileName(Dest)) = 0 then Exit;
  ForceDirectories(Dest);
  if FindFirst(AddBackslash(Source)+'*', F) then begin
    try repeat
      if (F.Name <> '.') and (F.Name <> '..') then begin
        FromName := AddBackslash(Source)+F.Name; ToName := AddBackslash(Dest)+F.Name;
        if (F.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then CopyTree(FromName, ToName)
        else if not CopyAsset(FromName, ToName) then RaiseException('Unable to copy '+FromName);
      end;
    until not FindNext(F); finally FindClose(F); end;
  end;
end;

procedure CopyUserFiles(const Source, Dest: String; Saves: Boolean);
var F: TFindRec; Ext: String;
begin
  ForceDirectories(Dest);
  if FindFirst(AddBackslash(Source)+'*', F) then begin
    try repeat
      if (F.Attributes and FILE_ATTRIBUTE_DIRECTORY) = 0 then begin
        Ext := Lowercase(ExtractFileExt(F.Name));
        if (Saves and (Ext = '.rrs')) or ((not Saves) and
            ((Ext = '.sf2') or (Ext = '.sf3') or (Ext = '.dls'))) then
          if not FileExists(AddBackslash(Dest)+F.Name) then
            if not CopyFile(AddBackslash(Source)+F.Name, AddBackslash(Dest)+F.Name, False) then
              RaiseException('Unable to import '+F.Name);
      end;
    until not FindNext(F); finally FindClose(F); end;
  end;
end;

procedure SetConfig(const Key, Value: String);
var Lines: TArrayOfString; I: Integer; Found: Boolean;
begin
  LoadStringsFromFile(AddBackslash(WizardDirValue)+'rr-port.ini', Lines); Found := False;
  for I := 0 to GetArrayLength(Lines)-1 do if Pos(Key+'=',Lines[I]) = 1 then begin
    Lines[I] := Key+'='+Value; Found := True;
  end;
  if not Found then begin I := GetArrayLength(Lines); SetArrayLength(Lines,I+1); Lines[I] := Key+'='+Value; end;
  if not SaveStringsToFile(AddBackslash(WizardDirValue)+'rr-port.ini',Lines,False) then RaiseException('Could not update configuration.');
end;

procedure ImportPC(const Source, Dest: String);
var Root, Stage: String; Code: Integer;
begin
  Root := GameRoot(Source);
  if FileExists(Source) then begin
    Stage := ExpandConstant('{app}\.pc-import-')+GetDateTimeString('yyyymmddhhnnss', '-', '-');
    if not Exec(ExpandConstant('{app}\support\rr-import.exe'), '"'+Source+'" "'+Stage+'"', '', SW_HIDE, ewWaitUntilTerminated, Code) or (Code <> 0) then
      RaiseException('Could not import the PC CD image. Supply an original compatible Road Rash PC disc or installation.');
    Root := Stage;
  end;
  if CompareText(ExpandFileName(Root),ExpandFileName(Dest)) <> 0 then begin
    ForceDirectories(Dest);
    if not CopyAsset(AddBackslash(Root)+'ROADRASH.EXE',AddBackslash(Dest)+'ROADRASH.EXE') then RaiseException('Could not import PC executable resources.');
    CopyTree(AddBackslash(Root)+'DATA',AddBackslash(Dest)+'DATA');
    CopyTree(AddBackslash(Root)+'IMAGES',AddBackslash(Dest)+'IMAGES');
    CopyTree(AddBackslash(Root)+'TEXT',AddBackslash(Dest)+'TEXT');
    CopyTree(AddBackslash(Root)+'AUDIO',AddBackslash(Dest)+'AUDIO');
    CopyTree(AddBackslash(Root)+'VIDEO',AddBackslash(Dest)+'VIDEO');
    CopyTree(AddBackslash(Root)+'soundfonts',AddBackslash(Dest)+'soundfonts');
    CopyUserFiles(Root,ExpandConstant('{app}\saves'),True);
    CopyUserFiles(Root,AddBackslash(Dest)+'soundfonts',False);
  end;
  if Stage <> '' then begin
    if CompareText(ExtractFileDir(ExpandFileName(Stage)),ExpandFileName(ExpandConstant('{app}'))) <> 0 then RaiseException('Invalid import staging path');
    DelTree(Stage,True,True,True);
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var Mode, Archive, Audio, Args: String; Code: Integer;
begin
  if CurStep <> ssPostInstall then Exit;
  WizardForm.StatusLabel.Caption := 'Importing PC game assets...';
  ImportPC(PCPage.Values[0],ExpandConstant('{app}\game'));
  ForceDirectories(ExpandConstant('{app}\saves'));
  Archive := AudioPage.Values[0]; Audio := AudioRoot;
  if Archive <> '' then begin
    WizardForm.StatusLabel.Caption := 'Extracting and converting 3DO audio...';
    Args := '"'+Archive+'" "'+ExpandConstant('{app}\3do_audio')+'" --ffmpeg "'+ExpandConstant('{app}\support\ffmpeg.exe')+'" --archiver "'+ExpandConstant('{app}\support\7z.exe')+'" --log "'+ExpandConstant('{app}\3do-import.log')+'"';
    if not Exec(ExpandConstant('{app}\support\rr-extract.exe'),Args,'',SW_HIDE,ewWaitUntilTerminated,Code) or (Code <> 0) then
      RaiseException('3DO extraction failed. Existing audio was preserved. See 3do-import.log in the installation directory.');
    Audio := ExpandConstant('{app}\3do_audio'); SetConfig('3do_audio','3do_audio');
  end;
  Mode := 'pc';
  if (ModePage.SelectedValueIndex = 0) and BankReady(Audio) then Mode := '3do';
  SetConfig('audio',Mode);
  if ConfigValue('3do_audio','') = '' then SetConfig('3do_audio','3do_audio');
  if ConfigValue('renderer','') = '' then SetConfig('renderer','dx9');
  ImportComplete := True;
end;

function GetCustomSetupExitCode: Integer;
begin
  Result := 0;
  if not ImportComplete then Result := 1;
end;
