@echo off
REM Build the standalone Windows game and stage a redistributable folder.
REM
REM Output: dist\game-native\  (CowEngine.exe + assets, self-contained — zip this folder to ship)
REM
REM Requires:
REM   - vcpkg installed and VCPKG_ROOT set as an env var
REM   - Visual Studio Build Tools (or any C++17 compiler) and CMake >= 3.21

setlocal enabledelayedexpansion
cd /d "%~dp0\..\.."

if "%VCPKG_ROOT%"=="" (
    echo ERROR: VCPKG_ROOT is not set. Set it ^(e.g. set VCPKG_ROOT=C:\vcpkg^) or edit this script.
    exit /b 1
)

cmake --preset game-native || exit /b 1
cmake --build --preset game-native --config Release || exit /b 1
cmake --install build\game-native --config Release || exit /b 1

echo.
echo === Game built ===
echo Self-contained folder: %CD%\dist\game-native
echo Run with:              .\dist\game-native\CowEngine.exe
endlocal
