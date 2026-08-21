# Configure and build re2DJ on a 64-bit Windows host.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts/build.ps1 [-Preset <name>] [-Configuration <cfg>]

param(
    [string]$Preset = "windows-x64-debug",
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
Set-Location $repository

cmake --preset $Preset
cmake --build --preset $Preset --config $Configuration

Write-Host ""
Write-Host "Build output: build/$Preset/bin/$Configuration"
