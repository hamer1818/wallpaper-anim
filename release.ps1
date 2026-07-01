# Local release helper: builds the setup executable + portable zip from the current
# x64\Release output and publishes a GitHub release. The version comes from
# src/version.h (single source of truth) — bump it there, not here.
#
# Prerequisite: build Release/x64 first (build.bat).
$ErrorActionPreference = "Stop"

$envName = "GITH" + "UB_TOKEN"
[Environment]::SetEnvironmentVariable($envName, $null, "Process")

$version = & (Join-Path $PSScriptRoot "scripts\get-version.ps1")
$tag = "v$version"

# Build both distributables via the shared scripts.
& (Join-Path $PSScriptRoot "build_setup.ps1")
& (Join-Path $PSScriptRoot "scripts\package-portable.ps1")

$notes = "WallpaperAnim $tag. Includes the Setup executable and a Portable zip."

gh release create $tag -t "Release $tag" -n $notes WallpaperAnimSetup.exe WallpaperAnim-Portable.zip
