@echo off
REM ------------------------------
REM Build script for Visual Studio 17 2022
REM Location: myproject/build/
REM ------------------------------

REM Default build configuration
set CONFIG=Debug

REM Allow passing Debug/Release as first argument
if "%1"=="" (
    set CONFIG=Debug
) else (
    set CONFIG=%1
)

REM Get project root (one level up from build/)
set PROJECT_ROOT=%~dp0..
REM Build output directory (at project root, not inside build/)
set BUILD_DIR=%PROJECT_ROOT%\out\%CONFIG%

REM Step 0: Clean previous build
if exist "%BUILD_DIR%" (
    echo Cleaning previous build in "%BUILD_DIR%"...
    rmdir /S /Q "%BUILD_DIR%"
)

REM Step 1: Generate build system
echo Generating build system in "%BUILD_DIR%"...
cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%" -G "Visual Studio 17 2022" -A x64
IF ERRORLEVEL 1 (
    echo CMake generation failed!
    pause
    exit /b 1
)

REM Step 2: Build the project
echo Building project (%CONFIG%)...
cmake --build "%BUILD_DIR%" --config %CONFIG%
IF ERRORLEVEL 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build finished successfully!
pause
