# -*- coding: utf-8 -*-
# ============================================================
#  uebersetze.py - die Meldungen des Plugins ins Englische
#
#  Einmalig benutzt fuer 1.7.0. Bleibt liegen, weil die Tabelle
#  zeigt, welche Formulierung wofuer steht - wer eine neue
#  Meldung schreibt, findet hier den Ton.
#
#  Reihenfolge zaehlt: laengere Muster stehen vor kuerzeren,
#  sonst zerlegt " Knoten" das laengere " Knoten, Faktor ".
# ============================================================
import io
import re
import sys

PAIRS = [
    # --- lange Meldungstexte zuerst ---
    ("Zum Exportieren muss das Modell im Plugin geladen sein.",
     "Exporting needs the model loaded in the plugin."),
    ("Der Schreiber setzt die Aenderungen in die urspruenglichen Bytes ",
     "The writer patches the changes back into the original bytes of "),
    ("der .baf zurueck. Ohne diese Vorlage gibt es nichts, worin er ",
     "the .baf. Without that template there is nothing for it to "),
    ("schreiben koennte - die Szene allein genuegt nicht.",
     "write into - the scene alone is not enough."),
    ("Also in dieser Reihenfolge:", "So, in this order:"),
    ("  1. Datei > Importieren, das Modell laden",
     "  1. File > Import, load the model"),
    ("  2. in der Szene aendern", "  2. change it in the scene"),
    ("  3. Datei > Exportieren", "  3. File > Export"),
    ("Diese Szene kennt ihre Herkunft nicht - sie kam entweder nicht ",
     "This scene does not know its origin - either it did not come "),
    ("aus einem BAF-Import, oder das Archiv liegt nicht mehr an ",
     "from a BAF import, or the archive is no longer where "),
    ("seinem Platz.", "it used to be."),
    ("Das Exportfenster geht gleich auf. Dort laesst sich eine Vorlage ",
     "The export window is about to open. A template can be picked "),
    ("von Hand waehlen: Kopf, Palette und Materialien kommen von dort, ",
     "by hand there: header, palette and materials come from it, "),
    ("Skelett und Geometrie aus der Szene.",
     "skeleton and geometry from the scene."),
    ("Eine .baf aus einer voellig neuen Szene zu bauen ist etwas ",
     "Building a .baf from a completely new scene is a different "),
    ("anderes und noch nicht eingebaut.", "matter and not built in yet."),

    ("WARNUNG: Rohdaten werden NICHT gespeichert. Ein spaeterer Export ",
     "WARNING: the raw data is NOT stored. A later export "),
    ("kann die Datei dann nicht mehr rekonstruieren.",
     "will then not be able to reconstruct the file."),
    ("WARNUNG: Huellkugeln werden nachgerechnet. Alle 3.131 mitgelieferten ",
     "WARNING: bounding spheres get recomputed. All 3,131 shipped "),
    ("Kugeln sind groesser als die eng anliegende - eine nachgerechnete ",
     "spheres are larger than the tightly fitting one, so recomputing "),
    ("schrumpft sie, und am Kopfknoten friert dann die Blickrichtung der KI ein.",
     "shrinks them - and on the head node that freezes the AI look-at direction."),
    ("  Kopfknoten: Huellkugel BEIBEHALTEN trotz Nachrechnen-Schalter. ",
     "  head node: bounding sphere KEPT despite the recompute switch. "),
    ("Eine geschrumpfte Kugel am Kopf laesst die Blickrichtung der KI ",
     "A shrunken sphere on the head freezes the AI look-at "),
    ("einfrieren.", "direction."),

    ("Keine BAF-Rohdaten in der Szene. Fuer den Export wird eine ",
     "No BAF raw data in the scene. Exporting needs a "),
    ("Vorlage gebraucht.", "template."),
    ("Herkunft unbekannt - der Modellname fehlt in den Rohdaten",
     "origin unknown - the model name is missing from the raw data"),
    ("Kein Mesh in der Szene - das Material haengt an nichts",
     "no mesh in the scene - the material hangs off nothing"),

    ("Rig steht in der Ausrichtung der Datei (Blick nach +Y)",
     "rig keeps the orientation of the file (looking along +Y)"),
    ("Rig wird der Frontansicht zugewandt (180 Grad um Z)",
     "rig turned to face the front view (180 degrees about Z)"),
    ("Umbau der Topologie ERLAUBT", "topology rebuild ALLOWED"),
    ("Umbau der Topologie gesperrt (nur an Ort und Stelle)",
     "topology rebuild blocked (in place only)"),
    ("Vertexnormalen der Datei uebernehmen",
     "use the vertex normals from the file"),
    ("Vertexnormalen von Max berechnen lassen",
     "let Max compute the vertex normals"),
    ("Modus: nur die Keys der Datei uebernehmen",
     "mode: take only the keys from the file"),
    ("Modus: jeden Frame backen", "mode: bake every frame"),

    # --- Fehlermeldungen der API ---
    ("keine Core-Schnittstelle", "no core interface"),
    ("kein Modell geoeffnet", "no model open"),
    ("kein Archiv geoeffnet", "no archive open"),
    ("Modell hat keine Knoten", "the model has no nodes"),
    ("Modell hat keine Meshes", "the model has no meshes"),
    ("Modell hat keine Clips", "the model has no clips"),
    ("addClip: fps muss groesser als null sein",
     "addClip: fps must be greater than zero"),
    ("addClip: mindestens ein Frame", "addClip: at least one frame"),
    ("Clipnummer ausserhalb", "clip number out of range"),
    ("buildMaterials: MultiMtl liess sich nicht anlegen",
     "buildMaterials: could not create the MultiMtl"),
    ("buildMaterials: keine Materialplaetze", "buildMaterials: no material slots"),
    ("buildMaterials: mehr Materialplaetze als Max-IDs",
     "buildMaterials: more material slots than Max IDs"),
    ("buildSequence: erst buildSkeleton aufrufen",
     "buildSequence: call buildSkeleton first"),
    ("captureAnim: kein Skelett in der Szene - erst scanScene",
     "captureAnim: no skeleton in the scene - run scanScene first"),
    ("exportFile: unvollstaendig geschrieben - Platte voll?",
     "exportFile: written incompletely - disk full?"),
    ("exportTextures: kein Texturordner - und es liess sich ",
     "exportTextures: no texture folder, and none could "),
    ("auch keiner anlegen", "be created either"),
    ("exportToArchive: dieses Modell kam nicht aus dem Archiv - ",
     "exportToArchive: this model did not come from the archive - "),
    ("bitte exportFile benutzen", "please use exportFile"),
    ("sceneAnimDump: dieser Clip ist nicht in der Szene",
     "sceneAnimDump: that clip is not in the scene"),
    ("sceneBoneDump: kein Skelett gebaut", "sceneBoneDump: no skeleton built"),
    ("sceneMeshDump: keine Meshes gebaut", "sceneMeshDump: no meshes built"),
    ("verifyRoundTrip: die Quellbytes sind nicht bekannt",
     "verifyRoundTrip: the source bytes are not known"),
    ("autoLoadTemplate: die Szene kennt ihre Herkunft nicht",
     "autoLoadTemplate: the scene does not know its origin"),
    ("autoLoadTemplate: die Szene nennt kein Archiv",
     "autoLoadTemplate: the scene names no archive"),
    ("Export: kein Modell geoeffnet.", "Export: no model open."),
    ("kein Pfad", "no path"),
    ("kein Name", "no name"),
    ("zu gross fuer einen 32-Bit-Groessenwert",
     "too large for a 32-bit size field"),

    # --- Protokollzeilen ---
    ("Archiv geoeffnet: ", "archive opened: "),
    ("Datei geoeffnet: ", "file opened: "),
    ("Modell geoeffnet: ", "model opened: "),
    ("Modell geschlossen", "model closed"),
    ("Archiv geschlossen", "archive closed"),
    ("Materialplaetze: ", "material slots: "),
    ("Ausfuehrliches Protokoll AN", "verbose log ON"),
    ("Ausfuehrliches Protokoll AUS", "verbose log OFF"),
    ("[... aelterer Teil des Protokolls entfernt ...]",
     "[... older part of the log dropped ...]"),
    ("vorher geraeumt: ", "cleared first: "),
    ("noch nicht untersucht", "not scanned yet"),
    ("noch nichts uebernommen", "nothing taken over yet"),
    ("buildSequence FEHLER: ", "buildSequence FAILED: "),
    ("Texturordner (automatisch): ", "texture folder (automatic): "),
    ("Texturordner: ", "texture folder: "),
    ("Animationsbereich: 0 bis ", "animation range: 0 to "),
    ("byte-gleich mit dem Original", "byte for byte identical to the original"),
    ("erste Abweichung bei ", "first difference at "),
    ("erste Abweichung bei Offset ", "first difference at offset "),
    ("ist bereits geladen", "is already loaded"),
    ("Byte, ", "bytes, "),
    ("Byte nach ", "bytes to "),
    ("neu gebaut: ", "rebuilt: "),
    ("Anfang", "start"),

    # --- Warnungen ---
    (" Bones liessen sich nicht anlegen, zuerst ",
     " bones could not be created, the first being "),
    (" Bones sind nicht mehr in der Szene", " bones are no longer in the scene"),
    (" Meshes sind nicht mehr in der Szene", " meshes are no longer in the scene"),
    (" Meshes gehoeren zu keinem Knoten", " meshes belong to no node"),
    (" doppelte Knotennamen", " duplicate node names"),
    (" entartete Faces uebersprungen (Vertex doppelt)",
     " degenerate faces skipped (repeated vertex)"),
    (" Faces ohne gueltiges Material, unbetextert geschrieben",
     " faces with no valid material, written untextured"),
    (" Tracks ohne Bone in der Szene - unveraendert gelassen",
     " tracks with no bone in the scene - left untouched"),
    (" weitere Objekte ohne Rohdaten (werden nicht ",
     " further objects without raw data (not "),
    ("angefasst)", "touched)"),
    (" Engine-Grenze(n) ueberschritten - der Loader des Spiels ",
     " engine limit(s) exceeded - the loader of the game "),
    ("lehnt das Modell so ab", "will reject the model like this"),
    (" Grenze(n) ueberschritten", " limit(s) exceeded"),
    ("Vertices, die Engine nimmt ", "vertices, the engine takes "),
    ("Faces, die Engine nimmt ", "faces, the engine takes "),
    ("Texturvertices, die Engine nimmt ", "texture vertices, the engine takes "),

    # --- Satzbausteine, absteigend nach Laenge ---
    (" Knoten, Faktor ", " nodes, scale "),
    (" Meshes uebernommen, ", " meshes taken over, "),
    (" Keys aus der Szene uebernommen", " keys taken from the scene"),
    (" Frames ab Szenenframe ", " frames from scene frame "),
    (" Tracks ohne Keys", " tracks without keys"),
    ("Bones, ", "bones, "),
    (" Objekte entfernt", " objects removed"),
    (" Knoten, ", " nodes, "),
    (" Knoten", " nodes"),
    (" Eintraege, ", " entries, "),
    (" Modelle", " models"),
    (" Plaetze, ", " slots, "),
    (" uebersprungen, ", " skipped, "),
    (" uebersprungen", " skipped"),
    (" Zeilen nach ", " lines to "),
    (" Zeilen, ", " lines, "),
    (" Dreiecke", " triangles"),
    (" fehlend, nach ", " missing, to "),
    (" geschrieben, ", " written, "),
    (" Texturen", " textures"),
    (" Meshes nach ", " meshes to "),
    ("Sicherung angelegt: ", "backup created: "),
    ("Sicherung vorhanden, bleibt unberuehrt: ",
     "backup already there, left untouched: "),
    ("An Ort und Stelle ersetzt: ", "replaced in place: "),
    ("Archiv neu aufgebaut: ", "archive rebuilt: "),
    ("Gegenprobe bestanden", "verification passed"),
    ("Aus der Sicherung wiederhergestellt: ", "restored from the backup: "),
    ("Keine Sicherung gefunden: ", "no backup found: "),

    # --- Wortreste in zusammengesetzten Meldungen ---
    (" an Knoten ", " on node "),
    (" ist kein Mesh mehr", " is no longer a mesh"),
    (" ist nicht im Archiv", " is not in the archive"),
    (" ist nicht in ", " is not in "),
    (" laesst sich nicht oeffnen. Bitte die Vorlage selbst waehlen.",
     " cannot be opened. Please pick the template by hand."),
    (" laesst sich nicht schreiben", " cannot be written"),
    (" laesst sich nicht zum Aendern oeffnen",
     " cannot be opened for modification"),
    (": Knoten liess sich nicht anlegen", ": could not create the node"),
    (": TriObject liess sich nicht anlegen", ": could not create the TriObject"),
    (": Topologie geaendert (", ": topology changed ("),
    (") - uebersprungen. Mit setRebuild 1 wird es ",
     ") - skipped. With setRebuild 1 it is "),
    ("stattdessen neu gebaut.", "rebuilt instead."),
    (": nicht dekodierbar (Bittiefe ", ": cannot be decoded (bit depth "),
    ("leeres Mesh", "empty mesh"),
    ("Bones, ", "bones, "),
    # --- Nachtrag: was der erste Filter uebersehen hat ---
    ("Schreiben abgebrochen - Platte voll?", "write aborted - disk full?"),
    ("Schreiben abgebrochen", "write aborted"),
    ("erst buildSkeleton aufrufen", "call buildSkeleton first"),
    ("erst openArchive aufrufen", "call openArchive first"),
    ("autoLoadTemplate: das Archiv ", "autoLoadTemplate: the archive "),
    ("dump geschrieben: ", "dump written: "),
    ("Skelett gebaut: ", "skeleton built: "),
    ("Meshes gebaut: ", "meshes built: "),
    (" Meshes mit Rohdaten gefunden", " meshes with raw data found"),
    (" Frames bei ", " frames at "),
    (" mit Textur, ", " with a texture, "),
    (", mit Transparenz", ", with transparency"),
    (" neu gebaut, ", " rebuilt, "),
    (" neu gebaut:", " rebuilt:"),
    ("gelesen: ", "read: "),
    (" nach ", " to "),
    (" von ", " of "),
]


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else 'src/baftoolkit.cpp'
    text = io.open(path, encoding='utf-8').read()

    # Nur INNERHALB von Zeichenketten ersetzen. Sonst zerlegt die
    # Tabelle die Kommentare gleich mit - und die sollen deutsch
    # bleiben, sie sind die Entwicklungsnotizen.
    def one(match):
        body = match.group(1)
        for german, english in PAIRS:
            body = body.replace(german, english)
        return 'L"' + body + '"'

    out = re.sub(r'L"([^"]*)"', one, text)
    io.open(path, 'w', encoding='utf-8').write(out)

    rest = sorted(set(
        t for t in re.findall(r'L"([^"]*)"', out)
        if re.search(r'[\u00e4\u00f6\u00fc\u00df]|aeh|oeff|ueber|Datei|Modell'
                     r'|Szene|Knoten|nicht|keine|kein |wurde|werden', t)))
    print('%d Zeichenketten noch deutschverdaechtig' % len(rest))
    for t in rest:
        print('   %r' % t)


if __name__ == '__main__':
    main()
