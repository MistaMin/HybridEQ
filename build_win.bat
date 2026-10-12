@echo off
REM Local Windows build helper for HybridEQ.
REM Loads the MSVC x64 environment, then configures (if needed) and builds.
REM
REM   build_win.bat                 -> configure + build all targets
REM   build_win.bat --configure     -> configure only

setlocal

set "VS=C:\Program Files\Microsoft Visual Studio\18\Community"
set "CMAKE_BIN=C:\Program Files\CMake\bin"
set "NINJA_BIN=C:\Users\marco\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin"

call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialise the MSVC environment.
    exit /b 1
)

set "PATH=%PATH%;%CMAKE_BIN%;%NINJA_BIN%"

if not exist "build3\CMakeCache.txt" (
    echo [HybridEQ] Configuring...
    cmake -B build3 -G Ninja -DCMAKE_BUILD_TYPE=Release
    if errorlevel 1 exit /b 1
)

if /i "%~1"=="--configure" exit /b 0

echo [HybridEQ] Building all targets...
cmake --build build3 --config Release

exit /b %errorlevel%
