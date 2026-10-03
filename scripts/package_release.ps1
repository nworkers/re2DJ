# Package the already-built Windows x86 runtime for a GitHub Release.
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File scripts/package_release.ps1
#   powershell -ExecutionPolicy Bypass -File scripts/package_release.ps1 -Version 0.0.40

param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$Version = "",
    [string]$OutputDirectory = "build/package"
)

$ErrorActionPreference = "Stop"
$repository = Split-Path -Parent $PSScriptRoot
Set-Location $repository

if ([string]::IsNullOrWhiteSpace($Version)) {
    $Version = (Get-Content (Join-Path $repository "VERSION") -Raw -Encoding UTF8).Trim()
}
if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    throw "Version must use major.minor.patch format, found '$Version'."
}

$buildOutput = Join-Path $repository "build/windows-x86/bin/$Configuration"
$packageDirectory = Join-Path $repository $OutputDirectory
$stagingDirectory = Join-Path $packageDirectory "staging"
$stagingRoot = Join-Path $packageDirectory "staging/re2dj-v$Version-windows-x86"
$archivePath = Join-Path $packageDirectory "re2dj-v$Version-windows-x86.zip"
$checksumPath = "$archivePath.sha256"

if (-not (Test-Path $buildOutput -PathType Container)) {
    throw "Build output directory does not exist: $buildOutput"
}

$requiredBinaries = @(
    "re2dj.exe",
    "re2dj_windows_injected_runtime.dll"
)
foreach ($binary in $requiredBinaries) {
    $source = Join-Path $buildOutput $binary
    if (-not (Test-Path $source -PathType Leaf)) {
        throw "Required Release binary does not exist: $source"
    }
}

if (Test-Path $stagingRoot) {
    Remove-Item $stagingRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $stagingRoot -Force | Out-Null
New-Item -ItemType Directory -Path $packageDirectory -Force | Out-Null

foreach ($binary in $requiredBinaries) {
    Copy-Item (Join-Path $buildOutput $binary) (Join-Path $stagingRoot $binary)
}

foreach ($document in @("README.md", "LICENSE", "VERSION", "RELEASE_NOTES.md", "THIRD_PARTY_NOTICES.md", "CREDITS.md")) {
    $source = Join-Path $repository $document
    if (Test-Path $source -PathType Leaf) {
        Copy-Item $source (Join-Path $stagingRoot $document)
    }
}

$configDirectory = Join-Path $repository "config"
if (Test-Path $configDirectory -PathType Container) {
    Copy-Item $configDirectory (Join-Path $stagingRoot "config") -Recurse
}

if (Test-Path $archivePath) {
    Remove-Item $archivePath -Force
}
Compress-Archive -Path (Join-Path $stagingRoot "*") -DestinationPath $archivePath -CompressionLevel Optimal

$hash = (Get-FileHash $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
[System.IO.File]::WriteAllText(
    $checksumPath,
    "$hash *$([System.IO.Path]::GetFileName($archivePath))`r`n",
    [System.Text.UTF8Encoding]::new($false))

Remove-Item $stagingRoot -Recurse -Force
if ((Test-Path $stagingDirectory -PathType Container) -and
    (@(Get-ChildItem $stagingDirectory -Force).Count -eq 0)) {
    Remove-Item $stagingDirectory -Force
}

Write-Host "Release package: $archivePath"
Write-Host "Release checksum: $checksumPath"
