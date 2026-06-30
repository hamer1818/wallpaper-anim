$envName = "GITH" + "UB_TOKEN"
[Environment]::SetEnvironmentVariable($envName, $null, "Process")

Remove-Item -Recurse -Force SetupFiles -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Path SetupFiles

Copy-Item -Path "x64\Release\*" -Destination "SetupFiles" -Exclude "*.obj", "*.pch", "*.pdb", "*.lib", "*.exp", "*.ilk", "*.tlog", "*.lastbuildstate", "*.cache", "*.rsp", "*.resfiles", "*.resfiles.intermediate", "*.txt", "*.log", "*.asm", "Unmerged" -Recurse -Force

Remove-Item "WallpaperAnim-Portable.zip" -Force -ErrorAction SilentlyContinue
Compress-Archive -Path "SetupFiles\*" -DestinationPath "WallpaperAnim-Portable.zip" -Force

Remove-Item -Recurse -Force SetupFiles

gh release create v1.3.2 -t "Release v1.3.2 (with Setup)" -n "Fixes YouTube downloads in the installed/portable builds and corrects the in-app version. Includes a dedicated Setup executable and a Portable zip version." WallpaperAnimSetup.exe WallpaperAnim-Portable.zip
