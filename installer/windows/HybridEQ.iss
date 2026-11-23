; HybridEQ installer for Windows (Inno Setup 6). Built by .github/workflows/windows-linux.yml:
;   iscc /DAppVersion=1.7.0 /DArch=x64 /DDist=<folder with the plug-ins> /DRoot=<repository> installer\windows\HybridEQ.iss
; Arch is x64 or arm64; the plug-ins in Dist must be built for it.

#ifndef AppVersion
  #error AppVersion is required
#endif
#ifndef Arch
  #error Arch is required (x64 or arm64)
#endif

[Setup]
AppId={{8C5E2A51-3B7D-4E0F-9A61-5D2C7B9E4F13}
AppName=HybridEQ
AppVersion={#AppVersion}
AppVerName=HybridEQ {#AppVersion}
AppPublisher=OpenGrid
AppPublisherURL=https://opengrid.studio
AppSupportURL=https://github.com/MistaMin/HybridEQ
DefaultDirName={commoncf64}\OpenGrid\HybridEQ
DisableDirPage=yes
DisableProgramGroupPage=yes
LicenseFile={#Root}\BINARY_LICENSE.txt
OutputDir={#Dist}\..
OutputBaseFilename=HybridEQ-{#AppVersion}-windows-{#Arch}-setup
MinVersion=10.0
#if Arch == "arm64"
ArchitecturesAllowed=arm64
ArchitecturesInstallIn64BitMode=arm64
#else
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#endif
PrivilegesRequired=admin
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=HybridEQ {#AppVersion}
VersionInfoVersion={#AppVersion}
VersionInfoCompany=OpenGrid
VersionInfoProductName=HybridEQ

[Types]
Name: "full"; Description: "All formats"
Name: "custom"; Description: "Choose formats"; Flags: iscustom

[Components]
Name: "vst3"; Description: "VST3 (Common Files\VST3)"; Types: full custom
Name: "clap"; Description: "CLAP (Common Files\CLAP)"; Types: full custom
Name: "lv2"; Description: "LV2 (Common Files\LV2)"; Types: full custom

[Files]
Source: "{#Dist}\HybridEQ.vst3\*"; DestDir: "{commoncf64}\VST3\HybridEQ.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Dist}\HybridEQ.clap"; DestDir: "{commoncf64}\CLAP"; Components: clap; Flags: ignoreversion
Source: "{#Dist}\HybridEQ.lv2\*"; DestDir: "{commoncf64}\LV2\HybridEQ.lv2"; Components: lv2; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#Root}\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "{#Root}\BINARY_LICENSE.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Root}\THIRD_PARTY_NOTICES.txt"; DestDir: "{app}"; Flags: ignoreversion
