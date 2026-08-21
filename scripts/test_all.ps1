# Build re2DJ and run the full unit test suite on a 64-bit Windows host.
#
# Warnings are errors here: a warning that only CI rejects is a warning that
# reaches the default branch first.

param(
    [string]$Preset = "windows-x64-debug",
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
Set-Location $repository

cmake --preset $Preset -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset $Preset --config $Configuration
ctest --preset $Preset
