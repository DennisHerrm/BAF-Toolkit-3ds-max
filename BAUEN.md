# BAF Toolkit - Bauen, Pruefen, Entwickeln

Importiert die Charaktere (Modell, Skelett, Texturen, Animationen) aus
`big.lab` direkt in 3ds Max. C++-Parser, MAXScript-Oberflaeche, Aufbau nach
dem Vorbild von XFBIN Import.

**Stand: 1.8.3** - Import und Export vollstaendig, Topologie umbaubar.
Siehe `DEVNOTES.md` fuer Formatabbildung, Stufenplan und Codierregeln.

## Inhalt

| Datei | |
|---|---|
| `src/baf_lab.*` | `.lab`-Archiv, nur lesend |
| `src/baf_reader.*` | `.baf`-Parser, haengt nicht am Max SDK |
| `src/baf_writer.cpp` | `.baf`-Schreiber, patcht statt neu zu bauen |
| `tools/bafdump_main.cpp` | eigenstaendiges `bafdump.exe` |
| `tools/pydump.py` | Python-Referenz, gleiche Ausgabe |
| `src/baftoolkit.*` | das Max-Plugin, `BafCpp.*` in MAXScript |
| `scripts/BafImport.mcr` | Oberflaeche zum Importieren |
| `scripts/BafExport.mcr` | Oberflaeche zum Exportieren |
| `package/BafImport/` | PackageContents.xml fuer ApplicationPlugins |
| `BAUE_ALLE.bat` | baut Werkzeug und alle Max-Versionen 2016-2027 |
| `INSTALLIERE.bat` | Paket bauen und nach ApplicationPlugins kopieren |
| `DEINSTALLIERE.bat` | Paket wieder entfernen |
| `VERGLEICHE.bat` | Gegenprobe C++ gegen Python |
| `tools/PRUEFE_API.py` | prueft die drei Eintragungsstellen der API |
| `tools/PRUEFE_SCRIPTS.py` | Linter fuer die MaxScript-Dateien |
| `tools/PRUEFE_PAKET.py` | prueft PackageContents.xml gegen die Autodesk-Spezifikation |
| `tools/PRUEFE_INCLUDES.py` | benutztes SDK-Symbol ohne seinen Header |
| `tools/PRUEFE_SDK.py` | SDK-Eigenheiten, die der Compiler erst spaet meldet |
| `tools/uebersetze.py` | Tabelle der Meldungstexte, deutsch -> englisch |
| `tools/BAF_TEST.ms` | im Listener durchspielen, ohne Oberflaeche |

## Bauen

    BAUE_ALLE.bat

Ohne Argument: erst das Werkzeug, dann jede Max-Version, fuer die ein SDK
installiert ist. Eine einzelne Version:

    BAUE_ALLE.bat 2025

Vor einem Build lohnen sich:

    python tools\PRUEFE_API.py
    python tools\PRUEFE_SCRIPTS.py
    python tools\PRUEFE_PAKET.py
    python tools\PRUEFE_INCLUDES.py
    python tools\PRUEFE_SDK.py

Das erste prueft, ob jede Plugin-Funktion an allen drei noetigen Stellen
eingetragen ist. Fehlt die dritte, ist sie bei MaxScript nicht angemeldet -
und kein Neubau behebt das. Das zweite prueft die Skripte auf die Fehlerarten,
die sonst erst in Max auffallen. Das dritte prueft `PackageContents.xml` gegen
die Autodesk-Spezifikation und vergleicht nebenbei die Versionsnummer in
`bafimport.h`, `BafImport.mcr` und dem Paket.

## Installieren

    INSTALLIERE.bat

Baut die Paketstruktur aus `output\` und `scripts\` und kopiert sie nach

    %APPDATA%\Autodesk\ApplicationPlugins\BafImport

Kein Admin noetig. Danach 3ds Max starten:

    Datei  ->  Importieren   ->  Big Ape Actor (*.baf, *.lab)
    Datei  ->  Exportieren   ->  Big Ape Actor (*.baf)

Beim Import laesst sich entweder eine einzelne `.baf` waehlen oder gleich das
Archiv `big.lab` - dann kommt die Modellliste.

Der Export ist ein eigenes Fenster und braucht keinen vorherigen Import in
derselben Sitzung: er untersucht die Szene, findet ihre Herkunft in den
Rohdaten an den Objekten und laedt die Vorlage selbst nach. Bei einem eigenen
Modell waehlt man die Vorlage - Kopf, Palette und Materialien kommen von dort,
Skelett und Geometrie aus der Szene.

Das Makro steht ausserdem unter Customize -> Customize User Interface ->
Kategorie "DH Tools", fuer Toolbar oder Shortcut.

Der Installer entfernt dabei eine eventuell vorhandene alte Fassung des Pakets
(`BafImport`, bis 1.3.1). Bleiben beide liegen, laedt Max beide DLLs, und die
zweite meldet dieselbe Schnittstelle mit derselben ID an - Max bricht dann
beim Start ab.

Wieder loswerden: `DEINSTALLIERE.bat`.

Beide Skripte pruefen vorher, ob eine installierte `.dlu` noch gesperrt ist -
das heisst fast immer, dass Max noch laeuft.

### Testen, ohne zu installieren

Ab Max 2019 geht das ueber eine Umgebungsvariable, die auf den Paketordner
zeigt - dann bleibt `ApplicationPlugins` unberuehrt:

    set ADSK_APPLICATION_PLUGINS=%CD%\package
    "C:\Program Files\Autodesk\3ds Max 2027\3dsmax.exe"

`package` ist hier der ELTERNORDNER: Max nimmt einen Pfad, unter dem ein
Paketordner liegt, und laedt jedes Paket darin. Praktisch, um eine frisch
gebaute `.dlu` auszuprobieren, ohne die installierte anzufassen.

Meldet Max beim Start einen Paketfehler, steht der Grund im `max.log`.

Steuerbar ueber Umgebungsvariablen:

    set BAF_STRICT=OFF        REM /W3 statt /W4
    set STOP_ON_ERROR=0       REM trotz Fehler alle Versionen durchbauen
    set BAF_GENERATOR=Visual Studio 17 2022

## Gegenprobe

    VERGLEICHE.bat "D:\Episode1\big.lab" "D:\addons\io_scene_baf"

Dumpt alle Modelle zweimal und vergleicht. Geprueft: 303 Modelle,
655.923 Zeilen, 0 Abweichungen.

### In Max

Ueber *Datei -> Importieren* `big.lab` waehlen, ein Modell anklicken, einen
Ordner fuer die Texturen setzen, dann "Alles bauen".

Danach die beiden Gegenproben. Jede schreibt zwei Dateien - eine aus den
Dateidaten, eine aus der gebauten Szene:

    fc /L anakin_soll.txt       anakin_ist.txt         (Ruhelage)
    fc /L anakin_geo_soll.txt   anakin_geo_ist.txt     (Geometrie)
    fc /L anakin_anim_soll.txt  anakin_anim_ist.txt    (Animation)

Sind sie gleich, steht in Max das, was in der Datei steht - unabhaengig davon,
ob es im Viewport "richtig aussieht".

Fuer den vollen Datenvergleich gibt es weiterhin "Dump schreiben".

### Wenn etwas schiefgeht

Das Plugin protokolliert durchgehend mit, auch ohne dass man es einschaltet.
"Protokoll speichern" schreibt es heraus; der Haken daneben macht es
ausfuehrlicher (jeder Bone, jedes Mesh, jeder Materialplatz einzeln).

    BafCpp.setDebug 1
    BafCpp.log()
    BafCpp.saveLog @"D:\tmp\baf_log.txt"

Texturen lassen sich auch ohne Max ansehen:

    bafdump.exe big.lab anakin.baf D:\tmp\tga --tga Der Dump laesst sich Zeile fuer Zeile gegen die
Python-Referenz halten:

    fc /L max_anakin.txt dump_py\anakin.baf.txt

Damit ist geprueft, dass der Parser IM MAX-PROZESS dasselbe liest wie
ausserhalb. Wer das ohne Oberflaeche will, nimmt `tools\BAF_TEST.ms`.

### Rundlauf

    bafdump.exe big.lab --rundlauf

Liest jedes Modell, schreibt es wieder und vergleicht die Bytes. Ohne
Aenderung muss die Datei dieselbe sein. Stand: 303 byte-gleich, 0 abweichend.

Einzeln geht auch:

    output\tools\bafdump.exe big.lab                    REM Archiv auflisten
    output\tools\bafdump.exe big.lab anakin.baf         REM Zusammenfassung
    output\tools\bafdump.exe big.lab anakin.baf a.txt --all

## Lizenz

Der Parser ist aus den Retail-Daten und dem Blender-Addon `io_scene_baf`
(OpenPhantom, MIT) hergeleitet. Es werden keine Spieldaten mitgeliefert.
Star Wars ist eine Marke der jeweiligen Rechteinhaber; dies ist ein
inoffizielles Fanprojekt.
