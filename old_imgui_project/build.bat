@echo off
setlocal

REM Find vcvarsall.bat
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if not exist %VCVARS% (
    set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist %VCVARS% (
    set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist %VCVARS% (
    echo "Could not find vcvarsall.bat"
    exit /b 1
)

call %VCVARS% amd64

mkdir build 2>NUL
cd build

rc.exe /nologo ..\resource.rc
cl.exe /nologo /EHsc /W4 /WX- /O2 /Oi /MD /I..\src /I..\third_party /I..\third_party\imgui /I..\third_party\imgui\backends /DUNICODE /D_UNICODE /FeWallpaperAnim.exe ..\src\main.cpp ..\src\app.cpp ..\src\desktop\desktop_integration.cpp ..\src\render\dx11_renderer.cpp ..\src\render\video_player.cpp ..\src\render\gif_player.cpp ..\src\render\shader_player.cpp ..\src\system\system_monitor.cpp ..\src\ui\settings_ui.cpp ..\src\utils\texture_loader.cpp ..\src\utils\thumbnail_generator.cpp ..\src\utils\preview_player.cpp ..\src\utils\youtube_downloader.cpp ..\src\utils\update_checker.cpp ..\src\tray.cpp ..\src\config.cpp ..\third_party\imgui\imgui.cpp ..\third_party\imgui\imgui_draw.cpp ..\third_party\imgui\imgui_tables.cpp ..\third_party\imgui\imgui_widgets.cpp ..\third_party\imgui\backends\imgui_impl_win32.cpp ..\third_party\imgui\backends\imgui_impl_dx11.cpp ..\resource.res user32.lib d3d11.lib dxgi.lib shell32.lib ole32.lib oleaut32.lib comdlg32.lib winhttp.lib shlwapi.lib

if %ERRORLEVEL% equ 0 (
    echo Build successful.
    if exist ..\yt-dlp.exe (
        copy /Y ..\yt-dlp.exe .\yt-dlp.exe >nul
    )
    exit /b 0
) else (
    echo Build failed.
)
