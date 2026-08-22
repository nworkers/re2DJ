# Build and run the x64 host / Win32 x86 native-helper IPC integration probe.

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
Set-Location $repository

cmake --preset windows-x64-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build build/windows-x64 --config Debug --target re2dj_native_ipc_host_probe
cmake --preset windows-x86-native-probe -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset windows-x86-native-probe

$host_probe = Join-Path $repository "build/windows-x64/bin/Debug/re2dj_native_ipc_host_probe.exe"
$x86_helper = Join-Path $repository "build/windows-x86-native-probe/bin/Debug/re2dj_native_ipc_helper.exe"
& $host_probe $x86_helper
if ($LASTEXITCODE -ne 0)
{
    throw "native IPC probe failed with exit code $LASTEXITCODE"
}
