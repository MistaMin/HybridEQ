# Builds the HybridEQ Windows bundle archive for the OpenGrid Plugin Manager (replaces the WiX MSI).
# Expects a Release build in .\build3. Run from the installer/windows directory.
# The MSI definition (HybridEQ.wxs) is kept for reference but is no longer part of the release.
param(
    [string]$BuildDir = "..\..\build3",
    [string]$SourceDir = "..\..",
    [string]$OutDir = "..\..\dist"
)
$ErrorActionPreference = "Stop"

$SourceDir = Resolve-Path $SourceDir | Select-Object -ExpandProperty Path
$Version = Get-Content "$SourceDir\VERSION" -Raw | ForEach-Object { $_.Trim() }
$Artefacts = Join-Path (Resolve-Path $BuildDir) "HybridEQ_artefacts\Release"
& "$SourceDir\tools\make_bundle_zip.ps1" -Plugin "HybridEQ" -Version $Version -Artefacts $Artefacts -Out $OutDir -MinOs "10.0"
