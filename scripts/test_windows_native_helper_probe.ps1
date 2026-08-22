# Build and run the Win32 x86 native-helper feasibility probe under WOW64.

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
Set-Location $repository

cmake --preset windows-x86-native-probe -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset windows-x86-native-probe
ctest --preset windows-x86-native-probe
