@echo off
REM ============================================================
REM  BAF Toolkit - Release-Dateien erstellen
REM
REM  Voraussetzung: BAUE_ALLE.bat ist gelaufen, output\ enthaelt
REM  die DLUs fuer alle zwoelf Max-Versionen.
REM
REM  Ergebnis in dist\:
REM    BafToolkit-<ver>-Setup.exe   Installer (Inno Setup), installiert
REM                                 nach %ProgramData%\Autodesk\
REM                                 ApplicationPlugins\BafToolkit
REM    BafToolkit-<ver>.zip         Paket + Install.bat zum Entpacken
REM    SHA256SUMS.txt               Pruefsummen fuer die Release-Seite
REM
REM  Der Installer bringt die VC++-Laufzeit von Microsoft mit. Sie
REM  wird hier frisch geladen und nur genommen, wenn sie gueltig
REM  von Microsoft signiert ist und mindestens 14.50 hat.
REM
REM  Danach hochladen - den genauen Befehl gibt das Skript am
REM  Ende aus (gh release create ...).
REM
REM  Optional:
REM    set BAF_ALLOW_PARTIAL=1   fehlende Max-Versionen erlauben
REM    set BAF_ISCC=<pfad>       ISCC.exe, falls nicht gefunden
REM    set BAF_SIGNTOOL=<name>   Installer signieren; <name> ist ein
REM                              in Inno Setup eingerichtetes Sign
REM                              Tool (Tools -^> Configure Sign Tools)
REM ============================================================

setlocal enabledelayedexpansion
cd /d "%~dp0"

set "SRC=%~dp0output"
set "SCRIPTS=%~dp0scripts"
set "DIST=%~dp0dist"
set "STAGE=%DIST%\stage\BafToolkit"
set "ZIPDIR=%DIST%\zip"
set "REDIST=%DIST%\redist\vc_redist.x64.exe"

REM ---- Version aus dem Plugin-Header - die eine Quelle ----
set "VER="
REM Zeile:  #define BAFTOOLKIT_VERSION_STR  _T("1.8.3")
for /f tokens^=2^ delims^=^" %%A in ('findstr /C:"BAFTOOLKIT_VERSION_STR" src\baftoolkit.h') do set "VER=%%A"
if not defined VER (
    echo FEHLER: Version in src\baftoolkit.h nicht gefunden.
    goto :fail
)

echo.
echo ========================================
echo  BAF Toolkit %VER% - Release erstellen
echo ========================================
echo.

findstr /C:"AppVersion=\"%VER%\"" package\BafToolkit\PackageContents.xml >nul
if errorlevel 1 (
    echo FEHLER: PackageContents.xml steht nicht auf %VER%.
    goto :fail
)

REM ---- Inno Setup finden ----
if not defined BAF_ISCC (
    if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "BAF_ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
)
if not defined BAF_ISCC (
    if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set "BAF_ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
)
if not defined BAF_ISCC (
    echo FEHLER: Inno Setup 6 nicht gefunden.
    echo Installieren:  winget install JRSoftware.InnoSetup
    goto :fail
)

REM ---- Paket zusammenstellen - dieselbe Form wie INSTALLIERE.bat ----
if exist "%DIST%" rmdir /s /q "%DIST%"
mkdir "%STAGE%\Contents\MacroScripts" >nul 2>&1

copy /Y package\BafToolkit\PackageContents.xml "%STAGE%\" >nul

set FOUND=0
set MISSING=0
for %%V in (2016 2017 2018 2019 2020 2021 2022 2023 2024 2025 2026 2027) do (
    if exist "%SRC%\%%V\BafToolkit.dlu" (
        mkdir "%STAGE%\Contents\%%V" >nul 2>&1
        copy /Y "%SRC%\%%V\BafToolkit.dlu" "%STAGE%\Contents\%%V\" >nul
        echo  %%V: OK
        set /a FOUND+=1
    ) else (
        echo  %%V: FEHLT  [output\%%V\BafToolkit.dlu]
        set /a MISSING+=1
    )
)
echo.

if !FOUND!==0 (
    echo FEHLER: Keine DLUs in output\. Zuerst BAUE_ALLE.bat ausfuehren.
    goto :fail
)
if !MISSING! GTR 0 if not defined BAF_ALLOW_PARTIAL (
    echo FEHLER: !MISSING! Max-Version^(en^) fehlen. Ein Release soll alle
    echo zwoelf enthalten. Bewusst ohne:  set BAF_ALLOW_PARTIAL=1
    goto :fail
)

copy /Y "%SCRIPTS%\BafImport.mcr" "%STAGE%\Contents\MacroScripts\" >nul || goto :fail
copy /Y "%SCRIPTS%\BafExport.mcr" "%STAGE%\Contents\MacroScripts\" >nul || goto :fail

REM ---- VC++-Laufzeit von Microsoft ----
REM Permalink laut learn.microsoft.com/cpp/windows/latest-supported-vc-redist
echo Lade VC++-Laufzeit von Microsoft...
mkdir "%DIST%\redist" >nul 2>&1
curl -sSfL -o "%REDIST%" https://aka.ms/vc14/vc_redist.x64.exe
if errorlevel 1 (
    echo FEHLER: Download fehlgeschlagen.
    goto :fail
)

REM Signatur und Version pruefen. PSModulePath wird zurueckgesetzt:
REM aus PowerShell 7 heraus gestartet findet Windows PowerShell 5
REM sonst seine eigenen Module nicht (geerbter Pfad).
powershell -NoProfile -Command ^
  "$env:PSModulePath = [Environment]::GetEnvironmentVariable('PSModulePath','Machine');" ^
  "$s = Get-AuthenticodeSignature -LiteralPath $env:REDIST;" ^
  "$v = [version](Get-Item -LiteralPath $env:REDIST).VersionInfo.ProductVersion;" ^
  "Write-Host ('  Signatur: ' + $s.Status + ' - ' + $s.SignerCertificate.Subject);" ^
  "Write-Host ('  Version:  ' + $v);" ^
  "if ($s.Status -ne 'Valid' -or $s.SignerCertificate.Subject -notmatch 'O=Microsoft Corporation') { exit 2 };" ^
  "if ($v -lt [version]'14.50') { exit 3 }"
if errorlevel 1 (
    echo FEHLER: vc_redist.x64.exe ist nicht gueltig von Microsoft signiert
    echo oder aelter als 14.50.
    goto :fail
)
echo.

REM ---- Installer ----
echo Baue Installer...
set "SIGNARG="
if defined BAF_SIGNTOOL set "SIGNARG=/DSignToolName=%BAF_SIGNTOOL%"

"%BAF_ISCC%" /Q /DAppVersion=%VER% "/DPackageDir=%STAGE%" "/DVCRedist=%REDIST%" %SIGNARG% installer\BafToolkit.iss
if errorlevel 1 (
    echo FEHLER: Inno Setup ist gescheitert.
    goto :fail
)
echo  OK: dist\BafToolkit-%VER%-Setup.exe
echo.

REM ---- ZIP ----
echo Baue ZIP...
mkdir "%ZIPDIR%" >nul 2>&1
xcopy /E /I /Y /Q "%STAGE%" "%ZIPDIR%\BafToolkit\" >nul
copy /Y installer\Install.bat   "%ZIPDIR%\" >nul
copy /Y installer\Uninstall.bat "%ZIPDIR%\" >nul
copy /Y installer\README.txt    "%ZIPDIR%\" >nul
copy /Y LICENSE                 "%ZIPDIR%\LICENSE.txt" >nul

REM tar ist seit Windows 10 1803 dabei und schreibt echte ZIPs
REM mit Vorwaertsschraegstrichen - anders als Compress-Archive
REM aus Windows PowerShell 5.
pushd "%ZIPDIR%"
tar -a -cf "%DIST%\BafToolkit-%VER%.zip" BafToolkit Install.bat Uninstall.bat README.txt LICENSE.txt
set "TARERR=!errorlevel!"
popd
if not "!TARERR!"=="0" (
    echo FEHLER: ZIP konnte nicht erstellt werden.
    goto :fail
)
echo  OK: dist\BafToolkit-%VER%.zip
echo.

REM ---- Pruefsummen ----
REM certutil statt Get-FileHash, aus demselben Grund wie oben.
REM Die Leerzeichen, die aeltere certutil-Versionen zwischen die
REM Bytes setzen, werden entfernt.
(for %%F in ("%DIST%\*.exe" "%DIST%\*.zip") do (
    set "H="
    for /f "skip=1 delims=" %%H in ('certutil -hashfile "%%~fF" SHA256') do if not defined H set "H=%%H"
    set "H=!H: =!"
    echo !H!  %%~nxF
)) > "%DIST%\SHA256SUMS.txt"
type "%DIST%\SHA256SUMS.txt"
echo.

rmdir /s /q "%DIST%\stage"  >nul 2>&1
rmdir /s /q "%DIST%\redist" >nul 2>&1
rmdir /s /q "%ZIPDIR%"      >nul 2>&1

echo ========================================
echo  Fertig. Dateien in dist\
echo ========================================
echo.
echo Hochladen:
echo   gh release create v%VER% --title "BAF Toolkit %VER%" dist\BafToolkit-%VER%-Setup.exe dist\BafToolkit-%VER%.zip dist\SHA256SUMS.txt
echo.
if not defined BAF_NOPAUSE pause
exit /b 0

:fail
echo.
if not defined BAF_NOPAUSE pause
exit /b 1
