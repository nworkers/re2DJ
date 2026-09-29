# Configure, build, and test the primary Windows x86 Release runtime.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts/build_release.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/build_release.ps1 -SkipTests

param(
    [string]$Preset = "windows-x86-debug",
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
Set-Location $repository

cmake --preset $Preset -DRE2DJ_WARNINGS_AS_ERRORS=ON
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

cmake --build --preset $Preset --config Release
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

# The preset's binaryDir, which need not be named after the preset:
# windows-x86-debug builds into build/windows-x86.
$presets = Get-Content (Join-Path $repository "CMakePresets.json") -Raw | ConvertFrom-Json
$binaryDir = ($presets.configurePresets | Where-Object { $_.name -eq $Preset }).binaryDir
$buildDirectory = [System.IO.Path]::GetFullPath($binaryDir.Replace('${sourceDir}', $repository))

if (-not $SkipTests) {
    ctest --test-dir $buildDirectory -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

Write-Host ""
Write-Host "Release output: $buildDirectory\bin\Release"
