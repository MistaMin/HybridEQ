# Verify the real release ZIP and the plugin manager's Windows copy semantics in a temporary root.
param(
    [Parameter(Mandatory)][string]$Archive,
    [string]$Plugin = "HybridEQ"
)
$ErrorActionPreference = "Stop"
if (-not [Environment]::Is64BitProcess) { throw "Run the x64 bundle check in 64-bit PowerShell" }
$Archive = (Resolve-Path -LiteralPath $Archive).Path
$metadata = Get-Content -Raw -LiteralPath "$Archive.installs.json" | ConvertFrom-Json
if ($metadata.os -ne "windows" -or $metadata.arch -ne "x64") { throw "Wrong platform/architecture in installs sidecar" }
$expected = @{
    vst3 = "Common Files/VST3/$Plugin.vst3"
    clap = "Common Files/CLAP/$Plugin.clap"
    lv2 = "Common Files/LV2/$Plugin.lv2"
    aax = "Common Files/Avid/Audio/Plug-Ins/$Plugin.aaxplugin"
}
foreach ($format in @("vst3", "clap", "lv2")) {
    $entries = @($metadata.installs | Where-Object { $_.format -eq $format })
    if ($entries.Count -ne 1 -or $entries[0].path -cne $expected[$format]) { throw "Incorrect $format destination" }
}
foreach ($entry in $metadata.installs) {
    if (-not $expected.ContainsKey($entry.format) -or $entry.path -cne $expected[$entry.format]) { throw "Unexpected install path: $($entry.path)" }
}
$work = Join-Path ([System.IO.Path]::GetTempPath()) ("HybridEQ user's ünicode install layout " + [guid]::NewGuid().ToString("N"))
$stage = Join-Path $work "staged archive"
$root = Join-Path $work "Program Files"
try {
    Expand-Archive -LiteralPath $Archive -DestinationPath $stage
    $tops = @(Get-ChildItem -LiteralPath $stage)
    foreach ($top in $tops) {
        if (-not $top.PSIsContainer -or $top.Name -notin @("Common Files", "OpenGrid")) { throw "Unexpected ZIP root: $($top.Name)" }
    }
    $common = Join-Path $root "Common Files"
    New-Item -ItemType Directory -Force -Path (Join-Path $common "VST3") | Out-Null
    $unrelated = Join-Path $common "VST3/Unrelated.vst3"
    Set-Content -LiteralPath $unrelated -Value "leave this plug-in untouched"
    foreach ($entry in $metadata.installs) {
        $source = Join-Path $stage $entry.path
        $dest = Join-Path $root $entry.path
        if (-not (Test-Path -LiteralPath $source)) { throw "ZIP missing $($entry.path)" }
        $nativeBinaries = 0
        $files = if ((Get-Item -LiteralPath $source).PSIsContainer) { @(Get-ChildItem -LiteralPath $source -Recurse -File) } else { @(Get-Item -LiteralPath $source) }
        foreach ($file in $files) {
            $bytes = [System.IO.File]::ReadAllBytes($file.FullName)
            if ($bytes.Length -gt 64 -and $bytes[0] -eq 0x4d -and $bytes[1] -eq 0x5a) {
                $pe = [BitConverter]::ToInt32($bytes, 0x3c)
                if ($pe -lt 0 -or $pe + 6 -gt $bytes.Length -or [BitConverter]::ToUInt32($bytes, $pe) -ne 0x4550) { throw "Invalid PE module" }
                if ([BitConverter]::ToUInt16($bytes, $pe + 4) -ne 0x8664) { throw "Non-x64 module in x64 archive: $($file.Name)" }
                $nativeBinaries++
            }
        }
        if ($nativeBinaries -eq 0) { throw "No native executable in $($entry.format) module" }
        New-Item -ItemType Directory -Force -Path (Split-Path $dest -Parent) | Out-Null
        if ((Get-Item -LiteralPath $source).PSIsContainer) {
            New-Item -ItemType Directory -Force -Path $dest | Out-Null
            Set-Content -LiteralPath (Join-Path $dest "stale-old-version.txt") -Value "old"
        } else { Set-Content -LiteralPath $dest -Value "old module" }
    }
    # Execute the same remove-then-merge operations as OpenGrid Plugin Manager's windows_script.
    # Repeat the install to check an update over an existing copy as well as first placement.
    for ($pass = 1; $pass -le 2; $pass++) {
        function Quote-Path([string]$value) { return "'" + $value.Replace("'", "''") + "'" }
        $commands = @("`$ErrorActionPreference = 'Stop'")
        foreach ($entry in $metadata.installs) {
            $commands += "Remove-Item -LiteralPath $(Quote-Path (Join-Path $root $entry.path)) -Recurse -Force -ErrorAction SilentlyContinue"
        }
        foreach ($top in $tops) {
            $commands += "New-Item -ItemType Directory -Force -Path $(Quote-Path $root) | Out-Null"
            $commands += "Copy-Item -LiteralPath $(Quote-Path $top.FullName) -Destination $(Quote-Path $root) -Recurse -Force"
        }
        $installScript = Join-Path $work "install.ps1"
        $commands | Set-Content -LiteralPath $installScript -Encoding utf8BOM
        # Exercise Start-Process as the manager does. Omit only UAC elevation:
        # every copy still targets the temporary root, never actual Program Files.
        $launcher = Join-Path $work "launch install.ps1"
        $scriptArgument = '"' + $installScript + '"'
        $body = "`$p = Start-Process -FilePath powershell -Wait -PassThru -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',$(Quote-Path $scriptArgument)); exit `$p.ExitCode"
        $body | Set-Content -LiteralPath $launcher -Encoding utf8BOM
        & powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File $launcher
        if ($LASTEXITCODE -ne 0) { throw "Plugin-manager launcher/install operations failed" }
        foreach ($entry in $metadata.installs) {
            $source = Join-Path $stage $entry.path
            $dest = Join-Path $root $entry.path
            if (-not (Test-Path -LiteralPath $dest)) { throw "Install missing $($entry.path)" }
            if ((Get-Item -LiteralPath $source).PSIsContainer) {
                $sourceFiles = @(Get-ChildItem -LiteralPath $source -Recurse -File)
                $destFiles = @(Get-ChildItem -LiteralPath $dest -Recurse -File)
                if ($sourceFiles.Count -ne $destFiles.Count) { throw "Stale or nested files after installing $($entry.format)" }
                foreach ($file in $sourceFiles) {
                    $relative = [System.IO.Path]::GetRelativePath($source, $file.FullName)
                    $installed = Join-Path $dest $relative
                    if (-not (Test-Path -LiteralPath $installed)) { throw "Missing module file: $relative" }
                    if ((Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $installed).Hash) { throw "Installed module differs: $relative" }
                }
            } elseif ((Get-FileHash -LiteralPath $source).Hash -ne (Get-FileHash -LiteralPath $dest).Hash) { throw "Installed $($entry.format) differs" }
        }
        if ((Get-Content -Raw -LiteralPath $unrelated).Trim() -ne "leave this plug-in untouched") { throw "Unrelated plug-in was modified" }
        if (-not (Test-Path -LiteralPath (Join-Path $root "OpenGrid/$Plugin/Manual/HybridEQ-User-Manual.pdf"))) { throw "Manual did not install under OpenGrid/$Plugin" }
        if (Test-Path -LiteralPath (Join-Path $common "Common Files")) { throw "Nested Common Files folder" }
        Write-Output "PASS installation pass ${pass}: module contents, replacement, resources and unrelated plug-in"
    }
    $programFiles = if ($env:ProgramW6432) { $env:ProgramW6432 } else { $env:ProgramFiles }
    foreach ($entry in $metadata.installs) { Write-Output ("DESTINATION " + (Join-Path $programFiles $entry.path)) }
    Write-Output "PASS x64 Windows release archive installation layout"
} finally {
    if (Test-Path -LiteralPath $work) { Remove-Item -LiteralPath $work -Recurse -Force }
}
