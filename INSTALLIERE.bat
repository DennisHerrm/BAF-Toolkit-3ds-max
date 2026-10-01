@echo off
REM ============================================================
REM  BAF Import 0.2.0 - Paket erstellen + installieren
REM
REM  1. Baut die Paketstruktur aus output\ und scripts\
REM  2. Kopiert das fertige Paket nach ApplicationPlugins
REM
REM  Zielordner (pro Benutzer, kein Admin noetig):
REM    %APPDATA%\Autodesk\ApplicationPlugins\BafToolkit
REM  ausgeschrieben:
REM    C:\Users\<du>\AppData\Roaming\Autodesk\ApplicationPlugins\BafToolkit
REM
REM  Voraussetzung: BAUE_ALLE.bat wurde bereits ausgefuehrt.
REM ============================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

set "SRC=%~dp0output"
set "SCRIPTS=%~dp0scripts"
set "PKG=%~dp0package\BafToolkit"
set "DEST=%APPDATA%\Autodesk\ApplicationPlugins\BafToolkit"

echo.
echo ========================================
echo  BAF Toolkit - Paket erstellen
echo ========================================
echo.

if not exist "%SRC%" (
    echo FEHLER: output\ Ordner nicht gefunden!
    echo Bitte zuerst BAUE_ALLE.bat ausfuehren.
    pause
    exit /b 1
)

if not exist "%PKG%\PackageContents.xml" (
    echo FEHLER: package\BafToolkit\PackageContents.xml fehlt!
    pause
    exit /b 1
)

REM ============================================================
REM  Ist eine bereits installierte DLU gesperrt?
REM
REM  Geprueft wird die echte Bedingung: die vorhandene Datei zum
REM  Anhaengen oeffnen, ohne etwas zu schreiben. Klappt das, ist
REM  sie frei. Eine tasklist-Abfrage auf 3dsmax.exe waere die
REM  falsche Pruefung - sie testet, ob ein Prozess laeuft, statt
REM  ob die Datei schreibbar ist.
REM
REM  Bei der ERSTEN Installation gibt es nichts zu pruefen - die
REM  Schleife laeuft dann ins Leere.
REM
REM  Notfalls ueberspringen:  set BAF_SKIP_LOCKCHECK=1
REM ============================================================

set "LOCKED="

if defined BAF_SKIP_LOCKCHECK (
    echo Sperrpruefung uebersprungen ^(BAF_SKIP_LOCKCHECK^).
    echo.
) else (
    if exist "%DEST%" (
        for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
            if exist "%DEST%\Contents\%%V\BafToolkit.dlu" (
                (call ) 2>nul 1>>"%DEST%\Contents\%%V\BafToolkit.dlu" || set "LOCKED=1"
            )
        )
    )
)

if defined LOCKED (
    echo FEHLER: Eine bereits installierte BafToolkit.dlu ist gesperrt.
    echo.
    echo Das heisst fast immer: 3ds Max laeuft noch und hat das Plugin
    echo geladen. Bitte alle Max-Instanzen schliessen und erneut starten.
    echo.
    pause
    exit /b 1
)

REM ---- Alte Paketinhalte entfernen, damit keine Reste ----
REM ---- aus einem frueheren Build mitgeschleppt werden  ----
REM ---- PackageContents.xml selbst bleibt stehen.       ----
if exist "%PKG%\Contents" rmdir /s /q "%PKG%\Contents" >nul 2>&1

echo Kopiere Plugins in das Paket...
echo.

set FOUND=0
for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
    if exist "%SRC%\%%V\BafToolkit.dlu" (
        mkdir "%PKG%\Contents\%%V" >nul 2>&1
        copy /Y "%SRC%\%%V\BafToolkit.dlu" "%PKG%\Contents\%%V\" >nul
        echo  %%V: OK
        set /a FOUND+=1
    ) else (
        echo  %%V: UEBERSPRUNGEN  [keine DLU in output\%%V\]
    )
)

echo.
if %FOUND%==0 (
    echo FEHLER: Keine kompilierten Plugins gefunden!
    echo Bitte zuerst BAUE_ALLE.bat ausfuehren.
    pause
    exit /b 1
)

REM ---- Skripte: Oberflaeche und Menueeintraege ----
REM Die liegen im Quellbaum unter scripts\ und werden bei jedem
REM Lauf frisch ins Paket kopiert. Deshalb ueberlebt ein
REM Bearbeiten von scripts\*.mcr das naechste INSTALLIERE.bat.
echo Kopiere Skripte in das Paket...

mkdir "%PKG%\Contents\MacroScripts"          >nul 2>&1

copy /Y "%SCRIPTS%\BafImport.mcr"          "%PKG%\Contents\MacroScripts\"          >nul
copy /Y "%SCRIPTS%\BafExport.mcr"          "%PKG%\Contents\MacroScripts\"          >nul

if not exist "%PKG%\Contents\MacroScripts\BafImport.mcr" (
    echo FEHLER: BafImport.mcr konnte nicht kopiert werden.
    pause
    exit /b 1
)
echo  Skripte: OK
echo.

REM ============================================================
REM  Alte Paketfassungen entfernen, BEVOR die neue kommt.
REM
REM  Bis 1.3.1 hiess das Paket BafImport. Liegen beide Ordner in
REM  ApplicationPlugins, laedt Max BEIDE DLLs - und die zweite
REM  meldet dieselbe Schnittstelle BafCpp mit derselben ID an.
REM  Max bricht dann beim Start ab:
REM
REM    Core interface named BafCpp from module (null) with a
REM    duplicate id of (0x41F26B07,0x1D9C3E55) will not be
REM    registered.
REM
REM  Die IDs sind beim Umbenennen absichtlich gleich geblieben,
REM  damit vorhandene Szenen weiter passen - genau deshalb
REM  kollidieren die beiden Fassungen aber hart.
REM
REM  Das hier gehoert in den Installer und nicht in eine
REM  Anleitung: wer die neue Fassung installiert, hat keinen
REM  Grund, vorher ein Deinstallationsskript zu suchen.
REM ============================================================
set "ALT=%APPDATA%\Autodesk\ApplicationPlugins\BafImport"
REM Auch der maschinenweite Ort wird geprueft: wer dort einmal
REM von Hand hinkopiert hat, faellt sonst in dieselbe Falle.
set "ALT2=%ALLUSERSPROFILE%\Autodesk\ApplicationPlugins\BafImport"
if exist "%ALT%" (
    echo Alte Fassung gefunden, wird entfernt:
    echo   %ALT%
    rmdir /s /q "%ALT%" >nul 2>&1
    if exist "%ALT%" (
        echo.
        echo FEHLER: Der alte Ordner laesst sich nicht entfernen.
        echo Laeuft 3ds Max noch? Bitte schliessen und erneut versuchen.
        echo Sonst von Hand loeschen:
        echo   %ALT%
        echo.
        pause
        exit /b 1
    )
    echo  entfernt.
    echo.
)
if exist "%ALT2%" (
    echo Alte Fassung auch unter ProgramData gefunden:
    echo   %ALT2%
    rmdir /s /q "%ALT2%" >nul 2>&1
    if exist "%ALT2%" (
        echo   Liess sich nicht entfernen - dort braucht es
        echo   Administratorrechte. Bitte von Hand loeschen.
        echo.
    )
)

echo Installiere nach:
echo   %DEST%
echo.

if exist "%DEST%" rmdir /s /q "%DEST%" >nul 2>&1
mkdir "%DEST%" >nul 2>&1

if not exist "%DEST%" (
    echo FEHLER: Zielordner konnte nicht angelegt werden.
    echo   %DEST%
    pause
    exit /b 1
)

xcopy /E /I /Y "%PKG%\*" "%DEST%\" >nul
if errorlevel 1 (
    echo FEHLER beim Kopieren nach %DEST%
    pause
    exit /b 1
)

REM ---- Gegenpruefen, dass wirklich alles angekommen ist. ----
REM ---- xcopy meldet nicht jeden Teilausfall als Fehler.  ----
set "MISSING="
if not exist "%DEST%\PackageContents.xml"                                 set "MISSING=PackageContents.xml"
if not exist "%DEST%\Contents\MacroScripts\BafImport.mcr"                 set "MISSING=BafImport.mcr"
if not exist "%DEST%\Contents\MacroScripts\BafExport.mcr"                 set "MISSING=BafExport.mcr"

if defined MISSING (
    echo FEHLER: Im Zielordner fehlt: !MISSING!
    pause
    exit /b 1
)

echo.
echo ========================================
echo  Installiert.
echo ========================================
echo.
echo 3ds Max starten. Danach:
echo.
echo   Datei  -^>  Importieren  -^>  Big Ape Actor ^(*.baf, *.lab^)
echo   Datei  -^>  Exportieren -^>  Big Ape Actor ^(*.baf^)
echo.
echo Beide Fenster stehen ausserdem unter
echo   Customize -^> Customize User Interface -^> Kategorie "DH Tools"
echo.
echo Das Makro steht ausserdem unter
echo   Customize -^> Customize User Interface -^> Category "DH Tools"
echo.
echo Test im Listener:
echo   BafCpp.version^(^)
echo.
pause
exit /b 0
