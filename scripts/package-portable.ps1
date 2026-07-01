# Creates WallpaperAnim-Portable.zip from the current x64\Release output.
# Run build.bat (or MSBuild Release/x64) first so x64\Release is up to date.
# Shared by release.ps1 and the GitHub Actions workflow so packaging lives in one place.
$ErrorActionPreference = "Stop"
$root = Join-Path $PSScriptRoot ".."

$releaseDir = Join-Path $root "x64\Release"
if (-not (Test-Path (Join-Path $releaseDir "WallpaperAnimWinUI.exe"))) {
    Write-Error "x64\Release\WallpaperAnimWinUI.exe not found. Build Release/x64 first."
}
if (-not (Test-Path (Join-Path $releaseDir "yt-dlp.exe"))) {
    Write-Error "x64\Release\yt-dlp.exe missing. Ensure yt-dlp.exe is next to the project and rebuild."
}

$stageDir = Join-Path $root "PortableStage"
$zipPath  = Join-Path $root "WallpaperAnim-Portable.zip"

Remove-Item -Recurse -Force $stageDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stageDir | Out-Null

# Copy the runtime payload, dropping build intermediates (leaf-name filter only).
Copy-Item -Path "$releaseDir\*" -Destination $stageDir -Recurse -Force -Exclude `
    "*.obj", "*.pch", "*.pdb", "*.lib", "*.exp", "*.ilk", "*.tlog", `
    "*.lastbuildstate", "*.cache", "*.rsp", "*.resfiles", `
    "*.resfiles.intermediate", "*.log", "*.asm"

# Intermediate folders Copy-Item -Exclude does not catch (it filters leaf names, not dirs).
foreach ($junk in @("Unmerged", "Merged", "Manifests", "Generated Files")) {
    Remove-Item -Recurse -Force (Join-Path $stageDir $junk) -ErrorAction SilentlyContinue
}

Remove-Item $zipPath -Force -ErrorAction SilentlyContinue
Compress-Archive -Path "$stageDir\*" -DestinationPath $zipPath -Force

Remove-Item -Recurse -Force $stageDir -ErrorAction SilentlyContinue
Write-Host "Created $zipPath"
