@echo off
REM Driver Build Script for Windows Driver Kit
REM Requires Visual Studio 2022/2025 and WDK 10.0.26100.0

echo =============================================
echo   Payson Driver Build Script
echo =============================================
echo.

REM Initialize Visual Studio Developer Environment
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64

REM Navigate to the project directory
cd /d "%~dp0"

echo.
echo Building driver...
echo.

REM Build the driver with correct WDK settings
msbuild "payson drv.sln" /p:Configuration=Release /p:Platform=x64 /p:WDKBuildFolder=10.0.26100.0 /p:VisualStudioVersion=17.0 /p:ApiValidator_Enable=false /t:driver

if %ERRORLEVEL% EQU 0 (
    echo.
    echo =============================================
    echo   BUILD SUCCESSFUL!
    echo =============================================
    echo.
    echo Driver output: build\driver\driver.sys
    echo.
    dir /b build\driver\*.sys
) else (
    echo.
    echo =============================================
    echo   BUILD FAILED!
    echo =============================================
)

pause
