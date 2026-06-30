# Builds WallpaperAnimSetup.exe from the current x64\Release output.
#
# The setup is a small .NET Framework WinForms app (setup.cs) that carries the
# whole application payload as an embedded resource named "app.zip". On run it
# extracts that zip into %LocalAppData%\WallpaperAnim and creates shortcuts.
#
# Steps:
#   1. Stage x64\Release into a clean folder (dropping build junk).
#   2. Zip the staged folder into app.zip.
#   3. Compile setup.cs with csc, embedding app.zip + the app icon.
#
# Run build.bat (or MSBuild Release/x64) first so x64\Release is up to date.

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

$releaseDir = Join-Path $root "x64\Release"
if (-not (Test-Path (Join-Path $releaseDir "WallpaperAnimWinUI.exe"))) {
    Write-Error "x64\Release\WallpaperAnimWinUI.exe bulunamadi. Once build.bat ile Release derleyin."
}

# yt-dlp.exe must ship with the app so YouTube downloads work after install.
if (-not (Test-Path (Join-Path $releaseDir "yt-dlp.exe"))) {
    Write-Error "x64\Release\yt-dlp.exe eksik. vcxproj artik kopyalamali; Release'i yeniden derleyin."
}

$stageDir = Join-Path $root "SetupStage"
$appZip   = Join-Path $root "app.zip"

Remove-Item -Recurse -Force $stageDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path $stageDir | Out-Null

Write-Host "Release dosyalari hazirlaniyor..."
Copy-Item -Path "$releaseDir\*" -Destination $stageDir -Recurse -Force -Exclude `
    "*.obj", "*.pch", "*.pdb", "*.lib", "*.exp", "*.ilk", "*.tlog", `
    "*.lastbuildstate", "*.cache", "*.rsp", "*.resfiles", `
    "*.resfiles.intermediate", "*.log", "*.asm"

# Intermediate build folders that Copy-Item -Exclude does not catch (it only
# filters leaf names, not directories).
foreach ($junk in @("Unmerged", "Merged", "Manifests", "Generated Files")) {
    Remove-Item -Recurse -Force (Join-Path $stageDir $junk) -ErrorAction SilentlyContinue
}

Write-Host "app.zip olusturuluyor..."
Remove-Item $appZip -Force -ErrorAction SilentlyContinue
Compress-Archive -Path "$stageDir\*" -DestinationPath $appZip -Force

Write-Host "Setup derleniyor (csc)..."
$csc = "$env:WINDIR\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
$icon = Join-Path $root "assets\icon.ico"

# Build args as an array so PowerShell quotes each element correctly
# (paths may contain spaces). csc's /resource takes "path,logicalName".
$cscArgs = @(
    "/nologo",
    "/target:winexe",
    "/out:$root\WallpaperAnimSetup.exe",
    "/win32icon:$icon",
    "/reference:System.Windows.Forms.dll",
    "/reference:System.Drawing.dll",
    "/reference:System.IO.Compression.dll",
    "/reference:System.IO.Compression.FileSystem.dll",
    "/resource:$appZip,app.zip",
    "$root\setup.cs"
)
& $csc $cscArgs

if ($LASTEXITCODE -ne 0) { Write-Error "csc derlemesi basarisiz." }

# Clean up intermediates
Remove-Item -Recurse -Force $stageDir -ErrorAction SilentlyContinue
Remove-Item $appZip -Force -ErrorAction SilentlyContinue

Write-Host "Tamamlandi: WallpaperAnimSetup.exe"
