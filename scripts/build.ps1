# Configure and build the primary Win32 x86 re2DJ host.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts/build.ps1 [-Preset <name>] [-Configuration <cfg>]

param(
    [string]$Preset = "windows-x86-debug",
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
Set-Location $repository

cmake --preset $Preset
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
cmake --build --preset $Preset --config $Configuration
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

# The preset's binaryDir, which need not be named after the preset:
# windows-x86-debug builds into build/windows-x86.
$presets = Get-Content (Join-Path $repository "CMakePresets.json") -Raw | ConvertFrom-Json
$binaryDir = ($presets.configurePresets | Where-Object { $_.name -eq $Preset }).binaryDir
$buildDirectory = [System.IO.Path]::GetFullPath($binaryDir.Replace('${sourceDir}', $repository))

Write-Host ""
Write-Host "Build output: $buildDirectory\bin\$Configuration"
