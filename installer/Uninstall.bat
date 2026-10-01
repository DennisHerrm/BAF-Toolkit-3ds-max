@echo off
REM ============================================================
REM  BAF Toolkit for 3ds Max - remove a ZIP installation
REM
REM  Removes %ProgramData%\Autodesk\ApplicationPlugins\BafToolkit
REM  (asks for administrator rights). If you used the Setup .exe,
REM  uninstall it from Windows Settings -> Apps instead.
REM ============================================================

setlocal enabledelayedexpansion

net session >nul 2>&1
if errorlevel 1 (
    echo Asking for administrator rights...
    powershell -NoProfile -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b 0
)

set "DEST=%ProgramData%\Autodesk\ApplicationPlugins\BafToolkit"

echo.
echo Removing: %DEST%
echo.

if not exist "%DEST%" (
    echo Nothing to do - BAF Toolkit is not installed.
    echo.
    pause
    exit /b 0
)

set "LOCKED="
for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
    if exist "%DEST%\Contents\%%V\BafToolkit.dlu" (
        (call ) 2>nul 1>>"%DEST%\Contents\%%V\BafToolkit.dlu" || set "LOCKED=1"
    )
)

if defined LOCKED (
    echo ERROR: The plugin is in use.
    echo Close every 3ds Max window and run Uninstall.bat again.
    echo.
    pause
    exit /b 1
)

rmdir /s /q "%DEST%"
if exist "%DEST%" (
    echo ERROR: The folder could not be removed.
    echo.
    pause
    exit /b 1
)

echo Removed.
echo.
pause
exit /b 0
