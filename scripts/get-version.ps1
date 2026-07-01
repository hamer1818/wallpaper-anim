# Single source of truth for the app version.
# Parses APP_VERSION_STRING from src/version.h and prints it (e.g. "1.4.0").
# Every packaging/release step should call this instead of hardcoding a version.
$ErrorActionPreference = "Stop"

$versionHeader = Join-Path $PSScriptRoot "..\src\version.h"
$content = Get-Content $versionHeader -Raw

if ($content -match '#define\s+APP_VERSION_STRING\s+"([0-9]+\.[0-9]+\.[0-9]+)"') {
    Write-Output $Matches[1]
} else {
    throw "APP_VERSION_STRING not found in $versionHeader"
}
