# Build Windows host and Win32 helper outputs, then stage the helper beside the host.

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
Set-Location $repository

cmake --preset windows-x64-debug -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset windows-x64-debug --target re2dj_windows_import_observer
cmake --preset windows-x86-native-probe -DRE2DJ_WARNINGS_AS_ERRORS=ON
cmake --build --preset windows-x86-native-probe

$hostDirectory = Join-Path $repository "build/windows-x64/bin/Debug"
$helperSource = Join-Path $repository "build/windows-x86-native-probe/bin/Debug/re2dj_native_ipc_helper.exe"
$runtimeSource = Join-Path $repository "build/windows-x86-native-probe/bin/Debug/re2dj_windows_injected_runtime.dll"
$helperDirectory = Join-Path $hostDirectory "helpers/win32"
$helperDestination = Join-Path $helperDirectory "re2dj_native_ipc_helper.exe"
$runtimeDestination = Join-Path $helperDirectory "re2dj_windows_injected_runtime.dll"

if (-not (Test-Path -LiteralPath $helperSource -PathType Leaf))
{
    throw "Win32 helper was not built: $helperSource"
}

New-Item -ItemType Directory -Force -Path $helperDirectory | Out-Null
Copy-Item -LiteralPath $helperSource -Destination $helperDestination -Force
Copy-Item -LiteralPath $runtimeSource -Destination $runtimeDestination -Force
Write-Output "Staged Win32 helper: $helperDestination"
