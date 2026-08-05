@echo off
REM ---------------------------------------------------------------------------
REM  build-gui.bat - configure, build and launch the MiniVCS Qt Widgets GUI.
REM
REM  Author: Bao Vo
REM
REM  Just double-click this file, or run it from any shell. It puts the Qt and
REM  MinGW tools on PATH for the duration of the build only, so nothing about
REM  your machine's environment is changed permanently.
REM
REM  If your Qt lives somewhere else, edit QT_DIR / MINGW_DIR below.
REM ---------------------------------------------------------------------------
setlocal

set "QT_DIR=C:\Qt\6.8.3\mingw_64"
set "MINGW_DIR=C:\Qt\Tools\mingw1310_64"
set "BUILD_DIR=build-qt"

cd /d "%~dp0"

if not exist "%QT_DIR%\bin\qmake.exe" (
    echo.
    echo [!] Qt was not found at %QT_DIR%
    echo     Edit QT_DIR at the top of this script to point at your Qt kit,
    echo     or build the console version instead:  cmake -S . -B build
    echo.
    pause
    exit /b 1
)

if not exist "%MINGW_DIR%\bin\g++.exe" (
    echo.
    echo [!] The MinGW compiler was not found at %MINGW_DIR%
    echo     Install it from the Qt Maintenance Tool, or edit MINGW_DIR above.
    echo.
    pause
    exit /b 1
)

set "PATH=%MINGW_DIR%\bin;%QT_DIR%\bin;%PATH%"

echo === Configuring ===
cmake -S . -B %BUILD_DIR% -G "MinGW Makefiles" ^
      -DCMAKE_PREFIX_PATH=%QT_DIR% -DCMAKE_BUILD_TYPE=Debug || goto :failed

echo.
echo === Building ===
cmake --build %BUILD_DIR% || goto :failed

echo.
echo === Copying the Qt runtime next to the .exe ===
REM Lets target\MiniVCSGui.exe run on its own, without Qt on PATH.
windeployqt.exe --compiler-runtime --no-translations ^
                --no-system-d3d-compiler --no-opengl-sw target\MiniVCSGui.exe >nul || goto :failed

echo.
echo Build complete. Launching MiniVCS...
start "" "target\MiniVCSGui.exe"
exit /b 0

:failed
echo.
echo [!] Build failed - see the messages above.
pause
exit /b 1
