@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo.
echo ========================================
echo  BAF Toolkit 1.8.3 - Max 2016 bis 2027
echo ========================================
echo.

REM ============================================================
REM  Visual Studio finden
REM
REM  Uebernommen aus dem Buildscript von XFBIN Import, das
REM  wiederum aus WhiteoutDex stammt. vswhere liegt bei jeder
REM  VS-Installation an derselben Stelle und sagt, was da ist -
REM  besser als einen Generator fest einzutragen und beim
REM  naechsten VS-Sprung wieder anzufassen.
REM
REM  Ueberschreibbar:  set BAF_GENERATOR=Visual Studio 17 2022
REM ============================================================
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

if not defined BAF_GENERATOR (
    set "BAF_GENERATOR=Visual Studio 17 2022"

    if exist "%VSWHERE%" (
        for /f "delims=" %%R in ('"%VSWHERE%" -version [18.0^,19.0^) -property installationPath 2^>nul') do (
            if exist "%%R" set "BAF_GENERATOR=Visual Studio 18 2026"
        )
    )
)

REM ---- Strenge Compiler-Warnungen? ----
REM ON  = /W4 (plus /permissive-, wenn die SDK-Header es aushalten)
REM OFF = /W3
if not defined BAF_STRICT set BAF_STRICT=ON

REM ---- Ausgabe-Ordner ----
set OUTPUT=%~dp0output

echo Generator:  %BAF_GENERATOR%
echo Warnungen:  %BAF_STRICT%
echo Ausgabe:    %OUTPUT%
echo.

REM STOP_ON_ERROR=1 bricht nach dem ersten Fehlschlag ab. Bei einem
REM Quellcode-Fehler scheitern ohnehin alle Versionen mit derselben
REM Meldung - dann muss man nicht zwoelfmal warten.
if not defined STOP_ON_ERROR set STOP_ON_ERROR=1
REM Auf 0 setzen, um trotz Fehler alle Versionen durchzubauen:
REM   set STOP_ON_ERROR=0
set BUILD_FAILED=0

REM Eine einzelne Version bauen:  BAUE_ALLE.bat 2019
set "SINGLE=%~1"

REM ============================================================
REM  Gibt es die Max-Schicht ueberhaupt schon?
REM
REM  Stufe 0 sind nur Parser und Werkzeug. src\baftoolkit.cpp
REM  entsteht erst in Stufe 1. Solange die Datei fehlt, wird das
REM  Plugin uebersprungen statt mit einem Fehler abzubrechen -
REM  das Paket soll auch im Zwischenstand bauen.
REM ============================================================
set HAVE_PLUGIN=0
if exist "src\baftoolkit.cpp" set HAVE_PLUGIN=1

if "%HAVE_PLUGIN%"=="0" (
    echo  Hinweis: src\baftoolkit.cpp fehlt noch - Stufe 0 baut nur
    echo           das Werkzeug bafdump.exe. Das ist erwartet.
    echo.
)

REM ============================================================
REM  Zuerst das eigenstaendige Werkzeug. Es braucht kein Max SDK
REM  und faellt damit als erstes durch, wenn im Parser ein Fehler
REM  steckt - dann muss man nicht zwoelf SDK-Builds abwarten.
REM ============================================================
if not defined SINGLE (
    call :build_tool
    if "%STOP_ON_ERROR%"=="1" if "!BUILD_FAILED!"=="1" goto :summary
)

if "%HAVE_PLUGIN%"=="1" (
    for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
        set "DO=1"
        if defined SINGLE if not "%%V"=="!SINGLE!" set "DO=0"

        if "!DO!"=="1" (
            call :build_version %%V
            if "%STOP_ON_ERROR%"=="1" if "!BUILD_FAILED!"=="1" goto :summary
        )
    )
)

:summary

echo.
echo ========================================
echo  Zusammenfassung
echo ========================================
echo.
if exist "%OUTPUT%\tools\bafdump.exe" (echo  Werkzeug: OK) else (echo  Werkzeug: FEHLT)
set FOUND=0
if "%HAVE_PLUGIN%"=="1" (
    for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
        call :report %%V
    )
    echo.
    echo  !FOUND! Version^(en^) gebaut. Dateien liegen in: %OUTPUT%
) else (
    echo  Plugin:   noch nicht vorhanden ^(Stufe 1^)
    echo.
    echo  Naechster Schritt - Gegenprobe gegen die Python-Referenz:
    echo    VERGLEICHE.bat ^<pfad-zu-big.lab^> ^<io_scene_baf-ordner^>
)
echo.
pause
exit /b 0

REM ============================================================
:build_tool

echo ----------------------------------------
echo  Werkzeug bafdump.exe (ohne Max SDK)
echo ----------------------------------------
echo  Konfiguriere...

rmdir /s /q "build_tool" >nul 2>&1

cmake -B "build_tool" -G "%BAF_GENERATOR%" -A x64 ^
    -DBAF_BUILD_PLUGIN=OFF ^
    -DBAF_BUILD_TOOL=ON ^
    -DBAF_STRICT=%BAF_STRICT% > "build_tool_configure.log" 2>&1
if errorlevel 1 (
    echo.
    echo  FEHLER beim Konfigurieren. Die letzten Zeilen:
    echo  ------------------------------------------------------------
    powershell -NoProfile -Command "Get-Content 'build_tool_configure.log' -Tail 15" 2>nul
    echo  ------------------------------------------------------------
    echo  Vollstaendiges Log: build_tool_configure.log
    echo.
    set BUILD_FAILED=1
    goto :eof
)

echo  Kompiliere...
cmake --build "build_tool" --config Release > "build_tool_build.log" 2>&1
if errorlevel 1 (
    call :show_errors "build_tool_build.log"
    set BUILD_FAILED=1
    goto :eof
)

mkdir "%OUTPUT%\tools" >nul 2>&1
for /R "build_tool" %%f in (bafdump.exe) do (
    copy /Y "%%f" "%OUTPUT%\tools\bafdump.exe" >nul 2>&1
)

if exist "%OUTPUT%\tools\bafdump.exe" (
    echo  ERFOLGREICH! -^> output\tools\bafdump.exe
    del /Q "build_tool_configure.log" >nul 2>&1
    del /Q "build_tool_build.log"     >nul 2>&1
) else (
    echo  FEHLER: bafdump.exe nicht gefunden
    set BUILD_FAILED=1
)
echo.
goto :eof

REM ============================================================
:build_version
set VER=%~1
set "SDK=C:\Program Files\Autodesk\3ds Max %VER% SDK\maxsdk"

if not exist "%SDK%\include\max.h" (
    echo  %VER%: kein SDK installiert - uebersprungen
    goto :eof
)

echo ----------------------------------------
echo  Max %VER%
echo ----------------------------------------
echo  SDK: %SDK%
echo  Konfiguriere...

rmdir /s /q "build_%VER%" >nul 2>&1

REM MAX_VERSION steuert nur Kleinigkeiten in der CMakeLists.
REM Ob /permissive- geht, wird dort per check_cxx_source_compiles
REM gegen <max.h> ermittelt - nicht ueber eine geratene Jahresgrenze.
cmake -B "build_%VER%" -G "%BAF_GENERATOR%" -A x64 ^
    -D3DSMAX_SDK_DIR="%SDK%" ^
    -DMAX_VERSION=%VER% ^
    -DBAF_BUILD_TOOL=OFF ^
    -DBAF_STRICT=%BAF_STRICT% > "build_%VER%_configure.log" 2>&1
if errorlevel 1 (
    echo.
    echo  FEHLER beim Konfigurieren. Die letzten Zeilen:
    echo  ------------------------------------------------------------
    powershell -NoProfile -Command "Get-Content 'build_%VER%_configure.log' -Tail 15" 2>nul
    echo  ------------------------------------------------------------
    echo  Vollstaendiges Log: build_%VER%_configure.log
    echo.
    set BUILD_FAILED=1
    goto :eof
)

echo  Kompiliere...
cmake --build "build_%VER%" --config Release > "build_%VER%_build.log" 2>&1
if errorlevel 1 (
    call :show_errors "build_%VER%_build.log"
    set BUILD_FAILED=1
    goto :eof
)

mkdir "%OUTPUT%\%VER%" >nul 2>&1
for /R "build_%VER%" %%f in (BafToolkit.dlu) do (
    copy /Y "%%f" "%OUTPUT%\%VER%\BafToolkit.dlu" >nul 2>&1
)

if exist "%OUTPUT%\%VER%\BafToolkit.dlu" (
    echo  ERFOLGREICH! -^> output\%VER%\BafToolkit.dlu
    del /Q "build_%VER%_configure.log" >nul 2>&1
    del /Q "build_%VER%_build.log"     >nul 2>&1
) else (
    echo  FEHLER: BafToolkit.dlu nicht gefunden
    set BUILD_FAILED=1
)
echo.
goto :eof

REM ============================================================
REM  Nur die Fehlerzeilen aus einem Build-Log zeigen, nicht alles
REM ============================================================
:show_errors
echo.
echo  FEHLER beim Kompilieren. Die ersten Meldungen:
echo  ------------------------------------------------------------
set FOUND_ERR=0
for /f "tokens=* delims=" %%L in ('findstr /R /C:" error " /C:"error C" /C:"error LNK" /C:"error MSB" %1 2^>nul') do (
    if !FOUND_ERR! LSS 12 (
        echo   %%L
        set /a FOUND_ERR+=1
    )
)
if !FOUND_ERR!==0 (
    echo   Keine Zeile mit "error" gefunden - letzte 15 Zeilen des Logs:
    powershell -NoProfile -Command "Get-Content %1 -Tail 15" 2>nul
)
echo  ------------------------------------------------------------
echo  Vollstaendiges Log: %~1
echo.
goto :eof

REM ============================================================
REM  Eine Zeile der Zusammenfassung
REM
REM  Als Unterprogramm und nicht in der Schleife: geschachtelte
REM  if-Bloecke mit Pfaden voller Leerzeichen sind in cmd eine
REM  bekannte Stolperstelle, hier gibt es die Schachtelung nicht.
REM ============================================================
:report
set VER=%~1
if exist "%OUTPUT%\%VER%\BafToolkit.dlu" (
    echo  %VER%: OK
    set /a FOUND+=1
    goto :eof
)
if exist "C:\Program Files\Autodesk\3ds Max %VER% SDK\maxsdk\include\max.h" (
    echo  %VER%: FEHLT
) else (
    echo  %VER%: kein SDK installiert
)
goto :eof
