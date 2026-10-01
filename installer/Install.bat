@echo off
REM ============================================================
REM  BAF Toolkit for 3ds Max - install from the ZIP package
REM
REM  Copies the BafToolkit folder next to this file into
REM    %ProgramData%\Autodesk\ApplicationPlugins\BafToolkit
REM  (all users - asks for administrator rights).
REM
REM  The Setup .exe from the release page does the same and also
REM  installs the Microsoft Visual C++ runtime when it is missing.
REM ============================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

REM ---- Administrator rights: restart elevated if needed ----
net session >nul 2>&1
if errorlevel 1 (
    echo Asking for administrator rights...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b 0
)

set "PKG=%~dp0BafToolkit"
set "DEST=%ProgramData%\Autodesk\ApplicationPlugins\BafToolkit"
set "OLDUSER=%APPDATA%\Autodesk\ApplicationPlugins\BafToolkit"

echo.
echo ========================================
echo  BAF Toolkit for 3ds Max - Install
echo ========================================
echo.

if not exist "%PKG%\PackageContents.xml" (
    echo ERROR: The BafToolkit folder was not found next to this file.
    echo Extract the whole ZIP first, then run Install.bat from there.
    echo.
    pause
    exit /b 1
)

REM Is an installed plugin locked? Opening the file for append
REM without writing tests exactly that - it fails while a running
REM 3ds Max holds the plugin.
set "LOCKED="
for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
    for %%D in ("%DEST%" "%OLDUSER%") do (
        if exist "%%~D\Contents\%%V\BafToolkit.dlu" (
            (call ) 2>nul 1>>"%%~D\Contents\%%V\BafToolkit.dlu" || set "LOCKED=1"
        )
    )
)

if defined LOCKED (
    echo ERROR: The installed plugin is in use.
    echo Close every 3ds Max window and run Install.bat again.
    echo.
    pause
    exit /b 1
)

REM A per-user copy would be a second package next to this one,
REM the pre-1.3.1 package "BafImport" even crashes 3ds Max.
for %%D in ("%OLDUSER%" "%APPDATA%\Autodesk\ApplicationPlugins\BafImport" "%ProgramData%\Autodesk\ApplicationPlugins\BafImport") do (
    if exist "%%~D" (
        echo Removing older copy: %%~D
        rmdir /s /q "%%~D" >nul 2>&1
    )
)

echo Installing to:
echo   %DEST%
echo.

if exist "%DEST%" rmdir /s /q "%DEST%" >nul 2>&1
mkdir "%DEST%" >nul 2>&1

xcopy /E /I /Y /Q "%PKG%\*" "%DEST%\" >nul
if errorlevel 1 goto :copyfail

if not exist "%DEST%\PackageContents.xml"                 goto :copyfail
if not exist "%DEST%\Contents\MacroScripts\BafImport.mcr" goto :copyfail
if not exist "%DEST%\Contents\MacroScripts\BafExport.mcr" goto :copyfail

echo ========================================
echo  Installed.
echo ========================================
echo.
echo Start 3ds Max, then:
echo.
echo   File -^> Import -^> Big Ape Actor ^(*.baf, *.lab^)
echo   File -^> Export -^> Big Ape Actor ^(*.baf^)
echo.
echo If 3ds Max reports that BafToolkit.dlu failed to load, install
echo the Microsoft Visual C++ runtime:
echo   https://aka.ms/vc14/vc_redist.x64.exe
echo.
pause
exit /b 0

:copyfail
echo ERROR: Copying to %DEST% failed.
echo.
pause
exit /b 1
