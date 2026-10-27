@echo off
rem Builds GluePP with Visual Studio 2022 (or 2019): double-click this file.
rem Needs Visual Studio with the "Desktop development with C++" workload (it includes CMake).
rem Everything else (GLFW, OpenGL loader, stb, miniaudio, JSON) is already in third_party\.
setlocal
cd /d "%~dp0"

rem Find Visual Studio's compiler and CMake, unless they are already on PATH.
where cl >nul 2>nul
if not errorlevel 1 goto build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto novs
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR goto novs
call "%VSDIR%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul

:build
cmake -B build -A x64
if errorlevel 1 goto failed
cmake --build build --config Release --parallel
if errorlevel 1 goto failed
echo.
echo Done: build\bin\Glue3D.exe  (with assets\ and player\ next to it)
start "" "build\bin"
pause
exit /b 0

:novs
echo Visual Studio was not found.
echo Install Visual Studio 2022 Community with the "Desktop development with C++" workload, then run this again.
pause
exit /b 1

:failed
echo.
echo The build failed (see the messages above).
pause
exit /b 1
