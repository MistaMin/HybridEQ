@echo off
REM Local Windows build helper for HybridEQ.
REM Loads the MSVC x64 environment, then configures (if needed) and builds.
REM
REM   build_win.bat                 -> configure + build all targets
REM   build_win.bat --configure     -> configure only

setlocal

call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 (
    echo [ERROR] Failed to initialise the MSVC environment.
    exit /b 1
)

if not exist "build3\CMakeCache.txt" (
    echo [HybridEQ] Configuring...
    cmake -B build3 -G Ninja -DCMAKE_BUILD_TYPE=Release -DHYBRIDEQ_BUILD_TESTS=OFF
    if errorlevel 1 exit /b 1
)

if /i "%~1"=="--configure" exit /b 0

echo [HybridEQ] Building all targets...
cmake --build build3 --config Release

exit /b %errorlevel%
