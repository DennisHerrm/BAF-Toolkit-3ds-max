// ============================================================
//  BAF Toolkit - Header
//  Version 1.8.3
//
//  MaxScript-API - alles unter BafCpp.*:
//
//  Archiv:
//    BafCpp.openArchive  <pfad>      -> int  Anzahl .baf-Modelle, -1 = Fehler
//    BafCpp.closeArchive()           -> int  1
//    BafCpp.archiveOpen()            -> int  0/1
//    BafCpp.archivePath()            -> string
//    BafCpp.entryCount()             -> int  ALLE Eintraege, nicht nur Modelle
//    BafCpp.modelCount()             -> int  nur die .baf
//    BafCpp.modelNames()             -> string  "\n"-getrennt, alphabetisch
//
//  Modell:
//    BafCpp.openModel    <name>      -> int  Anzahl Knoten, -1 = Fehler
//        Liest ein Modell AUS DEM GEOEFFNETEN ARCHIV.
//    BafCpp.openFile     <pfad>      -> int  dasselbe fuer eine lose .baf
//    BafCpp.close()                  -> int  1
//    BafCpp.isOpen()                 -> int  0/1
//    BafCpp.summary()                -> string
//    BafCpp.dump <pfad> <detail>     -> int  1 = geschrieben
//        detail: 0 = nur Zaehlwerte, 1 = Vertices, 2 = Keys, 3 = beides
//
//  Zaehlwerte des geoeffneten Modells:
//    BafCpp.modelName()              -> string
//    BafCpp.modelScale()             -> float
//    BafCpp.nodeCount()              -> int
//    BafCpp.meshCount()              -> int
//    BafCpp.materialCount()          -> int
//    BafCpp.clipCount()              -> int
//    BafCpp.vertexCount()            -> int
//    BafCpp.faceCount()              -> int
//    BafCpp.keyCount()               -> int
//
//  Einzelne Knoten und Clips - fuer die Oberflaeche, damit sie
//  nicht den ganzen Dump durchsuchen muss:
//    BafCpp.nodeName     <index>     -> string  (0-basiert)
//    BafCpp.nodeParent   <index>     -> int     -1 = Wurzel
//    BafCpp.nodeMesh     <index>     -> int     -1 = keine Geometrie
//    BafCpp.clipName     <index>     -> string
//    BafCpp.clipFps      <index>     -> float
//    BafCpp.clipFrames   <index>     -> int
//    BafCpp.clipKeys     <index>     -> int
//
//  Szene (Stufe 2):
//    BafCpp.setScale     <float>     -> float  Faktor fuer alle Laengen
//    BafCpp.setBoneSize  <float>     -> float  Breite/Hoehe der Bone-Kaesten
//    BafCpp.buildSkeleton()          -> int    Anzahl angelegter Bones, 0 = Fehler
//    BafCpp.clearScene()             -> int    loescht NUR die selbst angelegten
//    BafCpp.sceneBoneCount()         -> int
//    BafCpp.buildMeshes()            -> int    Anzahl angelegter Objekte
//    BafCpp.sceneMeshCount()         -> int
//    BafCpp.restDump     <pfad>      -> int    Ruhelage aus den DATEIDATEN
//    BafCpp.sceneBoneDump <pfad>     -> int    Ruhelage aus der SZENE
//    BafCpp.buildDump    <pfad>      -> int    Geometrie aus den DATEIDATEN
//    BafCpp.sceneMeshDump <pfad>     -> int    Geometrie aus der SZENE
//
//  Texturen und Materialien (Stufe 4):
//    BafCpp.setTextureDir <pfad>     -> string  Zielordner fuer die TGA
//    BafCpp.exportTextures()         -> int     Anzahl geschriebener Dateien
//    BafCpp.buildMaterials()         -> int     Anzahl Materialplaetze
//    BafCpp.slotCount()              -> int
//    BafCpp.slotInfo     <index>     -> string  eine Zeile je Platz
//
//  Die beiden letzten schreiben dasselbe Format. Genau das ist
//  die Gegenprobe fuer Stufe 2:
//
//    BafCpp.restDump      @"D:\tmp\soll.txt"
//    BafCpp.buildSkeleton()
//    BafCpp.sceneBoneDump @"D:\tmp\ist.txt"
//    -> fc /L soll.txt ist.txt
//
//  Stimmt das ueberein, sitzt jeder Bone da, wo die Datei ihn
//  haben will - unabhaengig davon, ob das Modell im Viewport
//  "richtig aussieht".
//
//  Export (Stufe 7):
//    BafCpp.applyScene()             -> int    uebernommene Objekte, -1 = Fehler
//    BafCpp.captureAnim  <clip>      -> int    Keys aus der Szene, -1 = Fehler
//    BafCpp.setKeyTolerance <grad>   -> float  0 = jeden Frame behalten
//    BafCpp.sceneIsAnimated()        -> int    Bones mit Keys in der Szene
//    BafCpp.setForceRestPose <0|1>   -> int    Standard 0
//
//  Zu sceneIsAnimated und der Sperre dahinter:
//
//  applyScene liest die RUHELAGE der Bones bei FRAME 0. Das ist
//  richtig, solange dort auch die Ruhelage steht - und falsch,
//  sobald jemand animiert hat. Bei einer Animation von Frame 0
//  bis 26 ist Frame 0 der ERSTE FRAME DER BEWEGUNG, nicht die
//  Ruhelage.
//
//  Genau das ist passiert: 17 von 48 Knoten bekamen eine neue
//  Ruhelage, der linke Oberarm um 70 Grad verdreht. Danach lief
//  im Spiel KEINE Animation mehr - die Ruhelage ist das
//  Fundament fuer jeden Knoten, den ein Clip nicht anfasst.
//
//  applyScene prueft deshalb, ob die Bones Keys tragen. Wenn ja,
//  bleibt die Ruhelage unangetastet und es gibt eine Warnung.
//  setForceRestPose 1 hebt die Sperre auf - fuer den seltenen
//  Fall, dass wirklich eine neue Ruhelage gemeint ist.
//    BafCpp.addClip <name> <fps> <frames> -> int  neue Clipnummer, -1 = Fehler
//    BafCpp.deleteClip   <clip>      -> int    1 = geloescht
//    BafCpp.setClipInfo <clip> <name> <fps> <frames> -> int
//
//  addClip legt einen LEEREN Clip an - Tracks fuer alle Knoten,
//  aber ohne Keys. Die kommen mit captureAnim aus der Szene.
//
//  Die undurchsichtigen Teile des Clip-Satzes werden von einem
//  vorhandenen Clip GEKLONT, nicht erfunden. Es sind genau drei
//  Felder, und alle drei sind alte Zeiger aus dem Prozess von
//  1999 (Descriptor+0x04, Key-Header+0x3c, Track+0x28). Die
//  Engine schreibt sie beim Laden ohnehin neu; sie stehen zu
//  lassen ist trotzdem richtiger, als eine Null hineinzusetzen,
//  von der niemand weiss, ob sie eine Bedeutung hat.
//    BafCpp.pendingExportPath()      -> string vom Export-Dialog, einmalig
//
//  captureAnim ist das Gegenstueck zu buildAnim: es liest die
//  Bewegung der Bones aus der Szene ZURUECK in einen Clip.
//  applyScene fasst die Animation NICHT an - es nimmt nur die
//  Ruhelage und die Geometrie. Wer eine eigene Animation
//  exportieren will, braucht captureAnim.
//    BafCpp.exportFile   <pfad>      -> int    1 = geschrieben
//    BafCpp.verifyRoundTrip()        -> int    1 = byte-gleich mit dem Original
//    BafCpp.exportReport()           -> string was uebernommen wurde
//
//    BafCpp.exportToArchive()        -> int    1 = ins Archiv geschrieben
//    BafCpp.putFile <eintrag> <pfad> -> int    1 = Datei ins Archiv gelegt
//
//  putFile legt eine fertige .baf von der Platte in das Archiv,
//  ohne sie durch Import und Export zu schicken. Gedacht zum
//  Eingrenzen: eine unveraenderte Datei hineinlegen und im Spiel
//  sehen, ob es am Schreibweg liegt oder am Inhalt. Ohne so
//  einen Knopf braucht es dafuer einen Hexeditor.
//    BafCpp.restoreArchive()         -> int    1 = Sicherung zurueckgespielt
//
//  Der Weg ist:  applyScene -> exportFile  (lose Datei)
//           oder applyScene -> exportToArchive  (zurueck ins big.lab)
//
//  exportToArchive legt beim ERSTEN Mal eine Sicherung .bak an
//  und laesst sie danach in Ruhe - eine zweite wuerde genau den
//  Stand ueberschreiben, zu dem man zurueckwill. Nach dem
//  Schreiben wird jeder Eintrag wieder eingelesen und gegen das
//  gehalten, was dastehen soll; erst dann gilt es als getan.
//
//  applyScene liest Bones und Meshes aus der Szene ZURUECK in das
//  geladene Modell. exportFile schreibt es heraus. Wer nichts
//  geaendert hat und exportFile ohne applyScene aufruft, bekommt
//  eine Datei, die BYTE-GLEICH mit dem Original ist - das prueft
//  verifyRoundTrip nach.
//
//  Die Szene untersuchen (Stufe 11):
//    BafCpp.scanScene()              -> int    gefundene Objekte mit Rohdaten
//    BafCpp.sceneOrigin()            -> string Herkunft, "" = unbekannt
//    BafCpp.sceneArchive()           -> string Archivpfad aus der Szene
//    BafCpp.sceneReport()            -> string Befund im Klartext
//    BafCpp.sceneClip()              -> string zuletzt angewandter Clip
//
//  Zu sceneClip: buildAnim schreibt den Clipnamen an den ersten
//  Bone. Damit weiss der Export, welchen Clip die Szene gerade
//  zeigt - sonst muss der Anwender ihn in einer Liste mit 114
//  Eintraegen selbst wiederfinden, und ein Griff daneben
//  ueberschreibt den falschen.
//    BafCpp.autoLoadTemplate()       -> int    1 = Vorlage selbst geladen
//
//  Der Sinn: der Export darf NICHT davon abhaengen, dass dieselbe
//  Max-Sitzung vorher importiert hat. Jede importierte Szene
//  traegt ihre Herkunft an den Objekten mit sich - das ueberlebt
//  Speichern und Neustart. scanScene liest sie zurueck und stellt
//  die Zuordnung wieder her, autoLoadTemplate holt die Vorlage
//  nach.
//
//    BafCpp.setRebuild   <0|1>       -> int    Standard 0
//    BafCpp.setRecomputeBounds <0|1> -> int    Standard 0
//
//  applyScene arbeitet standardmaessig AN ORT UND STELLE:
//  Positionen, Normalen und UVs werden uebernommen, die
//  Topologie nicht. Wer in Max Faces hinzufuegt oder loescht,
//  bekommt eine Meldung statt einer stillschweigend falschen
//  Datei.
//
//  Mit setRebuild 1 wird ein Mesh mit geaenderter Topologie
//  stattdessen NEU GEBAUT: Positionen, Normalen, UVs, Faces und
//  der Texturvertex-Vorrat entstehen komplett aus der Szene.
//  Nichts aus den urspruenglichen Arrays bleibt stehen, weil
//  nichts darin noch zu dieser Topologie gehoert. Die
//  undurchsichtigen Wortfelder je Face werden vom ersten Face des
//  Mesh geerbt - naeher an dessen ueblichem Wert als null.
//
//  Zu setRecomputeBounds, und das ist wichtiger als es aussieht:
//  die Huellkugel im Mesh-Satz wird beim Umbau standardmaessig
//  BEIBEHALTEN, nicht nachgerechnet. Nachgemessen an den
//  Retail-Daten ist jede einzelne der 3.131 mitgelieferten Kugeln
//  GROESSER als die eng anliegende, im Schnitt um 16 Prozent, und
//  keine einzige kleiner. Sie sind also mit Absicht weiter
//  gefasst. Nachrechnen wuerde sie alle schrumpfen - und am
//  Kopfknoten ist genau das die bekannte Ursache dafuer, dass die
//  Blickrichtung der KI einfriert.
//
//  Schalter und Berichte (Stufe 6):
//    BafCpp.setImportNormals <0|1>   -> int  Standard 1
//    BafCpp.setHideUndrawn   <0|1>   -> int  Standard 1
//    BafCpp.setStoreProps    <0|1>   -> int  Standard 1
//    BafCpp.setShowBounds    <0|1>   -> int  Standard 0
//    BafCpp.setFaceFront     <0|1>   -> int  Standard 1
//
//  Zu setFaceFront: die Engine schaut nach +Y. Nachgeprueft an
//  der Fussgeometrie - die Zehen liegen bei allen Modellen auf
//  der +Y-Seite des Knoechels. Max' Frontansicht blickt aber
//  ebenfalls nach +Y, also sieht man den Charakter dort von
//  hinten. (Blender genauso; das Addon dreht auch nichts.)
//
//  Mit setFaceFront 1 wird das ganze Rig um 180 Grad um Z
//  gedreht, damit die Figur der Frontansicht zugewandt steht.
//  Beim Zurueckschreiben wird die Drehung wieder herausgerechnet
//  - die Datei bleibt also unveraendert. Der Rundlauf muss auch
//  mit eingeschalteter Drehung byte-gleich bleiben.
//    BafCpp.limitsReport()           -> string
//    BafCpp.limitsExceeded()         -> int  0 = alles im Rahmen
//
//  Zu setStoreProps: das Plugin haengt an jeden Bone und jedes
//  Mesh die Rohdaten der Datei - Wortfelder je Face, Intensitaet
//  je Vertex, Pivot, Huellkugel, Clip-Metadaten, Sound-Events.
//  Sie stehen in 3ds Max unter Objekteigenschaften ->
//  Benutzerdefiniert und lassen sich von Hand ansehen.
//
//  Der Grund, warum das nicht optional gedacht ist: diese Angaben
//  lassen sich SPAETER NICHT REKONSTRUIEREN. Wer heute importiert
//  und in einem halben Jahr exportieren will, hat sie sonst nicht
//  mehr. Der Schalter ist nur da, um sie bei riesigen Szenen
//  weglassen zu koennen.
//
//  Animationen (Stufe 5):
//    BafCpp.setBake      <0|1>       -> int    Standard 1
//    BafCpp.buildAnim    <clip>      -> int    gesetzte Keys, 0 = Fehler
//    BafCpp.buildSequence <abstand>  -> int    Gesamtlaenge in Frames
//    BafCpp.clipStart    <clip>      -> int    Startframe in der Sequenz
//    BafCpp.clipEnd      <clip>      -> int
//    BafCpp.clipSceneFrames <clip>   -> int    Laenge NACH der FPS-Umrechnung
//    BafCpp.clearAnim()              -> int
//    BafCpp.animDump  <clip> <pfad>  -> int    aus den DATEIDATEN
//    BafCpp.sceneAnimDump <clip> <pfad> -> int aus der SZENE
//
//  Zur FPS-Umrechnung: jeder Clip hat SEINE EIGENE Framerate, im
//  Datenbestand von 2 bis 160. Eine Max-Szene hat genau eine.
//  Ohne Umrechnung laeuft jede Animation mit der falschen
//  Geschwindigkeit, und zwar unauffaellig - es sieht nur
//  "irgendwie zaeh" aus.
//
//  Uebergabe aus Max' Dateidialogen:
//    BafCpp.pendingPath()            -> string  einmalig abholbar
//
//  Allgemein:
//    BafCpp.version()                -> string
//    BafCpp.lastError()              -> string  "" = kein Fehler
//    BafCpp.warnings()               -> string
//    BafCpp.log()                    -> string
//    BafCpp.clearLog()               -> int
//    BafCpp.saveLog      <pfad>      -> int     Protokoll in eine Datei
//    BafCpp.timings()                -> string
//    BafCpp.setDebug     <0|1>       -> int
//
//  Zum Protokoll: es laeuft IMMER mit, nicht nur bei setDebug 1.
//  Ein Fehler, der sich erst nach zwanzig Minuten zeigt, laesst
//  sich sonst nicht mehr nachvollziehen - dann muesste man ihn
//  mit eingeschaltetem Protokoll neu erzeugen, und genau das
//  gelingt oft nicht. setDebug 1 macht es nur ausfuehrlicher
//  (jeder Aufruf mit seinen Argumenten statt nur die Meilensteine).
//  Die Groesse ist gedeckelt; laeuft es voll, faellt der ALTE Teil
//  weg, nicht der neue - beim Suchen zaehlt das Ende.
//
//  Schnelltest im Listener:
//
//    BafCpp.setDebug 1
//    BafCpp.openArchive @"D:\Episode1\big.lab"
//    BafCpp.openModel "anakin.baf"
//    format "%\n" (BafCpp.summary())
//    BafCpp.dump @"D:\tmp\max_anakin.txt" 3
//
//  Der geschriebene Dump ist zeilenweise mit dem von bafdump.exe
//  und dem von tools/pydump.py vergleichbar. Das ist die
//  Gegenprobe fuer die Max-Seite: was das Plugin IM
//  MAX-PROZESS liest, muss dasselbe sein.
// ============================================================

#pragma once

#include <max.h>
#include <utilapi.h>
#include <ifnpub.h>
#include <hold.h>
// Mtl steht in imtl.h. Neuere SDKs ziehen es ueber max.h mit, das
// 2016er nicht - und ohne die Zeile faellt erst der aelteste Build
// um, meist als letzter.
#include <imtl.h>

#include "baf_lab.h"
#include "baf_reader.h"

#include <memory>
#include <string>
#include <vector>

// ============================================================
//  Class IDs
//
//  Frei gewaehlt und einmalig. Nicht aendern, sobald eine Szene
//  Daten dieses Plugins enthaelt.
// ============================================================

#define BAFTOOLKIT_CLASS_ID     Class_ID(0x41F26B07, 0x1D9C3E54)

// Eigene Kennungen fuer die Eintraege in Max' Import- und
// Export-Dialog. Drei Klassen in einer DLL brauchen drei
// verschiedene.
#define BAFTOOLKIT_SCENE_IMPORT_ID Class_ID(0x6A417D93, 0x38EB25C0)
#define BAFTOOLKIT_SCENE_EXPORT_ID Class_ID(0x6A417D93, 0x38EB25C1)
#define BAFTOOLKIT_INTERFACE_ID Interface_ID(0x41F26B07, 0x1D9C3E55)

#define BAFTOOLKIT_VERSION_STR  _T("1.8.3")

// ============================================================
//  Function IDs
//
//  ACHTUNG: eine neue Funktion muss an DREI Stellen stehen -
//  hier im enum, in der FUNCTION_MAP weiter unten UND im
//  Interface-Deskriptor in bafimport.cpp. Fehlt die dritte, ist
//  sie bei MaxScript gar nicht angemeldet, und ein Neubau
//  behebt das nicht. tools/PRUEFE_API.py prueft alle drei.
// ============================================================

enum BafToolkitFnID {
    fn_openArchive = 0,
    fn_closeArchive,
    fn_archiveOpen,
    fn_archivePath,
    fn_entryCount,
    fn_modelCount,
    fn_modelNames,

    fn_openModel,
    fn_openFile,
    fn_close,
    fn_isOpen,
    fn_summary,
    fn_dump,

    fn_modelName,
    fn_modelScale,
    fn_nodeCount,
    fn_meshCount,
    fn_materialCount,
    fn_clipCount,
    fn_vertexCount,
    fn_faceCount,
    fn_keyCount,

    fn_nodeName,
    fn_nodeParent,
    fn_nodeMesh,
    fn_clipName,
    fn_clipFps,
    fn_clipFrames,
    fn_clipKeys,

    fn_setScale,
    fn_setBoneSize,
    fn_buildSkeleton,
    fn_clearScene,
    fn_sceneBoneCount,
    fn_buildMeshes,
    fn_sceneMeshCount,
    fn_restDump,
    fn_sceneBoneDump,
    fn_buildDump,
    fn_sceneMeshDump,

    fn_applyScene,
    fn_captureAnim,
    fn_setKeyTolerance,
    fn_sceneIsAnimated,
    fn_setForceRestPose,
    fn_addClip,
    fn_deleteClip,
    fn_setClipInfo,
    fn_pendingExportPath,
    fn_scanScene,
    fn_sceneOrigin,
    fn_sceneArchive,
    fn_sceneReport,
    fn_sceneClip,
    fn_autoLoadTemplate,

    fn_setRebuild,
    fn_setRecomputeBounds,
    fn_exportToArchive,
    fn_putFile,
    fn_restoreArchive,
    fn_exportFile,
    fn_verifyRoundTrip,
    fn_exportReport,

    fn_setImportNormals,
    fn_setHideUndrawn,
    fn_setStoreProps,
    fn_setShowBounds,
    fn_setFaceFront,
    fn_limitsReport,
    fn_limitsExceeded,

    fn_setBake,
    fn_buildAnim,
    fn_buildSequence,
    fn_clipStart,
    fn_clipEnd,
    fn_clipSceneFrames,
    fn_clearAnim,
    fn_animDump,
    fn_sceneAnimDump,

    fn_setTextureDir,
    fn_exportTextures,
    fn_buildMaterials,
    fn_slotCount,
    fn_slotInfo,

    fn_pendingPath,

    fn_version,
    fn_lastError,
    fn_warnings,
    fn_log,
    fn_clearLog,
    fn_saveLog,
    fn_timings,
    fn_setDebug,
};

// ============================================================
//  RAII-Guards
//
//  Uebernommen aus AnimMerge und XFBIN Import. In Stufe 1 noch
//  ungenutzt, weil nichts in die Szene geschrieben wird. Sie
//  stehen hier trotzdem schon, damit ab Stufe 2 (Bones anlegen)
//  niemand auf die Idee kommt, die Aufrufe von Hand zu paaren.
// ============================================================

// Stellt sicher, dass EnableSceneRedraw() auch bei einer Exception
// oder einem fruehen return wieder aufgerufen wird. Ohne das bleibt
// 3ds Max im Fehlerfall eingefroren zurueck.
class SceneRedrawGuard {
public:
    explicit SceneRedrawGuard(Interface* ip) : ip_(ip) {
        if (ip_) ip_->DisableSceneRedraw();
    }
    ~SceneRedrawGuard() {
        if (ip_) ip_->EnableSceneRedraw();
    }
    SceneRedrawGuard(const SceneRedrawGuard&)            = delete;
    SceneRedrawGuard& operator=(const SceneRedrawGuard&) = delete;
private:
    Interface* ip_;
};

// Haelt das Undo-System waehrend der Batch-Operationen an.
// theHold zaehlt Suspend/Resume intern, verschachteln ist erlaubt.
// Ohne das legt Max pro Objekt einen Undo-Record an, und ein
// spaeteres Undo hinterlaesst eine halb zurueckgerollte Szene.
class HoldSuspendGuard {
public:
    HoldSuspendGuard()  { theHold.Suspend(); }
    ~HoldSuspendGuard() { theHold.Resume();  }
    HoldSuspendGuard(const HoldSuspendGuard&)            = delete;
    HoldSuspendGuard& operator=(const HoldSuspendGuard&) = delete;
};

// ------------------------------------------------------------
//  Der Animationsmodus.
//
//  OHNE IHN ENTSTEHT KEIN EINZIGER KEYFRAME.
//
//  Control::SetValue legt nur dann einen Key an, wenn Max im
//  Animationsmodus ist. Sonst setzt es den KONSTANTEN Wert des
//  Controllers - der Bone landet also stumm auf der zuletzt
//  gesetzten Lage, und die Animation fehlt vollstaendig. Es gibt
//  keine Fehlermeldung dabei; SetValue meldet Erfolg.
//
//  Genau dieser Fehler steckte in 1.2.1: alle Keys wurden
//  berechnet, keiner kam an.
//
//  SuspendAnimate() davor haelt das Undo-System an, damit nicht
//  je Key ein Eintrag entsteht. Beides gepaart als Guard, weil
//  ein vergessenes AnimateOff() Max im Animationsmodus
//  zuruecklaesst - der rote Rahmen um den Viewport bleibt dann
//  stehen und jede weitere Aenderung des Anwenders wird
//  aufgezeichnet.
// ------------------------------------------------------------
class AnimateGuard {
public:
    AnimateGuard()  { SuspendAnimate(); AnimateOn(); }
    ~AnimateGuard() { AnimateOff(); ResumeAnimate(); }
    AnimateGuard(const AnimateGuard&)            = delete;
    AnimateGuard& operator=(const AnimateGuard&) = delete;
};

// ============================================================
//  Der Utility-Plugin-Rumpf
//
//  Traegt nichts als die Registrierung. Die eigentliche Arbeit
//  macht das FPStaticInterface darunter - genau wie bei XFBIN
//  Import. Die Oberflaeche ist MAXScript, nicht C++.
// ============================================================

class BafToolkitPlugin : public UtilityObj {
public:
    void BeginEditParams(Interface*, IUtil*) override {}
    void EndEditParams(Interface*, IUtil*) override {}
    void DeleteThis() override {}   // statische Instanz, nie loeschen
};

// ============================================================
//  FPStaticInterface - BafCpp.xxx() in MaxScript
// ============================================================

class BafToolkitInterface : public FPStaticInterface {
public:
    DECLARE_DESCRIPTOR(BafToolkitInterface)

    BEGIN_FUNCTION_MAP
        FN_1(fn_openArchive,   TYPE_INT,    OpenArchive,   TYPE_STRING)
        FN_0(fn_closeArchive,  TYPE_INT,    CloseArchive)
        FN_0(fn_archiveOpen,   TYPE_INT,    ArchiveOpen)
        FN_0(fn_archivePath,   TYPE_STRING, GetArchivePath)
        FN_0(fn_entryCount,    TYPE_INT,    EntryCount)
        FN_0(fn_modelCount,    TYPE_INT,    ModelCount)
        FN_0(fn_modelNames,    TYPE_STRING, GetModelNames)

        FN_1(fn_openModel,     TYPE_INT,    OpenModel,     TYPE_STRING)
        FN_1(fn_openFile,      TYPE_INT,    OpenFile,      TYPE_STRING)
        FN_0(fn_close,         TYPE_INT,    Close)
        FN_0(fn_isOpen,        TYPE_INT,    IsOpen)
        FN_0(fn_summary,       TYPE_STRING, GetSummary)
        FN_2(fn_dump,          TYPE_INT,    Dump,          TYPE_STRING, TYPE_INT)

        FN_0(fn_modelName,     TYPE_STRING, GetModelName)
        FN_0(fn_modelScale,    TYPE_FLOAT,  ModelScale)
        FN_0(fn_nodeCount,     TYPE_INT,    NodeCount)
        FN_0(fn_meshCount,     TYPE_INT,    MeshCount)
        FN_0(fn_materialCount, TYPE_INT,    MaterialCount)
        FN_0(fn_clipCount,     TYPE_INT,    ClipCount)
        FN_0(fn_vertexCount,   TYPE_INT,    VertexCount)
        FN_0(fn_faceCount,     TYPE_INT,    FaceCount)
        FN_0(fn_keyCount,      TYPE_INT,    KeyCount)

        FN_1(fn_nodeName,      TYPE_STRING, GetNodeName,   TYPE_INT)
        FN_1(fn_nodeParent,    TYPE_INT,    NodeParent,    TYPE_INT)
        FN_1(fn_nodeMesh,      TYPE_INT,    NodeMesh,      TYPE_INT)
        FN_1(fn_clipName,      TYPE_STRING, GetClipName,   TYPE_INT)
        FN_1(fn_clipFps,       TYPE_FLOAT,  ClipFps,       TYPE_INT)
        FN_1(fn_clipFrames,    TYPE_INT,    ClipFrames,    TYPE_INT)
        FN_1(fn_clipKeys,      TYPE_INT,    ClipKeys,      TYPE_INT)

        FN_1(fn_setScale,      TYPE_FLOAT,  SetScale,      TYPE_FLOAT)
        FN_1(fn_setBoneSize,   TYPE_FLOAT,  SetBoneSize,   TYPE_FLOAT)
        FN_0(fn_buildSkeleton, TYPE_INT,    BuildSkeleton)
        FN_0(fn_clearScene,    TYPE_INT,    ClearScene)
        FN_0(fn_sceneBoneCount,TYPE_INT,    SceneBoneCount)
        FN_0(fn_buildMeshes,   TYPE_INT,    BuildMeshes)
        FN_0(fn_sceneMeshCount,TYPE_INT,    SceneMeshCount)
        FN_1(fn_restDump,      TYPE_INT,    RestDump,      TYPE_STRING)
        FN_1(fn_sceneBoneDump, TYPE_INT,    SceneBoneDump, TYPE_STRING)
        FN_1(fn_buildDump,     TYPE_INT,    BuildDump,     TYPE_STRING)
        FN_1(fn_sceneMeshDump, TYPE_INT,    SceneMeshDump, TYPE_STRING)

        FN_0(fn_applyScene,    TYPE_INT,    ApplyScene)
        FN_1(fn_captureAnim,   TYPE_INT,    CaptureAnim,   TYPE_INT)
        FN_1(fn_setKeyTolerance,TYPE_FLOAT,  SetKeyTolerance, TYPE_FLOAT)
        FN_0(fn_sceneIsAnimated,TYPE_INT,    SceneIsAnimated)
        FN_1(fn_setForceRestPose,TYPE_INT,   SetForceRestPose, TYPE_INT)
        FN_3(fn_addClip,       TYPE_INT,    AddClip,       TYPE_STRING, TYPE_FLOAT, TYPE_INT)
        FN_1(fn_deleteClip,    TYPE_INT,    DeleteClip,    TYPE_INT)
        FN_4(fn_setClipInfo,   TYPE_INT,    SetClipInfo,   TYPE_INT, TYPE_STRING, TYPE_FLOAT, TYPE_INT)
        FN_0(fn_pendingExportPath,TYPE_STRING,TakePendingExportPath)
        FN_0(fn_scanScene,     TYPE_INT,    ScanScene)
        FN_0(fn_sceneOrigin,   TYPE_STRING, GetSceneOrigin)
        FN_0(fn_sceneArchive,  TYPE_STRING, GetSceneArchive)
        FN_0(fn_sceneReport,   TYPE_STRING, GetSceneReport)
        FN_0(fn_sceneClip,     TYPE_STRING, GetSceneClip)
        FN_0(fn_autoLoadTemplate,TYPE_INT,  AutoLoadTemplate)

        FN_1(fn_setRebuild,    TYPE_INT,    SetRebuild,    TYPE_INT)
        FN_1(fn_setRecomputeBounds,TYPE_INT,SetRecomputeBounds, TYPE_INT)
        FN_0(fn_exportToArchive,TYPE_INT,   ExportToArchive)
        FN_2(fn_putFile,       TYPE_INT,    PutFile,       TYPE_STRING, TYPE_STRING)
        FN_0(fn_restoreArchive,TYPE_INT,    RestoreArchive)
        FN_1(fn_exportFile,    TYPE_INT,    ExportFile,    TYPE_STRING)
        FN_0(fn_verifyRoundTrip,TYPE_INT,   VerifyRoundTrip)
        FN_0(fn_exportReport,  TYPE_STRING, GetExportReport)

        FN_1(fn_setImportNormals,TYPE_INT,  SetImportNormals, TYPE_INT)
        FN_1(fn_setHideUndrawn, TYPE_INT,   SetHideUndrawn, TYPE_INT)
        FN_1(fn_setStoreProps, TYPE_INT,    SetStoreProps, TYPE_INT)
        FN_1(fn_setShowBounds, TYPE_INT,    SetShowBounds, TYPE_INT)
        FN_1(fn_setFaceFront,  TYPE_INT,    SetFaceFront,  TYPE_INT)
        FN_0(fn_limitsReport,  TYPE_STRING, GetLimitsReport)
        FN_0(fn_limitsExceeded,TYPE_INT,    LimitsExceeded)

        FN_1(fn_setBake,       TYPE_INT,    SetBake,       TYPE_INT)
        FN_1(fn_buildAnim,     TYPE_INT,    BuildAnim,     TYPE_INT)
        FN_1(fn_buildSequence, TYPE_INT,    BuildSequence, TYPE_INT)
        FN_1(fn_clipStart,     TYPE_INT,    ClipStart,     TYPE_INT)
        FN_1(fn_clipEnd,       TYPE_INT,    ClipEnd,       TYPE_INT)
        FN_1(fn_clipSceneFrames,TYPE_INT,   ClipSceneFrames, TYPE_INT)
        FN_0(fn_clearAnim,     TYPE_INT,    ClearAnim)
        FN_2(fn_animDump,      TYPE_INT,    AnimDump,      TYPE_INT, TYPE_STRING)
        FN_2(fn_sceneAnimDump, TYPE_INT,    SceneAnimDump, TYPE_INT, TYPE_STRING)

        FN_1(fn_setTextureDir, TYPE_STRING, SetTextureDir, TYPE_STRING)
        FN_0(fn_exportTextures,TYPE_INT,    ExportTextures)
        FN_0(fn_buildMaterials,TYPE_INT,    BuildMaterials)
        FN_0(fn_slotCount,     TYPE_INT,    SlotCount)
        FN_1(fn_slotInfo,      TYPE_STRING, GetSlotInfo,   TYPE_INT)

        FN_0(fn_pendingPath,   TYPE_STRING, TakePendingPath)

        FN_0(fn_version,       TYPE_STRING, GetVersion)
        FN_0(fn_lastError,     TYPE_STRING, GetLastError)
        FN_0(fn_warnings,      TYPE_STRING, GetWarnings)
        FN_0(fn_log,           TYPE_STRING, GetLog)
        FN_0(fn_clearLog,      TYPE_INT,    ClearLog)
        FN_1(fn_saveLog,       TYPE_INT,    SaveLog,       TYPE_STRING)
        FN_0(fn_timings,       TYPE_STRING, GetTimings)
        FN_1(fn_setDebug,      TYPE_INT,    SetDebug,      TYPE_INT)
    END_FUNCTION_MAP

    // --- Von MaxScript aufrufbar ---
    int OpenArchive(const MCHAR* path);
    int CloseArchive();
    int ArchiveOpen();
    const MCHAR* GetArchivePath();
    int EntryCount();
    int ModelCount();
    const MCHAR* GetModelNames();

    int OpenModel(const MCHAR* name);
    int OpenFile(const MCHAR* path);
    int Close();
    int IsOpen();
    const MCHAR* GetSummary();
    int Dump(const MCHAR* outPath, int detail);

    const MCHAR* GetModelName();
    float ModelScale();
    int NodeCount();
    int MeshCount();
    int MaterialCount();
    int ClipCount();
    int VertexCount();
    int FaceCount();
    int KeyCount();

    const MCHAR* GetNodeName(int index);
    int NodeParent(int index);
    int NodeMesh(int index);
    const MCHAR* GetClipName(int index);
    float ClipFps(int index);
    int ClipFrames(int index);
    int ClipKeys(int index);

    float SetScale(float scale);
    float SetBoneSize(float size);
    int BuildSkeleton();
    int ClearScene();
    int SceneBoneCount();
    int BuildMeshes();
    int SceneMeshCount();
    int RestDump(const MCHAR* outPath);
    int SceneBoneDump(const MCHAR* outPath);
    int BuildDump(const MCHAR* outPath);
    int SceneMeshDump(const MCHAR* outPath);

    int ApplyScene();
    int CaptureAnim(int clipIndex);
    float SetKeyTolerance(float degrees);
    int SceneIsAnimated();
    int SetForceRestPose(int on);
    int AddClip(const MCHAR* name, float fps, int frames);
    int DeleteClip(int clipIndex);
    int SetClipInfo(int clipIndex, const MCHAR* name, float fps, int frames);
    const MCHAR* TakePendingExportPath();
    void SetPendingExportPath(const std::wstring& path);
    int ScanScene();
    const MCHAR* GetSceneOrigin();
    const MCHAR* GetSceneArchive();
    const MCHAR* GetSceneReport();
    const MCHAR* GetSceneClip();
    int AutoLoadTemplate();

    int SetRebuild(int on);
    int SetRecomputeBounds(int on);
    int ExportToArchive();
    int PutFile(const MCHAR* entryName, const MCHAR* path);
    int RestoreArchive();
    int ExportFile(const MCHAR* path);
    int VerifyRoundTrip();
    const MCHAR* GetExportReport();

    int SetImportNormals(int on);
    int SetHideUndrawn(int on);
    int SetStoreProps(int on);
    int SetShowBounds(int on);
    int SetFaceFront(int on);
    const MCHAR* GetLimitsReport();
    int LimitsExceeded();

    int SetBake(int on);
    int BuildAnim(int clipIndex);
    int BuildSequence(int spacing);
    int ClipStart(int clipIndex);
    int ClipEnd(int clipIndex);
    int ClipSceneFrames(int clipIndex);
    int ClearAnim();
    int AnimDump(int clipIndex, const MCHAR* outPath);
    int SceneAnimDump(int clipIndex, const MCHAR* outPath);

    const MCHAR* SetTextureDir(const MCHAR* path);
    int ExportTextures();
    int BuildMaterials();
    int SlotCount();
    const MCHAR* GetSlotInfo(int index);

    // Der Pfad, den ein Dateidialog von Max uebergeben hat.
    // Wird beim Abholen geleert - er gilt fuer genau einen
    // Fensteraufbau.
    const MCHAR* TakePendingPath();
    void SetPendingPath(const std::wstring& path);
    // Oeffnet die Oberflaeche. Ueber das Makro, damit es genau
    // dasselbe Fenster ist - eine zweite, leicht abweichende
    // Fassung waere die naechste Fehlerquelle.
    void RunMacro();
    // Dasselbe fuer das Exportfenster.
    void RunExportMacro();
    // Eine Meldung mit dem Max-Fenster als Elternteil.
    void ShowMessage(const std::wstring& text);
    // Fuer den Export-Dialog: Szene uebernehmen und schreiben.
    bool ExportFromDialog(const MCHAR* path);

    const MCHAR* GetVersion();
    const MCHAR* GetLastError();
    const MCHAR* GetWarnings();
    const MCHAR* GetLog();
    int ClearLog();
    int SaveLog(const MCHAR* outPath);
    const MCHAR* GetTimings();
    int SetDebug(int on);

private:
    // --- Zustand zwischen den Aufrufen ---
    baf::LabArchive archive_;
    std::unique_ptr<baf::BafFile> model_;

    // Namen der Modelle im Archiv, einmal beim Oeffnen ermittelt.
    std::vector<std::string> modelNames_;

    // Rueckgabepuffer. Eine an MaxScript zurueckgegebene MCHAR*
    // muss den Aufruf ueberleben - ein lokaler std::wstring waere
    // beim Lesen laengst weg. Deshalb Member.
    std::wstring returnBuffer_;

    std::wstring lastError_;
    std::wstring warnings_;
    std::wstring log_;
    std::wstring timings_;
    bool debug_ = false;

    // --- Stufe 2: Szenenzustand ---
    float boneSize_ = 0.0f;   // 0 = blosse Linien statt Kaesten

    // Die angelegten Bones, parallel zu model_->nodes.
    //
    // Ein Handle, NICHT INode*. Ein roher Zeiger auf einen
    // Szenenknoten ueberlebt es nicht, wenn der Benutzer den
    // Knoten zwischen zwei Aufrufen loescht oder eine neue Szene
    // anfaengt - und das Plugin merkt davon nichts. Ueber das
    // Handle liefert die Nachschlagefunktion in dem Fall schlicht
    // NULL, statt auf freigegebenen Speicher zu zeigen. Genau
    // dieser Umbau war bei XFBIN 1.9.3 faellig, nach einem
    // Absturz.
    //
    // Typ ist ULONG, weil INode::GetHandle() ein KNOTEN-Handle
    // liefert - und das ist etwas anderes als ein AnimHandle.
    // Die SDK-Doku sagt es ausdruecklich: "Do not confuse Node
    // Handles with Animatable Handles: they are actually quite
    // different." Ein Knoten-Handle ist eine dauerhafte, mit der
    // Szene gespeicherte Nummer; ein AnimHandle ist ein
    // Instanzzaehler, der nur innerhalb einer Max-Sitzung gilt.
    //
    // Bis 1.0.0 stand hier AnimHandle, und nachgeschlagen wurde
    // mit Animatable::GetAnimByHandle - also ein Knotenwert in
    // einer fremden Nummernwelt. Das haette entweder NULL oder,
    // schlimmer, ein voellig anderes Objekt geliefert.
    // Nachgeschlagen wird jetzt mit Interface::GetINodeByHandle.
    std::vector<ULONG> boneHandles_;

    // Die angelegten Mesh-Objekte, parallel zu model_->meshes.
    std::vector<ULONG> meshHandles_;
    std::wstring sceneModelName_;

    // --- Stufe 4 ---
    std::wstring textureDir_;
    // Liefert textureDir_, legt aber einen Standardordner an,
    // wenn noch keiner gewaehlt wurde. Leer nur, wenn sich auch
    // der nicht anlegen liess.
    std::wstring EnsureTextureDir();
    std::vector<baf::MaterialSlot> slots_;

    // Das gebaute Multi/Sub-Object.
    //
    // Hier ist AnimHandle richtig: ein Material ist ein
    // Animatable, aber kein Knoten. Geholt wird es mit
    // Animatable::GetHandleByAnim - MultiMtl selbst hat kein
    // GetHandle().
    AnimHandle materialHandle_ = 0;
    Mtl* MaterialFromHandle() const;

    // --- Stufe 7 ---
    // Die Bytes, aus denen model_ gelesen wurde. Der Rundlauf
    // vergleicht dagegen; ohne sie waere "byte-gleich" nicht
    // pruefbar.
    std::vector<uint8_t> sourceBytes_;
    std::wstring exportReport_;
    // Von einem Dateidialog uebergeben, bis die Oberflaeche ihn
    // abholt.
    std::wstring pendingPath_;
    // Das Ziel, das Max' Export-Dialog gewaehlt hat. Das
    // Exportfenster holt es ab und traegt es als Ziel ein.
    std::wstring pendingExportPath_;
    // Der Name, unter dem das Modell im Archiv steht. Ohne ihn
    // waere nach einem openFile nicht klar, wohin zurueck.
    std::string  sourceEntry_;
    bool rebuild_ = false;

    // Wie weit ein Frame von der geraden Linie zwischen zwei
    // behaltenen Keys abweichen darf, bevor ein eigener Key
    // noetig wird. In Grad; die Laengentoleranz haengt daran.
    //
    // 0 behaelt jeden Frame - das war das Verhalten bis 1.7.1
    // und ist der Grund, warum ein einziger geaenderter Clip die
    // Datei um 123 KB wachsen liess.
    float keyTolerance_ = 0.5f;

    // Hebt die Sperre gegen das Ueberschreiben der Ruhelage auf.
    // Standard aus: eine kaputte Ruhelage merkt man erst im
    // Spiel, und dann ist die Datei schon geschrieben.
    bool forceRestPose_ = false;

    // --- Stufe 11: was in der Szene steht ---
    //
    // Aus den Rohdaten an den Objekten gelesen, nicht aus dem
    // Speicher. Deshalb ueberlebt es einen Neustart von Max.
    std::wstring sceneOrigin_;    // bafModelName
    std::wstring sceneArchive_;   // bafArchive
    std::wstring sceneReport_;
    // Der Clip, den buildAnim zuletzt in die Szene gebaut hat.
    // Steht als bafSceneClip am ersten Bone und ueberlebt damit
    // Speichern und Neustart.
    std::wstring sceneClip_;
    // Eine einzelne Zeile im User-Prop-Puffer setzen oder
    // ersetzen.
    static void SetProp(INode* node, const std::wstring& key,
                        const std::wstring& value);
    // Eine Zeile aus einem User-Prop-Puffer lesen.
    static bool ReadProp(const std::wstring& buffer, const wchar_t* key,
                         std::wstring& value);
    bool recomputeBounds_ = false;

    // Ein Mesh vollstaendig aus der Szene neu bauen. Gibt false
    // zurueck und fuellt note, wenn es nicht geht.
    bool RebuildMeshFromScene(size_t meshIndex, Mesh& maxMesh,
                              const std::vector<baf::MaterialSlot>& slots,
                              std::wstring& note);

    // --- Stufe 6 ---
    bool importNormals_ = true;
    bool hideUndrawn_   = true;
    bool storeProps_    = true;
    bool showBounds_    = false;
    bool faceFront_     = true;

    // Die Drehung, die das Rig der Frontansicht zuwendet, und
    // ihre Umkehrung. Bei 180 Grad um Z sind beide dieselbe
    // Matrix - trotzdem getrennt, damit ein anderer Winkel
    // spaeter nicht stillschweigend falsch wird.
    baf::Matrix34 FrontRotation() const;
    baf::Matrix34 FrontRotationInverse() const;
    // Die angelegten Huellkugel-Helfer, damit clearScene sie
    // wieder loswird.
    std::vector<ULONG> boundsHandles_;

    // Der Laengenfaktor.
    //
    // 80 statt 1: die Modelle sind in Einheiten von rund 0,8
    // hoch. Mit Faktor 1 steht in Max eine Figur von 0,8
    // Einheiten - kleiner als das Raster und praktisch nicht zu
    // bearbeiten. Mit 80 wird daraus 64, und das ist ungefaehr
    // die Groesse, die Max' eigener Biped mitbringt.
    float scale_ = 80.0f;

    // --- Stufe 5 ---
    bool bake_ = true;
    // Startframe je Clip im Sequenzmodus, sonst leer.
    std::vector<int> clipStart_;
    std::vector<int> clipEnd_;

    // Die Keys EINES Clips auf das Skelett legen, ab startFrame.
    // Gemeinsame Grundlage von buildAnim und buildSequence.
    int ApplyClip(size_t clipIndex, int startFrame, bool restKeys);

    // Der Knoten zu einem Handle, oder nullptr.
    INode* NodeFromHandle(ULONG handle) const;

    void ClearMessages();
    // Meilenstein - laeuft immer mit.
    void Log(const std::wstring& line);
    // Einzelheit - nur bei setDebug 1.
    void LogDetail(const std::wstring& line);
    const MCHAR* Give(const std::wstring& text);
};
