// ============================================================
//  BAF Toolkit - DLL Entry
//
//  Hinweis zu den Exports: Die Funktionen sind NICHT extern "C",
//  deshalb exportiert __declspec(dllexport) den dekorierten Namen.
//  3ds Max sucht aber den undekorierten. Die bafimport.def sorgt
//  dafuer, dass beide Namen sauber herauskommen - sie darf NICHT
//  entfernt werden, auch wenn das doppelt gemoppelt aussieht.
// ============================================================

#include "baftoolkit.h"

// ClassDesc2 steht in iparamb2.h. Die neueren SDKs ziehen den
// Header ueber max.h mit herein, 2016 nicht - dort fehlt sonst die
// Basisklasse, und jede override-Methode meldet danach einen
// eigenen Fehler.
#include <iparamb2.h>

// SceneImport und SceneExport stehen in impexp.h; die neueren
// SDKs ziehen ihn ueber max.h mit, das von 2016 nicht.
#include <impexp.h>

#include <string>

HINSTANCE hInstance = nullptr;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, ULONG fdwReason, LPVOID) {
    if (fdwReason == DLL_PROCESS_ATTACH) {
        hInstance = hinstDLL;
        DisableThreadLibraryCalls(hinstDLL);
    }
    return TRUE;
}

static BafToolkitPlugin thePlugin;

class BafToolkitClassDesc : public ClassDesc2 {
public:
    int          IsPublic() override                { return TRUE; }
    void*        Create(BOOL) override              { return &thePlugin; }
    const MCHAR* ClassName() override               { return _T("BafToolkit"); }

    // NonLocalizedClassName gibt es erst ab Max 2022. Davor hat
    // ClassDesc die Methode nicht, und override waere ein Fehler.
    // MAX_RELEASE kommt aus plugapi.h: 2016 = 18000, je Jahrgang
    // 1000 mehr, also 2022 = 24000.
#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
    const MCHAR* NonLocalizedClassName() override   { return _T("BafToolkit"); }
#endif
    SClass_ID    SuperClassID() override            { return UTILITY_CLASS_ID; }
    Class_ID     ClassID() override                 { return BAFTOOLKIT_CLASS_ID; }
    const MCHAR* Category() override                { return _T("Import"); }
    const MCHAR* InternalName() override            { return _T("BafToolkit"); }
    HINSTANCE    HInstance() override               { return hInstance; }
};

static BafToolkitClassDesc theClassDesc;

// ============================================================
//  Eintraege in Max' eigenen Import- und Export-Dialogen
//
//  Wer "Datei -> Importieren" benutzt, sucht das Format dort -
//  und findet es nicht, wenn ein Plugin nur ein eigenes Menue
//  mitbringt. Eine SceneImport- und eine SceneExport-Klasse
//  tragen BAF in die Formatliste ein, neben FBX, Collada und den
//  uebrigen.
//
//  Damit entfaellt das eigene Menue ganz. Nebenbei loest das den
//  Konflikt, den es ab Max 2025 mit einem gemeinsamen "DH Tools"
//  gab: CreateSubMenu legt an und sucht nicht, zwei Pakete
//  koennen sich ein Untermenue also nicht teilen.
//
//  Das Makro bleibt trotzdem registriert - unter
//  Customize -> Customize User Interface -> Kategorie
//  "DH Tools" laesst es sich auf eine Toolbar oder einen
//  Shortcut legen, wer das lieber mag.
// ============================================================

BafToolkitInterface* GetBafInterface();

class BafSceneImport : public SceneImport {
public:
    // Zwei Endungen: die lose Datei und das Archiv. Wer das
    // Archiv waehlt, bekommt die Modellliste.
    int ExtCount() override { return 2; }
    const MCHAR* Ext(int i) override {
        if (i == 0) return _T("baf");
        if (i == 1) return _T("lab");
        return _T("");
    }

    const MCHAR* LongDesc() override {
        return _T("Big Ape Actor - Star Wars Episode I (model, textures, animations)");
    }
    const MCHAR* ShortDesc() override        { return _T("BAF"); }
    const MCHAR* AuthorName() override       { return _T("DennisH"); }
    const MCHAR* CopyrightMessage() override { return _T(""); }
    const MCHAR* OtherMessage1() override    { return _T(""); }
    const MCHAR* OtherMessage2() override    { return _T(""); }

    unsigned int Version() override          { return 183; }
    void ShowAbout(HWND) override            {}

    int DoImport(const MCHAR* name, ImpInterface*, Interface*,
                 BOOL suppressPrompts) override {
        if (name == nullptr) return 0;
        BafToolkitInterface* iface = GetBafInterface();
        if (iface == nullptr) return 0;

        iface->SetPendingPath(std::wstring(name));

        // suppressPrompts ist gesetzt, wenn Max ohne Rueckfragen
        // importieren soll - etwa aus einem Skript heraus. Dann
        // waere ein Fenster falsch; der Pfad bleibt hinterlegt
        // und die Oberflaeche holt ihn beim naechsten Oeffnen ab.
        if (suppressPrompts) return 1;

        iface->RunMacro();
        return 1;
    }
};

class BafSceneImportClassDesc : public ClassDesc2 {
public:
    int IsPublic() override { return TRUE; }

    // ------------------------------------------------------
    //  new, NICHT ein statisches Objekt.
    //
    //  Das Singleton-Muster, das die Autodesk-Doku zeigt, gilt
    //  fuer UTILITY-Plugins - von denen gibt es genau eines.
    //  SceneImport ist ein anderer Fall: Max legt eine Instanz
    //  an, fragt ExtCount, Ext und die Beschreibungen ab und
    //  gibt den Speicher danach wieder frei. Die Klasse hat kein
    //  DeleteThis, also loescht Max direkt.
    //
    //  Ein statisches Objekt zu loeschen zerlegt den Heap. Genau
    //  daran ist bei XFBIN Import 3ds Max abgestuerzt, sobald man
    //  den Import-Dialog nur GEOEFFNET hat - noch bevor
    //  irgendetwas gewaehlt war.
    // ------------------------------------------------------
    void* Create(BOOL) override { return new BafSceneImport(); }

    const MCHAR* ClassName() override               { return _T("BAF Import"); }
#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
    const MCHAR* NonLocalizedClassName() override   { return _T("BAF Import"); }
#endif
    SClass_ID    SuperClassID() override            { return SCENE_IMPORT_CLASS_ID; }
    Class_ID     ClassID() override                 { return BAFTOOLKIT_SCENE_IMPORT_ID; }
    const MCHAR* Category() override                { return _T("Import"); }
    const MCHAR* InternalName() override            { return _T("BafSceneImport"); }
    HINSTANCE    HInstance() override               { return hInstance; }
};

class BafSceneExport : public SceneExport {
public:
    int ExtCount() override { return 1; }
    const MCHAR* Ext(int i) override         { return (i == 0) ? _T("baf") : _T(""); }

    const MCHAR* LongDesc() override {
        return _T("Big Ape Actor - Star Wars Episode I");
    }
    const MCHAR* ShortDesc() override        { return _T("BAF"); }
    const MCHAR* AuthorName() override       { return _T("DennisH"); }
    const MCHAR* CopyrightMessage() override { return _T(""); }
    const MCHAR* OtherMessage1() override    { return _T(""); }
    const MCHAR* OtherMessage2() override    { return _T(""); }

    unsigned int Version() override          { return 183; }
    void ShowAbout(HWND) override            {}

    int DoExport(const MCHAR* name, ExpInterface*, Interface*,
                 BOOL suppressPrompts, DWORD) override {
        if (name == nullptr) return 0;
        BafToolkitInterface* iface = GetBafInterface();
        if (iface == nullptr) return 0;

        // Ohne geoeffnetes Modell geht es nicht: der Schreiber
        // patcht die Aenderungen in die urspruenglichen Bytes
        // zurueck, und ohne die gibt es nichts zu patchen. Eine
        // .baf aus einer beliebigen Szene zu erfinden waere etwas
        // ganz anderes als ein Export.
        // ------------------------------------------------------
        //  Das Exportfenster geht IMMER auf.
        //
        //  Vorher lief der Export durch, sobald die Vorlage sich
        //  von selbst fand - und man sah nie, was uebernommen
        //  wurde. Beim Export ist genau das aber die Frage:
        //  welche Vorlage, welcher Clip, wird die Topologie
        //  umgebaut, halten die Engine-Grenzen. Ein Dateidialog
        //  allein kann das nicht beantworten.
        //
        //  Max hat den Zielpfad schon erfragt; er wird
        //  durchgereicht und steht im Fenster als Ziel bereit.
        //
        //  Rueckgabe 1, obwohl hier noch nichts geschrieben ist:
        //  aus Max' Sicht hat das Modul uebernommen. Eine 0 wuerde
        //  "Export module failure" ausloesen, und das waere
        //  falsch - es ist ja nichts schiefgegangen.
        // ------------------------------------------------------
        iface->SetPendingExportPath(std::wstring(name));

        // Bei suppressPrompts (Aufruf aus einem Skript) hat ein
        // Fenster nichts zu suchen: dann den automatischen Weg
        // gehen und ehrlich melden, ob er geklappt hat.
        if (suppressPrompts) return iface->ExportFromDialog(name) ? 1 : 0;

        iface->RunExportMacro();
        return 1;
    }

    BOOL SupportsOptions(int, DWORD) override { return FALSE; }
};

class BafSceneExportClassDesc : public ClassDesc2 {
public:
    int IsPublic() override { return TRUE; }
    // Dasselbe wie beim Import: new, kein statisches Objekt.
    void* Create(BOOL) override { return new BafSceneExport(); }

    const MCHAR* ClassName() override               { return _T("BAF Export"); }
#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
    const MCHAR* NonLocalizedClassName() override   { return _T("BAF Export"); }
#endif
    SClass_ID    SuperClassID() override            { return SCENE_EXPORT_CLASS_ID; }
    Class_ID     ClassID() override                 { return BAFTOOLKIT_SCENE_EXPORT_ID; }
    const MCHAR* Category() override                { return _T("Export"); }
    const MCHAR* InternalName() override            { return _T("BafSceneExport"); }
    HINSTANCE    HInstance() override               { return hInstance; }
};

static BafSceneImportClassDesc theSceneImportClassDesc;
static BafSceneExportClassDesc theSceneExportClassDesc;

// Diesen Text zeigt Max unter Customize -> Plugin Manager. Die
// Versionsnummer gehoert hinein: sie ist der schnellste Weg
// nachzusehen, welche .dlu wirklich geladen ist.
__declspec(dllexport) const TCHAR* LibDescription() {
    return _T("BAF Toolkit ") BAFTOOLKIT_VERSION_STR
           _T(" - Star Wars Episode I: The Phantom Menace");
}
__declspec(dllexport) int        LibNumberClasses()  { return 3; }

__declspec(dllexport) ClassDesc* LibClassDesc(int i) {
    switch (i) {
    case 0:  return &theClassDesc;
    case 1:  return &theSceneImportClassDesc;
    case 2:  return &theSceneExportClassDesc;
    default: return nullptr;
    }
}
__declspec(dllexport) ULONG      LibVersion()        { return VERSION_3DSMAX; }
__declspec(dllexport) int        CanAutoDefer()      { return FALSE; }
