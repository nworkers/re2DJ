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
cmake --build --preset $Preset --config $Configuration

Write-Host ""
Write-Host "Build output: build/$Preset/bin/$Configuration"
