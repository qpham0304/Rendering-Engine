@echo off
rem Builds a Ninja preset with MSVC, e.g.  scripts\ninja-build.cmd window-ninja-debug
rem
rem Ninja uses whatever compiler is on PATH. In a normal shell that is MinGW gcc (C:\msys64), not cl.exe,
rem and the engine only builds with MSVC. vcvars64.bat puts cl.exe and its include / lib paths in place first.

setlocal
if "%~1"=="" (
    echo usage: %~nx0 ^<build preset^>   e.g. window-ninja-debug
    exit /b 1
)

rem Visual Studio 2022 (version 17), the same toolset the MSVC preset uses
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -version [17.0^,18.0^) -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL (
    echo Visual Studio 2022 with the C++ tools was not found
    exit /b 1
)

call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1

cd /d "%~dp0.."
if not exist "build\ninja\CMakeCache.txt" (
    cmake --preset windows-ninja || exit /b 1
)
cmake --build --preset %1
