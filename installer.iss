[Setup]
AppID=WinIPBroadcast
AppName=WinIPBroadcast
AppVerName=WinIPBroadcast 1.3
AppVersion=1.3
AppPublisher=Etienne Dechamps
AppPublisherURL=https://github.com/dechamps/WinIPBroadcast
AppSupportURL=https://github.com/dechamps/WinIPBroadcast
AppUpdatesURL=https://github.com/dechamps/WinIPBroadcast
AppContact=etienne@edechamps.fr

OutputDir=.
OutputBaseFilename=WinIPBroadcast-1.3

DefaultDirName={pf}\WinIPBroadcast
AppendDefaultDirName=no

LicenseFile=LICENSE.txt


[Files]
Source:"Release\WinIPBroadcast.exe"; DestDir: "{app}"; Flags: ignoreversion; BeforeInstall: StopService
Source:"Release\msvcr120.dll"; DestDir: "{app}"; Flags: ignoreversion
Source:"WinIPBroadcast.c"; DestDir:"{app}\src"; Flags: ignoreversion
Source:"WinIPBroadcast.sln"; DestDir:"{app}\src"; Flags: ignoreversion
Source:"WinIPBroadcast.vcxproj"; DestDir:"{app}\src"; Flags: ignoreversion
Source:"LICENSE.txt"; DestDir:"{app}"; Flags: ignoreversion 
Source:"README.md"; DestDir:"{app}"; DestName:"README.txt"; Flags: ignoreversion isreadme

[Registry]
Root: HKLM; Subkey: "SOFTWARE\WinIPBroadcast"; Flags: uninsdeletekey
Root: HKLM; Subkey: "SOFTWARE\WinIPBroadcast"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"; Flags: deletevalue
Root: HKLM; Subkey: "SOFTWARE\WinIPBroadcast"; ValueType: string; ValueName: "Version"; ValueData: "1.3"; Flags: deletevalue

[Run]
Filename: "{app}\WinIPBroadcast.exe"; Parameters: "install"; StatusMsg: "Installing service..."; Flags: runhidden
Filename: "{sys}\net.exe"; Parameters: "start WinIPBroadcast"; StatusMsg: "Starting service..."; Flags: runhidden

[UninstallRun]
Filename: "{sys}\net.exe"; Parameters: "stop WinIPBroadcast"; Flags: runhidden; RunOnceId: "StopService"
Filename: "{app}\WinIPBroadcast.exe"; Parameters: "remove"; Flags: runhidden; RunOnceId: "RemoveService"

[code]
procedure StopService();
var
	ResultCode : Integer;
begin
	Exec(ExpandConstant('{sys}') + '\net.exe', 'stop WinIPBroadcast', '', SW_HIDE, ewWaitUntilTerminated, ResultCode);
end;
