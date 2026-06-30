@echo off
setlocal

echo Ortam degiskenleri ayarlaniyor...

REM Ortak vcvarsall.bat yollarini kontrol et
set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
if not exist %VCVARS% (
    set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist %VCVARS% (
    set VCVARS="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist %VCVARS% (
    set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist %VCVARS% (
    set VCVARS="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
)
if not exist %VCVARS% (
    echo "HATA: vcvarsall.bat bulunamadi! Visual Studio yuklu oldugundan emin olun."
    pause
    exit /b 1
)

call %VCVARS% amd64

echo.
echo Proje derleniyor (Release - x64)...
msbuild WallpaperAnimWinUI.vcxproj /p:Configuration=Release /p:Platform=x64

if %ERRORLEVEL% equ 0 (
    echo.
    echo Derleme BASARILI! 
    echo Uygulamanizi x64\Release klasoru icinde bulabilirsiniz.
    pause
    exit /b 0
) else (
    echo.
    echo Derleme BASARISIZ! Lutfen yukaridaki hatalari kontrol edin.
    pause
    exit /b 1
)
