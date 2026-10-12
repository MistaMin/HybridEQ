# Build HybridEQ Windows MSI using WiX v4.
# Expects a Release build in .\build3 (configure with build_win.bat then build).
# Run from the installer/windows directory.
param(
    [string]$BuildDir = "..\..\build3",
    [string]$SourceDir = "..\..",
    [string]$OutDir = "build"
)
$ErrorActionPreference = "Stop"

$SourceDir = Resolve-Path $SourceDir | Select-Object -ExpandProperty Path
$BuildDir = Resolve-Path $BuildDir | Select-Object -ExpandProperty Path
$OutPath = New-Item -ItemType Directory -Force -Path $OutDir | Select-Object -ExpandProperty FullName

$Version = Get-Content "$SourceDir\VERSION" -Raw | ForEach-Object { $_.Trim() }
$Msi = "$OutPath\HybridEQ-$Version-x64.msi"

wix build HybridEQ.wxs -define "SourceDir=$SourceDir\" -define "BuildDir=$BuildDir\" -define "Version=$Version" -o "$Msi"
Write-Host "MSI: $Msi"
