@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

REM ============================================================
REM  VERGLEICHE.bat - die Gegenprobe
REM
REM  Dumpt alle Modelle aus big.lab zweimal: einmal mit dem
REM  C++-Parser, einmal mit dem Codec des Blender-Addons. Danach
REM  Datei fuer Datei vergleichen. Erwartet wird: kein Unterschied.
REM
REM    VERGLEICHE.bat "D:\Spiele\Episode1\big.lab" "D:\addon\io_scene_baf"
REM
REM  Das zweite Argument ist der Ordner, in dem "baf\codec.py"
REM  liegt - also der entpackte Addon-Ordner io_scene_baf.
REM ============================================================

set "LAB=%~1"
set "ADDON=%~2"

if "%LAB%"=="" goto :usage
if "%ADDON%"=="" goto :usage

if not exist "%LAB%" (
    echo  Nicht gefunden: %LAB%
    goto :ende
)
if not exist "%ADDON%\baf\codec.py" (
    echo  In %ADDON% liegt kein baf\codec.py - falscher Ordner?
    goto :ende
)
if not exist "output\tools\bafdump.exe" (
    echo  output\tools\bafdump.exe fehlt. Erst BAUE_ALLE.bat laufen lassen.
    goto :ende
)

echo.
echo ========================================
echo  Gegenprobe C++ gegen Python
echo ========================================
echo.

rmdir /s /q "dump_cpp" >nul 2>&1
rmdir /s /q "dump_py"  >nul 2>&1
mkdir "dump_cpp" >nul 2>&1
mkdir "dump_py"  >nul 2>&1

echo  Hinweis: mit --all wird jeder Keyframe nachgerechnet. Das ergibt
echo  rund 150 MB je Dump-Ordner, also 300 MB insgesamt.
echo.
echo  [1/3] C++-Dump...
output\tools\bafdump.exe "%LAB%" --alle "dump_cpp" --all
if errorlevel 1 (
    echo  FEHLER beim C++-Dump.
    goto :ende
)

echo.
echo  [2/3] Python-Dump...
python tools\pydump.py "%ADDON%" "%LAB%" --alle "dump_py" --all
if errorlevel 1 (
    echo  FEHLER beim Python-Dump. Ist Python im PATH?
    goto :ende
)

echo.
echo  [3/3] Vergleiche...
set DIFFS=0
set FILES=0
for %%F in ("dump_cpp\*.txt") do (
    set /a FILES+=1
    REM /L = zeilenweise vergleichen, nicht binaer
    fc /L "dump_cpp\%%~nxF" "dump_py\%%~nxF" >nul 2>&1
    if errorlevel 1 (
        echo   ABWEICHUNG: %%~nxF
        set /a DIFFS+=1
    )
)

echo.
echo ========================================
if "!DIFFS!"=="0" (
    echo  !FILES! Dateien, 0 Abweichungen. Parser stimmt.
) else (
    echo  !FILES! Dateien, !DIFFS! mit Abweichungen.
    echo  Einzelne ansehen mit:
    echo    fc /L dump_cpp\NAME.baf.txt dump_py\NAME.baf.txt ^| more
)
echo ========================================
echo.
goto :ende

:usage
echo.
echo  VERGLEICHE.bat ^<pfad-zu-big.lab^> ^<io_scene_baf-ordner^>
echo.
echo  Beispiel:
echo    VERGLEICHE.bat "D:\Episode1\big.lab" "D:\addons\io_scene_baf"
echo.

:ende
pause
