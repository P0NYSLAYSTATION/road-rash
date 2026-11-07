#if !Defined(SourceDir) || !Defined(OutputDir) || !Defined(AppVersion)
  #error Supply SourceDir, OutputDir and AppVersion
#endif
[Setup]
AppId={{B4985203-78C0-4A56-8621-795168DF7B26}
AppName=Road Rash 3DO Audio Port
AppVersion={#AppVersion}
AppPublisher=P0NYSLAYSTATION
AppPublisherURL=https://github.com/P0NYSLAYSTATION/road-rash
DefaultDirName={localappdata}\Programs\Road Rash 3DO Audio Port
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.18362
DisableProgramGroupPage=yes
OutputDir={#OutputDir}
OutputBaseFilename=RoadRash-3DO-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayIcon={app}\rr-game.exe
CloseApplications=yes

[Tasks]
Name: desktopicon; Description: "Create a desktop shortcut"; Flags: unchecked

[Files]
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\Road Rash 3DO Audio Port"; Filename: "{app}\rr-game.exe"; WorkingDir: "{app}"
Name: "{autodesktop}\Road Rash 3DO Audio Port"; Filename: "{app}\rr-game.exe"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\rr-game.exe"; Description: "Launch Road Rash"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent

[Code]
var
  PCPage: TInputDirWizardPage;
  AudioPage: TInputQueryWizardPage;
  AudioBrowse: TNewButton;
  ModePage: TInputOptionWizardPage;

function BankReady(const Folder: String): Boolean;
var I: Integer; Name: String;
begin
  Result := False;
  if Folder = '' then Exit;
  for I := 1 to 19 do begin
    if I < 10 then Name := 'resource0' + IntToStr(I) + '.wav'
    else Name := 'resource' + IntToStr(I) + '.wav';
    if not FileExists(AddBackslash(Folder) + Name) then Exit;
  end;
  Result := True;
end;

procedure BrowseAudio(Sender: TObject);
var Folder: String;
begin
  Folder := AudioPage.Values[0];
  if BrowseForFolder('Select extracted 3DO WAV folder', Folder, False) then
    AudioPage.Values[0] := Folder;
end;

procedure InitializeWizard;
begin
  PCPage := CreateInputDirPage(wpSelectDir, 'Original PC game files',
    'Select your existing Road Rash PC installation',
    'The game assets are copied into this installation. No game assets are included in this installer.', False, '');
  PCPage.Add('PC installation folder:');
  PCPage.Values[0] := ExpandConstant('{param:PCASSETS|}');
  AudioPage := CreateInputQueryPage(PCPage.ID, 'Optional 3DO audio',
    'Select the folder containing extracted 3DO WAV files',
    'Select the output folder from extract_3do_audio.py (resource01.wav through resource19.wav). Leave it blank to use PC audio.');
  AudioPage.Add('3DO audio folder (optional):', False);
  AudioBrowse := TNewButton.Create(WizardForm);
  AudioBrowse.Parent := AudioPage.Surface;
  AudioBrowse.Caption := 'Browse...';
  AudioBrowse.Width := ScaleX(80);
  AudioBrowse.Height := AudioPage.Edits[0].Height + ScaleY(4);
  AudioBrowse.Left := AudioPage.SurfaceWidth - AudioBrowse.Width;
  AudioBrowse.Top := AudioPage.Edits[0].Top - ScaleY(2);
  AudioPage.Edits[0].Width := AudioBrowse.Left - ScaleX(8);
  AudioBrowse.OnClick := @BrowseAudio;
  AudioPage.Values[0] := ExpandConstant('{param:THREEDOAUDIO|}');
  ModePage := CreateInputOptionPage(AudioPage.ID, 'Audio mode', 'Choose the game audio processing',
    'The 3DO mode uses native sample rates, loop points, linear spatial mixing and startup engine modulation. If its files are unavailable, PC audio is used automatically.', True, False);
  ModePage.Add('3DO audio when available'); ModePage.Add('Original PC audio');
  if ExpandConstant('{param:AUDIOMODE|3do}') = 'pc' then ModePage.SelectedValueIndex := 1
  else ModePage.SelectedValueIndex := 0;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if CurPageID = PCPage.ID then begin
    Result := FileExists(AddBackslash(PCPage.Values[0]) + 'ROADRASH.EXE') and
      FileExists(AddBackslash(PCPage.Values[0]) + 'DATA\BIKESPEC.RSC');
    if not Result then MsgBox('Select the PC installation containing ROADRASH.EXE and DATA\BIKESPEC.RSC.', mbError, MB_OK);
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  if not (FileExists(AddBackslash(PCPage.Values[0]) + 'ROADRASH.EXE') and
      FileExists(AddBackslash(PCPage.Values[0]) + 'DATA\BIKESPEC.RSC')) then
    Result := 'A valid PC installation is required. For silent setup, supply /PCASSETS="path".';
end;

procedure CopyTree(const Source, Dest: String; AudioOnly: Boolean);
var F: TFindRec; FromName, ToName, Ext: String;
begin
  if not DirExists(Source) then Exit;
  ForceDirectories(Dest);
  if FindFirst(AddBackslash(Source) + '*', F) then begin
    try
      repeat
        if (F.Name <> '.') and (F.Name <> '..') then begin
          FromName := AddBackslash(Source) + F.Name; ToName := AddBackslash(Dest) + F.Name;
          if (F.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then begin
            if not (AudioOnly and ((Lowercase(F.Name) = 'native') or (Lowercase(F.Name) = 'aiff'))) then
              CopyTree(FromName, ToName, AudioOnly);
          end else begin
            Ext := Lowercase(ExtractFileExt(F.Name));
            if (not AudioOnly) or (Ext = '.wav') or (Ext = '.json') then
              if not CopyFile(FromName, ToName, False) then RaiseException('Unable to copy ' + FromName);
          end;
        end;
      until not FindNext(F);
    finally FindClose(F); end;
  end;
end;

procedure CopyUserFiles(const Source, Dest: String; Saves: Boolean);
var F: TFindRec; Ext: String;
begin
  ForceDirectories(Dest);
  if FindFirst(AddBackslash(Source) + '*', F) then begin
    try
      repeat
        if (F.Attributes and FILE_ATTRIBUTE_DIRECTORY) = 0 then begin
          Ext := Lowercase(ExtractFileExt(F.Name));
          if (Saves and (Ext = '.rrs')) or ((not Saves) and
              ((Ext = '.sf2') or (Ext = '.sf3') or (Ext = '.dls'))) then
            if not FileExists(AddBackslash(Dest) + F.Name) then
              if not CopyFile(AddBackslash(Source) + F.Name, AddBackslash(Dest) + F.Name, False) then
                RaiseException('Unable to import ' + F.Name);
        end;
      until not FindNext(F);
    finally FindClose(F); end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var PC, Game, Mode, Config, Audio, Source: String;
begin
  if CurStep <> ssPostInstall then Exit;
  PC := PCPage.Values[0]; Game := ExpandConstant('{app}\game');
  ForceDirectories(Game);
  if not CopyFile(AddBackslash(PC)+'ROADRASH.EXE', AddBackslash(Game)+'ROADRASH.EXE', False) then
    RaiseException('Could not copy the original executable resource data.');
  CopyTree(AddBackslash(PC)+'DATA', AddBackslash(Game)+'DATA', False);
  CopyTree(AddBackslash(PC)+'IMAGES', AddBackslash(Game)+'IMAGES', False);
  CopyTree(AddBackslash(PC)+'TEXT', AddBackslash(Game)+'TEXT', False);
  CopyTree(AddBackslash(PC)+'AUDIO', AddBackslash(Game)+'AUDIO', False);
  Source := AddBackslash(PC)+'VIDEO';
  if not DirExists(Source) then Source := AddBackslash(PC)+'VIDEO BAK';
  CopyTree(Source, AddBackslash(Game)+'VIDEO', False);
  CopyTree(AddBackslash(PC)+'soundfonts', AddBackslash(Game)+'soundfonts', False);
  CopyFile(AddBackslash(PC)+'RASHICON.DLL',AddBackslash(Game)+'RASHICON.DLL',False);
  CopyFile(AddBackslash(PC)+'RASHPROP.DLL',AddBackslash(Game)+'RASHPROP.DLL',False);
  ForceDirectories(ExpandConstant('{app}\saves'));
  CopyUserFiles(PC, ExpandConstant('{app}\saves'), True);
  CopyUserFiles(PC, AddBackslash(Game)+'soundfonts', False);
  Audio := AudioPage.Values[0]; Mode := 'pc';
  if (ModePage.SelectedValueIndex = 0) and BankReady(Audio) then begin
    CopyTree(Audio, ExpandConstant('{app}\3do_audio'), True); Mode := '3do';
  end;
  Config := 'audio=' + Mode + #13#10 + '3do_audio=3do_audio' + #13#10 + 'renderer=dx9' + #13#10;
  if not SaveStringToFile(ExpandConstant('{app}\rr-port.ini'), Config, False) then
    RaiseException('Could not write the audio configuration.');
end;
