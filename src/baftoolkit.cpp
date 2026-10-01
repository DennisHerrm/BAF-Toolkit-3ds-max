// ============================================================
//  BAF Toolkit - Implementierung
//  Version 1.8.3
//
//  Siehe baftoolkit.h fuer die MaxScript-API.
// ============================================================

#include "baftoolkit.h"

// ClassDesc2 steht in iparamb2.h. Die neueren SDKs ziehen den
// Header ueber max.h mit herein, 2016 nicht.
#include <iparamb2.h>
// ExecuteMAXScriptScript steht in maxscript/maxscript.h.
#include <maxscript/maxscript.h>
// TriObject und CreateNewTriObject stehen in triobj.h, ObjectState
// und triObjectClassID in object.h. Beide zieht nur ein neueres SDK
// ueber max.h mit herein - bei 2016 fehlen sie sonst, und jede
// Verwendung meldet danach einen eigenen Fehler.
#include <object.h>
#include <triobj.h>
// StdMat2/NewDefaultStdMat und BitmapTex stehen in stdmat.h, das
// Multi/Sub-Object in istdplug.h. Beide zieht nur ein neueres SDK
// ueber max.h mit herein.
#include <control.h>
// MeshNormalSpec fuer die vorgegebenen Vertexnormalen. Nur ein
// neueres SDK zieht den Header ueber max.h mit herein.
#include <MeshNormalSpec.h>
#include <stdmat.h>
#include <istdplug.h>
#include <imtl.h>

#include <tchar.h>

#include <chrono>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ============================================================
//  Statische Interface-Instanz (registriert BafCpp.xxx)
//
//  Dritte der drei Eintragungsstellen. Die anderen beiden sind
//  das enum und die FUNCTION_MAP in baftoolkit.h.
// ============================================================

BafToolkitInterface* GetBafInterface();

static BafToolkitInterface theBafToolkitInterface(
    BAFTOOLKIT_INTERFACE_ID,
    _T("BafCpp"), 0, NULL, FP_CORE,

    fn_openArchive, _T("openArchive"), 0, TYPE_INT, 0, 1,
        _T("path"), 0, TYPE_STRING,
    fn_closeArchive, _T("closeArchive"), 0, TYPE_INT, 0, 0,
    fn_archiveOpen, _T("archiveOpen"), 0, TYPE_INT, 0, 0,
    fn_archivePath, _T("archivePath"), 0, TYPE_STRING, 0, 0,
    fn_entryCount, _T("entryCount"), 0, TYPE_INT, 0, 0,
    fn_modelCount, _T("modelCount"), 0, TYPE_INT, 0, 0,
    fn_modelNames, _T("modelNames"), 0, TYPE_STRING, 0, 0,

    fn_openModel, _T("openModel"), 0, TYPE_INT, 0, 1,
        _T("name"), 0, TYPE_STRING,
    fn_openFile, _T("openFile"), 0, TYPE_INT, 0, 1,
        _T("path"), 0, TYPE_STRING,
    fn_close, _T("close"), 0, TYPE_INT, 0, 0,
    fn_isOpen, _T("isOpen"), 0, TYPE_INT, 0, 0,
    fn_summary, _T("summary"), 0, TYPE_STRING, 0, 0,
    fn_dump, _T("dump"), 0, TYPE_INT, 0, 2,
        _T("outPath"), 0, TYPE_STRING,
        _T("detail"), 0, TYPE_INT,

    fn_modelName, _T("modelName"), 0, TYPE_STRING, 0, 0,
    fn_modelScale, _T("modelScale"), 0, TYPE_FLOAT, 0, 0,
    fn_nodeCount, _T("nodeCount"), 0, TYPE_INT, 0, 0,
    fn_meshCount, _T("meshCount"), 0, TYPE_INT, 0, 0,
    fn_materialCount, _T("materialCount"), 0, TYPE_INT, 0, 0,
    fn_clipCount, _T("clipCount"), 0, TYPE_INT, 0, 0,
    fn_vertexCount, _T("vertexCount"), 0, TYPE_INT, 0, 0,
    fn_faceCount, _T("faceCount"), 0, TYPE_INT, 0, 0,
    fn_keyCount, _T("keyCount"), 0, TYPE_INT, 0, 0,

    fn_nodeName, _T("nodeName"), 0, TYPE_STRING, 0, 1,
        _T("index"), 0, TYPE_INT,
    fn_nodeParent, _T("nodeParent"), 0, TYPE_INT, 0, 1,
        _T("index"), 0, TYPE_INT,
    fn_nodeMesh, _T("nodeMesh"), 0, TYPE_INT, 0, 1,
        _T("index"), 0, TYPE_INT,
    fn_clipName, _T("clipName"), 0, TYPE_STRING, 0, 1,
        _T("index"), 0, TYPE_INT,
    fn_clipFps, _T("clipFps"), 0, TYPE_FLOAT, 0, 1,
        _T("index"), 0, TYPE_INT,
    fn_clipFrames, _T("clipFrames"), 0, TYPE_INT, 0, 1,
        _T("index"), 0, TYPE_INT,
    fn_clipKeys, _T("clipKeys"), 0, TYPE_INT, 0, 1,
        _T("index"), 0, TYPE_INT,

    fn_setScale, _T("setScale"), 0, TYPE_FLOAT, 0, 1,
        _T("scale"), 0, TYPE_FLOAT,
    fn_setBoneSize, _T("setBoneSize"), 0, TYPE_FLOAT, 0, 1,
        _T("size"), 0, TYPE_FLOAT,
    fn_buildSkeleton, _T("buildSkeleton"), 0, TYPE_INT, 0, 0,
    fn_clearScene, _T("clearScene"), 0, TYPE_INT, 0, 0,
    fn_sceneBoneCount, _T("sceneBoneCount"), 0, TYPE_INT, 0, 0,
    fn_buildMeshes, _T("buildMeshes"), 0, TYPE_INT, 0, 0,
    fn_sceneMeshCount, _T("sceneMeshCount"), 0, TYPE_INT, 0, 0,
    fn_restDump, _T("restDump"), 0, TYPE_INT, 0, 1,
        _T("outPath"), 0, TYPE_STRING,
    fn_sceneBoneDump, _T("sceneBoneDump"), 0, TYPE_INT, 0, 1,
        _T("outPath"), 0, TYPE_STRING,
    fn_buildDump, _T("buildDump"), 0, TYPE_INT, 0, 1,
        _T("outPath"), 0, TYPE_STRING,
    fn_sceneMeshDump, _T("sceneMeshDump"), 0, TYPE_INT, 0, 1,
        _T("outPath"), 0, TYPE_STRING,

    fn_applyScene, _T("applyScene"), 0, TYPE_INT, 0, 0,
    fn_captureAnim, _T("captureAnim"), 0, TYPE_INT, 0, 1,
        _T("clip"), 0, TYPE_INT,
    fn_setKeyTolerance, _T("setKeyTolerance"), 0, TYPE_FLOAT, 0, 1,
        _T("degrees"), 0, TYPE_FLOAT,
    fn_sceneIsAnimated, _T("sceneIsAnimated"), 0, TYPE_INT, 0, 0,
    fn_setForceRestPose, _T("setForceRestPose"), 0, TYPE_INT, 0, 1,
        _T("on"), 0, TYPE_INT,
    fn_addClip, _T("addClip"), 0, TYPE_INT, 0, 3,
        _T("name"), 0, TYPE_STRING,
        _T("fps"), 0, TYPE_FLOAT,
        _T("frames"), 0, TYPE_INT,
    fn_deleteClip, _T("deleteClip"), 0, TYPE_INT, 0, 1,
        _T("clip"), 0, TYPE_INT,
    fn_setClipInfo, _T("setClipInfo"), 0, TYPE_INT, 0, 4,
        _T("clip"), 0, TYPE_INT,
        _T("name"), 0, TYPE_STRING,
        _T("fps"), 0, TYPE_FLOAT,
        _T("frames"), 0, TYPE_INT,
    fn_pendingExportPath, _T("pendingExportPath"), 0, TYPE_STRING, 0, 0,
    fn_scanScene, _T("scanScene"), 0, TYPE_INT, 0, 0,
    fn_sceneOrigin, _T("sceneOrigin"), 0, TYPE_STRING, 0, 0,
    fn_sceneArchive, _T("sceneArchive"), 0, TYPE_STRING, 0, 0,
    fn_sceneReport, _T("sceneReport"), 0, TYPE_STRING, 0, 0,
    fn_sceneClip, _T("sceneClip"), 0, TYPE_STRING, 0, 0,
    fn_autoLoadTemplate, _T("autoLoadTemplate"), 0, TYPE_INT, 0, 0,

    fn_setRebuild, _T("setRebuild"), 0, TYPE_INT, 0, 1,
        _T("on"), 0, TYPE_INT,
    fn_setRecomputeBounds, _T("setRecomputeBounds"), 0, TYPE_INT, 0, 1,
        _T("on"), 0, TYPE_INT,
    fn_exportToArchive, _T("exportToArchive"), 0, TYPE_INT, 0, 0,
    fn_putFile, _T("putFile"), 0, TYPE_INT, 0, 2,
        _T("entry"), 0, TYPE_STRING,
        _T("path"), 0, TYPE_STRING,
    fn_restoreArchive, _T("restoreArchive"), 0, TYPE_INT, 0, 0,
    fn_exportFile, _T("exportFile"), 0, TYPE_INT, 0, 1,
        _T("path"), 0, TYPE_STRING,
    fn_verifyRoundTrip, _T("verifyRoundTrip"), 0, TYPE_INT, 0, 0,
    fn_exportReport, _T("exportReport"), 0, TYPE_STRING, 0, 0,

    fn_setImportNormals, _T("setImportNormals"), 0, TYPE_INT, 0, 1,
        _T("on"), 0, TYPE_INT,
    fn_setHideUndrawn, _T("setHideUndrawn"), 0, TYPE_INT, 0, 1,
        _T("on"), 0, TYPE_INT,
    fn_setStoreProps, _T("setStoreProps"), 0, TYPE_INT, 0, 1,
        _T("on"), 0, TYPE_INT,
    fn_setShowBounds, _T("setShowBounds"), 0, TYPE_INT, 0, 1,
        _T("on"), 0, TYPE_INT,
    fn_setFaceFront, _T("setFaceFront"), 0, TYPE_INT, 0, 1,
        _T("on"), 0, TYPE_INT,
    fn_limitsReport, _T("limitsReport"), 0, TYPE_STRING, 0, 0,
    fn_limitsExceeded, _T("limitsExceeded"), 0, TYPE_INT, 0, 0,

    fn_setBake, _T("setBake"), 0, TYPE_INT, 0, 1,
        _T("on"), 0, TYPE_INT,
    fn_buildAnim, _T("buildAnim"), 0, TYPE_INT, 0, 1,
        _T("clip"), 0, TYPE_INT,
    fn_buildSequence, _T("buildSequence"), 0, TYPE_INT, 0, 1,
        _T("spacing"), 0, TYPE_INT,
    fn_clipStart, _T("clipStart"), 0, TYPE_INT, 0, 1,
        _T("clip"), 0, TYPE_INT,
    fn_clipEnd, _T("clipEnd"), 0, TYPE_INT, 0, 1,
        _T("clip"), 0, TYPE_INT,
    fn_clipSceneFrames, _T("clipSceneFrames"), 0, TYPE_INT, 0, 1,
        _T("clip"), 0, TYPE_INT,
    fn_clearAnim, _T("clearAnim"), 0, TYPE_INT, 0, 0,
    fn_animDump, _T("animDump"), 0, TYPE_INT, 0, 2,
        _T("clip"), 0, TYPE_INT,
        _T("outPath"), 0, TYPE_STRING,
    fn_sceneAnimDump, _T("sceneAnimDump"), 0, TYPE_INT, 0, 2,
        _T("clip"), 0, TYPE_INT,
        _T("outPath"), 0, TYPE_STRING,

    fn_setTextureDir, _T("setTextureDir"), 0, TYPE_STRING, 0, 1,
        _T("path"), 0, TYPE_STRING,
    fn_exportTextures, _T("exportTextures"), 0, TYPE_INT, 0, 0,
    fn_buildMaterials, _T("buildMaterials"), 0, TYPE_INT, 0, 0,
    fn_slotCount, _T("slotCount"), 0, TYPE_INT, 0, 0,
    fn_slotInfo, _T("slotInfo"), 0, TYPE_STRING, 0, 1,
        _T("index"), 0, TYPE_INT,

    fn_pendingPath, _T("pendingPath"), 0, TYPE_STRING, 0, 0,

    fn_version, _T("version"), 0, TYPE_STRING, 0, 0,
    fn_lastError, _T("lastError"), 0, TYPE_STRING, 0, 0,
    fn_warnings, _T("warnings"), 0, TYPE_STRING, 0, 0,
    fn_log, _T("log"), 0, TYPE_STRING, 0, 0,
    fn_clearLog, _T("clearLog"), 0, TYPE_INT, 0, 0,
    fn_saveLog, _T("saveLog"), 0, TYPE_INT, 0, 1,
        _T("outPath"), 0, TYPE_STRING,
    fn_timings, _T("timings"), 0, TYPE_STRING, 0, 0,
    fn_setDebug, _T("setDebug"), 0, TYPE_INT, 0, 1,
        _T("on"), 0, TYPE_INT,

    p_end
);

BafToolkitInterface* GetBafInterface() {
    return &theBafToolkitInterface;
}

// ============================================================
//  Hilfen
// ============================================================

namespace {

// ------------------------------------------------------------
//  Meldungen zusammensetzen - ohne feste Puffergroesse.
//
//  Die Meldungen enthalten Namen aus der Datei: Modelle, Knoten,
//  Pfade. Deren Laenge steht nicht im Voraus fest. Mit swprintf_s
//  in ein wchar_t[N] geschrieben ist das eine Falle mit Ansage -
//  bei XFBIN 1.9.3 hat genau das 3ds Max beendet: 656 Zeichen in
//  einem 420er-Puffer, swprintf_s ruft den
//  Invalid-Parameter-Handler, und der beendet den Prozess. Kein
//  Nullzeiger weit und breit.
// ------------------------------------------------------------
class Msg {
public:
    template <typename T>
    Msg& operator<<(const T& value) { stream_ << value; return *this; }

    std::wstring str() const { return stream_.str(); }
    operator std::wstring() const { return stream_.str(); }

private:
    std::wostringstream stream_;
};

// ------------------------------------------------------------
//  latin1 -> UTF-16.
//
//  Strings in einer .baf sind latin1, nicht cp932 wie bei XFBIN
//  und nicht UTF-8. Bei latin1 ist die Umwandlung besonders
//  einfach und exakt: die Codepunkte 0 bis 255 sind in Unicode
//  dieselben Zahlen. Ein Byte, ein wchar_t, fertig - hier braucht
//  es kein MultiByteToWideChar.
// ------------------------------------------------------------
std::wstring Latin1ToWide(const std::string& text) {
    std::wstring wide;
    wide.reserve(text.size());
    for (unsigned char byte : text) wide.push_back(static_cast<wchar_t>(byte));
    return wide;
}

// ------------------------------------------------------------
//  UTF-16 -> latin1, das Gegenstueck zu Latin1ToWide.
//
//  Alles ausserhalb von 0..255 wird zu '_'. Das ist kein
//  Notbehelf, sondern die einzige ehrliche Antwort: das Format
//  kann diese Zeichen nicht speichern, und ein stillschweigend
//  verstuemmelter Name waere schlimmer als ein sichtbarer
//  Ersatz.
// ------------------------------------------------------------
std::string WideToLatin1(const std::wstring& text) {
    std::string out;
    out.reserve(text.size());
    for (wchar_t character : text) {
        const unsigned int code = static_cast<unsigned int>(character);
        out.push_back((code < 256u) ? static_cast<char>(code) : '_');
    }
    return out;
}

// ------------------------------------------------------------
//  UTF-16 -> UTF-8, fuer Pfade, die an den SDK-freien Parser
//  gehen. Der nimmt std::string und macht daraus per
//  OpenBinaryRead wieder UTF-16 - der Umweg ist gewollt, denn
//  damit bleibt der Parser frei von Windows-Typen und laesst sich
//  weiter als bafdump gegen die Python-Referenz bauen.
// ------------------------------------------------------------
std::string WideToUtf8(const std::wstring& wide) {
    std::string out;
    out.reserve(wide.size() * 2);
    for (size_t i = 0; i < wide.size(); ++i) {
        uint32_t code = static_cast<uint32_t>(wide[i]);
        // Surrogatpaar zusammensetzen
        if (code >= 0xD800u && code <= 0xDBFFu && i + 1 < wide.size()) {
            const uint32_t low = static_cast<uint32_t>(wide[i + 1]);
            if (low >= 0xDC00u && low <= 0xDFFFu) {
                code = 0x10000u + ((code - 0xD800u) << 10) + (low - 0xDC00u);
                ++i;
            }
        }
        if (code < 0x80u) {
            out.push_back(static_cast<char>(code));
        } else if (code < 0x800u) {
            out.push_back(static_cast<char>(0xC0u | (code >> 6)));
            out.push_back(static_cast<char>(0x80u | (code & 0x3Fu)));
        } else if (code < 0x10000u) {
            out.push_back(static_cast<char>(0xE0u | (code >> 12)));
            out.push_back(static_cast<char>(0x80u | ((code >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (code & 0x3Fu)));
        } else {
            out.push_back(static_cast<char>(0xF0u | (code >> 18)));
            out.push_back(static_cast<char>(0x80u | ((code >> 12) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | ((code >> 6) & 0x3Fu)));
            out.push_back(static_cast<char>(0x80u | (code & 0x3Fu)));
        }
    }
    return out;
}

// Ein Offset als Hexzahl. Von Hand statt ueber std::hex, weil
// der Manipulator im Stringstream haengen bleibt und die naechste
// Zahl in derselben Meldung ebenfalls hexadezimal ausgaebe.
std::wstring HexText(size_t value) {
    static const wchar_t* kDigits = L"0123456789abcdef";
    std::wstring text;
    do {
        text.insert(text.begin(), kDigits[value & 0xF]);
        value >>= 4;
    } while (value != 0);
    return L"0x" + text;
}

// Millisekunden seit einem Startpunkt, als Text mit einer
// Nachkommastelle. Bewusst ueber FormatFloat des Parsers, damit
// auch diese Zahl kein Dezimalkomma bekommt.
std::wstring MillisSince(const std::chrono::steady_clock::time_point& start) {
    const double micros = static_cast<double>(
        std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count());
    const std::string text = baf::FormatFloat(static_cast<float>(micros / 1000.0), 1);
    return std::wstring(text.begin(), text.end()) + L" ms";
}

// ------------------------------------------------------------
//  Hilfen rund um die Zeitrechnung.
//
//  Sie stehen HIER OBEN und nicht bei der Animation, obwohl sie
//  dort hingehoeren wuerden. Der Grund: eine dateilokale
//  Funktion muss in C++ VOR ihrem ersten Aufruf definiert sein,
//  und captureAnim in Stufe 7 benutzt ClipFrameToScene. Stand
//  der Block weiter unten, meldete der Compiler nur
//    C3861: "ClipFrameToScene": Bezeichner wurde nicht gefunden
//  und man sucht an der falschen Stelle.
// ------------------------------------------------------------

// Die Framerate der Szene. GetFrameRate() liefert eine ganze
// Zahl; 0 oder negativ kann eigentlich nicht vorkommen, waere
// hier aber eine Division durch null.
double SceneFps() {
    const int rate = GetFrameRate();
    return (rate > 0) ? static_cast<double>(rate) : 30.0;
}

// Frame des Clips -> Frame der Szene.
//
// Jeder Clip hat SEINE EIGENE Framerate. Ohne diese Umrechnung
// laeuft jede Animation mit der falschen Geschwindigkeit, und
// zwar unauffaellig - es sieht nur "irgendwie zaeh" aus.
double ClipFrameToScene(double clipFrame, double clipFps) {
    if (clipFps <= 0.0) return clipFrame;   // fps 0 kommt vor: dann 1:1
    return clipFrame * SceneFps() / clipFps;
}

// ------------------------------------------------------------
//  Keys eines Controllers zaehlen - rekursiv, aus demselben
//  Grund wie ClearKeys: ein Position_XYZ haelt selbst keine,
//  sondern drei Float-Controller darunter.
// ------------------------------------------------------------
int CountKeys(Animatable* anim) {
    if (anim == nullptr) return 0;
    int total = 0;
    IKeyControl* keys = GetKeyControlInterface(anim);
    if (keys != nullptr) total += keys->GetNumKeys();
    const int count = anim->NumSubs();
    for (int i = 0; i < count; ++i) total += CountKeys(anim->SubAnim(i));
    return total;
}

// ------------------------------------------------------------
//  Alle Keys eines Controllers entfernen - REKURSIV.
//
//  Ein Position_XYZ-Controller haelt selbst keine Keys, sondern
//  drei Float-Controller darunter. GetKeyControlInterface liefert
//  fuer ihn NULL, und ein SetNumKeys(0) darauf tut nichts - die
//  alten Keys blieben stehen und der naechste Clip legte sich
//  darueber.
//
//  Deshalb ueber die Unteranimationen hinabsteigen. Ein
//  Bezier-Float hat keine, dort endet die Rekursion.
// ------------------------------------------------------------
void ClearKeys(Animatable* anim) {
    if (anim == nullptr) return;
    IKeyControl* keys = GetKeyControlInterface(anim);
    if (keys != nullptr) keys->SetNumKeys(0);
    const int count = anim->NumSubs();
    for (int i = 0; i < count; ++i) ClearKeys(anim->SubAnim(i));
}

} // namespace

// ============================================================
//  Interne Helfer der Interface-Klasse
// ============================================================

void BafToolkitInterface::ClearMessages() {
    lastError_.clear();
    warnings_.clear();
    // Das Protokoll wird hier ausdruecklich NICHT geleert. Ein
    // Fehler, den man erst nach zwanzig Minuten bemerkt, waere
    // sonst weg - und mit eingeschaltetem Protokoll neu zu
    // erzeugen gelingt oft nicht.
}

void BafToolkitInterface::Log(const std::wstring& line) {
    log_ += line;
    log_ += L"\n";

    // Gedeckelt, damit ein Stapellauf den Speicher nicht auffrisst.
    // Weg faellt der ALTE Teil, nicht der neue: beim Suchen zaehlt
    // das Ende. Geschnitten wird an einem Zeilenumbruch, sonst
    // beginnt das Protokoll mitten in einem Wort.
    const size_t kLogLimit = 256u * 1024u;
    if (log_.size() > kLogLimit) {
        size_t cut = log_.size() - kLogLimit;
        const size_t newline = log_.find(L'\n', cut);
        if (newline != std::wstring::npos) cut = newline + 1;
        log_.erase(0, cut);
        log_.insert(0, L"[... older part of the log dropped ...]\n");
    }
}

void BafToolkitInterface::LogDetail(const std::wstring& line) {
    if (!debug_) return;
    Log(L"  . " + line);
}

const MCHAR* BafToolkitInterface::Give(const std::wstring& text) {
    returnBuffer_ = text;
    return returnBuffer_.c_str();
}

// ============================================================
//  Archiv
// ============================================================

int BafToolkitInterface::OpenArchive(const MCHAR* path) {
    ClearMessages();
    if (!path) { lastError_ = L"openArchive: no path"; return -1; }

    LogDetail(Msg() << L"openArchive '" << path << L"'");
    const auto started = std::chrono::steady_clock::now();
    archive_.Close();
    modelNames_.clear();
    // Das geoeffnete Modell bleibt bewusst stehen: ein Archivwechsel
    // ist kein Grund, ein bereits gelesenes Modell zu vergessen.

    const baf::LabResult result = archive_.Open(WideToUtf8(path));
    if (!result.ok) {
        lastError_ = Latin1ToWide(result.error);
        Log(L"openArchive FEHLER: " + lastError_);
        return -1;
    }
    modelNames_ = archive_.NamesWithExtension("baf");
    timings_ = L"openArchive: " + MillisSince(started);
    Log(Msg() << L"archive opened: " << archive_.Count() << L" entries, "
              << modelNames_.size() << L" models");
    return static_cast<int>(modelNames_.size());
}

int BafToolkitInterface::CloseArchive() {
    archive_.Close();
    modelNames_.clear();
    Log(L"archive closed");
    return 1;
}

int BafToolkitInterface::ArchiveOpen() { return archive_.IsOpen() ? 1 : 0; }

const MCHAR* BafToolkitInterface::GetArchivePath() {
    return Give(Latin1ToWide(archive_.Path()));
}

int BafToolkitInterface::EntryCount() { return static_cast<int>(archive_.Count()); }
int BafToolkitInterface::ModelCount() { return static_cast<int>(modelNames_.size()); }

const MCHAR* BafToolkitInterface::GetModelNames() {
    std::wstring text;
    for (size_t i = 0; i < modelNames_.size(); ++i) {
        if (i) text += L"\n";
        text += Latin1ToWide(modelNames_[i]);
    }
    return Give(text);
}

// ============================================================
//  Modell
// ============================================================

int BafToolkitInterface::OpenModel(const MCHAR* name) {
    ClearMessages();
    if (!name) { lastError_ = L"openModel: no name"; return -1; }
    if (!archive_.IsOpen()) {
        lastError_ = L"openModel: call openArchive first";
        Log(L"openModel FEHLER: " + lastError_);
        return -1;
    }
    LogDetail(Msg() << L"openModel '" << name << L"'");

    const std::string wanted = WideToUtf8(name);
    const baf::LabEntry* entry = archive_.Find(wanted);
    if (!entry) {
        lastError_ = Msg() << L"openModel: '" << name << L"' is not in the archive";
        Log(L"openModel FEHLER: " + lastError_);
        return -1;
    }

    const auto started = std::chrono::steady_clock::now();
    std::vector<uint8_t> bytes;
    std::string error;
    if (!archive_.ReadBytes(*entry, bytes, error)) {
        lastError_ = Latin1ToWide(error);
        Log(L"openModel FEHLER: " + lastError_);
        return -1;
    }
    LogDetail(Msg() << L"read: " << bytes.size() << L" Byte");
    const auto afterRead = std::chrono::steady_clock::now();

    auto file = std::make_unique<baf::BafFile>();
    const baf::ReadResult result =
        baf::ReadBafBuffer(bytes.data(), bytes.size(), entry->name, *file);
    if (!result.ok) {
        lastError_ = Latin1ToWide(result.error);
        Log(L"openModel FEHLER: " + lastError_);
        return -1;
    }
    warnings_ = Latin1ToWide(result.warnings);
    model_ = std::move(file);
    // Fuer den Rundlauf: gegen diese Bytes wird verglichen.
    sourceBytes_ = std::move(bytes);
    sourceEntry_ = entry->name;

    // Ein neues Modell macht den Szenenzustand des alten
    // gegenstandslos. Die Handles zeigen sonst weiter auf die
    // Bones des vorigen Imports.
    slots_.clear();

    timings_ = L"lesen: " + MillisSince(started) +
               L", parsen: " + MillisSince(afterRead);
    Log(Msg() << L"model opened: " << Latin1ToWide(entry->name) << L", "
              << model_->nodes.size() << L" nodes, "
              << model_->meshes.size() << L" Meshes, "
              << model_->clips.size() << L" Clips");

    // Die Grenzen gleich beim Oeffnen pruefen. Bei einem
    // Retail-Modell steht dort immer "im Rahmen"; auffaellig wird
    // es erst bei einem selbst gebauten - und dann soll es
    // auffallen, bevor jemand eine Stunde in die Szene steckt.
    baf::LimitReport limits;
    baf::CheckLimits(*model_, limits);
    if (limits.exceeded > 0) {
        warnings_ += Latin1ToWide(limits.text);
        Log(Msg() << L"ACHTUNG: " << limits.exceeded
                  << L" engine limit(s) exceeded - the loader of the game "
                     L"will reject the model like this");
    }
    return static_cast<int>(model_->nodes.size());
}

int BafToolkitInterface::OpenFile(const MCHAR* path) {
    ClearMessages();
    if (!path) { lastError_ = L"openFile: no path"; return -1; }

    const auto started = std::chrono::steady_clock::now();
    auto file = std::make_unique<baf::BafFile>();
    const baf::ReadResult result = baf::ReadBafFile(WideToUtf8(path), *file);
    if (!result.ok) {
        lastError_ = Latin1ToWide(result.error);
        return -1;
    }
    warnings_ = Latin1ToWide(result.warnings);
    model_ = std::move(file);
    slots_.clear();
    // Eine lose Datei hat keinen Platz im Archiv.
    sourceEntry_.clear();
    sourceBytes_.clear();
    timings_ = L"openFile: " + MillisSince(started);
    Log(Msg() << L"file opened: " << path << L", "
              << model_->nodes.size() << L" nodes");
    return static_cast<int>(model_->nodes.size());
}

int BafToolkitInterface::Close() {
    model_.reset();
    slots_.clear();
    Log(L"model closed");
    return 1;
}

int BafToolkitInterface::IsOpen() { return model_ ? 1 : 0; }

const MCHAR* BafToolkitInterface::GetSummary() {
    if (!model_) return Give(L"no model open");
    return Give(Latin1ToWide(baf::MakeSummary(*model_)));
}

int BafToolkitInterface::Dump(const MCHAR* outPath, int detail) {
    ClearMessages();
    if (!model_) { lastError_ = L"dump: no model open"; return 0; }
    if (!outPath) { lastError_ = L"dump: no path"; return 0; }

    // std::ofstream nimmt unter MSVC einen wchar_t-Pfad an - damit
    // gilt hier dasselbe wie bei OpenBinaryRead: Umlaute im Pfad
    // funktionieren, ohne dass die Codepage mitspielen muss.
#if defined(_WIN32)
    std::ofstream out(outPath, std::ios::binary);
#else
    std::ofstream out(WideToUtf8(outPath), std::ios::binary);
#endif
    if (!out) {
        lastError_ = Msg() << L"dump: '" << outPath << L"' cannot be written";
        return 0;
    }

    const auto started = std::chrono::steady_clock::now();
    baf::WriteDump(*model_, model_->source, out,
                   static_cast<uint32_t>(detail) & baf::kDumpAll);
    out.close();
    if (!out) {
        lastError_ = L"dump: write aborted - disk full?";
        return 0;
    }
    timings_ = L"dump: " + MillisSince(started);
    Log(Msg() << L"dump written: '" << outPath << L"', Detailgrad " << detail
              << L", " << timings_);
    return 1;
}

// ============================================================
//  Zaehlwerte
// ============================================================

const MCHAR* BafToolkitInterface::GetModelName() {
    return Give(model_ ? Latin1ToWide(model_->name) : std::wstring());
}
float BafToolkitInterface::ModelScale() { return model_ ? model_->scale : 0.0f; }
int BafToolkitInterface::NodeCount()    { return model_ ? static_cast<int>(model_->nodes.size()) : 0; }
int BafToolkitInterface::MeshCount()    { return model_ ? static_cast<int>(model_->meshes.size()) : 0; }
int BafToolkitInterface::MaterialCount(){ return model_ ? static_cast<int>(model_->materials.size()) : 0; }
int BafToolkitInterface::ClipCount()    { return model_ ? static_cast<int>(model_->clips.size()) : 0; }

int BafToolkitInterface::VertexCount() {
    if (!model_) return 0;
    size_t total = 0;
    for (const baf::Mesh& mesh : model_->meshes) total += mesh.positions.size();
    return static_cast<int>(total);
}

int BafToolkitInterface::FaceCount() {
    if (!model_) return 0;
    size_t total = 0;
    for (const baf::Mesh& mesh : model_->meshes) total += mesh.faces.size();
    return static_cast<int>(total);
}

int BafToolkitInterface::KeyCount() {
    if (!model_) return 0;
    size_t total = 0;
    for (const baf::Clip& clip : model_->clips) {
        for (const baf::Track& track : clip.tracks) total += track.keys.size();
    }
    return static_cast<int>(total);
}

// ============================================================
//  Einzelne Knoten und Clips
//
//  Alle Indexzugriffe pruefen den Bereich und geben sonst einen
//  neutralen Wert zurueck. MaxScript ist 1-basiert und der
//  Umrechnungsfehler passiert im Skript, nicht hier - ein
//  Absturz waere die falsche Antwort darauf.
// ============================================================

const MCHAR* BafToolkitInterface::GetNodeName(int index) {
    if (!model_ || index < 0 || static_cast<size_t>(index) >= model_->nodes.size()) {
        return Give(std::wstring());
    }
    return Give(Latin1ToWide(model_->nodes[static_cast<size_t>(index)].name));
}

int BafToolkitInterface::NodeParent(int index) {
    if (!model_ || index < 0 || static_cast<size_t>(index) >= model_->nodes.size()) return -1;
    return model_->nodes[static_cast<size_t>(index)].parent;
}

int BafToolkitInterface::NodeMesh(int index) {
    if (!model_ || index < 0 || static_cast<size_t>(index) >= model_->nodes.size()) return -1;
    return model_->nodes[static_cast<size_t>(index)].meshIndex;
}

const MCHAR* BafToolkitInterface::GetClipName(int index) {
    if (!model_ || index < 0 || static_cast<size_t>(index) >= model_->clips.size()) {
        return Give(std::wstring());
    }
    return Give(Latin1ToWide(model_->clips[static_cast<size_t>(index)].name));
}

float BafToolkitInterface::ClipFps(int index) {
    if (!model_ || index < 0 || static_cast<size_t>(index) >= model_->clips.size()) return 0.0f;
    return model_->clips[static_cast<size_t>(index)].fps;
}

int BafToolkitInterface::ClipFrames(int index) {
    if (!model_ || index < 0 || static_cast<size_t>(index) >= model_->clips.size()) return 0;
    return model_->clips[static_cast<size_t>(index)].numFrames;
}

int BafToolkitInterface::ClipKeys(int index) {
    if (!model_ || index < 0 || static_cast<size_t>(index) >= model_->clips.size()) return 0;
    size_t total = 0;
    for (const baf::Track& track : model_->clips[static_cast<size_t>(index)].tracks) {
        total += track.keys.size();
    }
    return static_cast<int>(total);
}

// ============================================================
//  Stufe 2 - Skelett
// ============================================================

INode* BafToolkitInterface::NodeFromHandle(ULONG handle) const {
    if (handle == 0) return nullptr;
    Interface* ip = GetCOREInterface();
    if (ip == nullptr) return nullptr;
    // GetINodeByHandle liefert NULL, wenn es den Knoten nicht mehr
    // gibt - anders als ein gemerkter INode*, der dann auf
    // freigegebenen Speicher zeigt.
    //
    // NICHT Animatable::GetAnimByHandle: das erwartet ein
    // AnimHandle, und INode::GetHandle() liefert ein
    // Knoten-Handle. Zwei verschiedene Nummernwelten - der Wert
    // aus der einen in der anderen nachgeschlagen trifft
    // bestenfalls nichts.
    return ip->GetINodeByHandle(handle);
}

float BafToolkitInterface::SetScale(float scale) {
    if (scale > 0.0f) scale_ = scale;
    return scale_;
}

float BafToolkitInterface::SetBoneSize(float size) {
    if (size >= 0.0f) boneSize_ = size;
    return boneSize_;
}

int BafToolkitInterface::SceneBoneCount() {
    int alive = 0;
    for (ULONG handle : boneHandles_) {
        if (NodeFromHandle(handle) != nullptr) ++alive;
    }
    return alive;
}

int BafToolkitInterface::ClearScene() {
    Interface* ip = GetCOREInterface();
    int removed = 0;
    if (ip != nullptr) {
        // Rueckwaerts loeschen: so kommen die Kinder vor ihren
        // Eltern dran, und Max muss nicht bei jedem Elternteil die
        // Kindliste neu haengen.
        // Erst die Meshes, dann die Bones: die Meshes haengen an
        // den Bones, und ein geloeschter Elternteil nimmt sie sonst
        // gleich mit - dann zaehlt removed falsch.
        // Zuerst die Huellkugeln: sie haengen an den Meshes.
        for (size_t i = boundsHandles_.size(); i-- > 0;) {
            INode* node = NodeFromHandle(boundsHandles_[i]);
            if (node != nullptr) { ip->DeleteNode(node, FALSE); ++removed; }
        }
        for (size_t i = meshHandles_.size(); i-- > 0;) {
            INode* node = NodeFromHandle(meshHandles_[i]);
            if (node != nullptr) { ip->DeleteNode(node, FALSE); ++removed; }
        }
        for (size_t i = boneHandles_.size(); i-- > 0;) {
            INode* node = NodeFromHandle(boneHandles_[i]);
            if (node != nullptr) { ip->DeleteNode(node, FALSE); ++removed; }
        }
    }
    boneHandles_.clear();
    meshHandles_.clear();
    boundsHandles_.clear();
    sceneModelName_.clear();
    materialHandle_ = 0;
    if (removed > 0) Log(Msg() << L"clearScene: " << removed << L" objects removed");
    return removed;
}

int BafToolkitInterface::BuildSkeleton() {
    ClearMessages();
    if (!model_) { lastError_ = L"buildSkeleton: no model open"; return 0; }
    if (model_->nodes.empty()) { lastError_ = L"buildSkeleton: the model has no nodes"; return 0; }

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) { lastError_ = L"buildSkeleton: no core interface"; return 0; }

    Log(Msg() << L"buildSkeleton: start, " << model_->nodes.size()
              << L" nodes, scale " << Latin1ToWide(baf::FormatFloat(scale_, 3)));
    const auto started = std::chrono::steady_clock::now();

    // Ein frueher Import derselben Sitzung wird ersetzt, nicht
    // verdoppelt - sonst sammeln sich Karteileichen an.
    const int cleared = ClearScene();
    if (cleared > 0) LogDetail(Msg() << L"cleared first: " << cleared << L" Objekte");

    // Hier verdienen sich die beiden Guards ihr Geld: ohne sie
    // gaebe es je Knoten einen Undo-Record und ein
    // Viewport-Update.
    SceneRedrawGuard redrawGuard(ip);
    HoldSuspendGuard holdGuard;

    std::vector<baf::Matrix34> world;
    baf::ComputeRestWorld(*model_, static_cast<double>(scale_), world);
    // Das ganze Rig auf einmal drehen, nicht nur die Wurzel: die
    // Weltmatrizen sind hier schon ausgerechnet, und eine Drehung
    // um den Ursprung wirkt auf alle gleich.
    if (faceFront_) {
        const baf::Matrix34 turn = FrontRotation();
        for (baf::Matrix34& tm : world) tm = baf::Concat(turn, tm);
    }

    std::vector<INode*> made(model_->nodes.size(), nullptr);
    boneHandles_.assign(model_->nodes.size(), 0);

    int created = 0, failed = 0;
    std::wstring firstFailure;

    for (size_t i = 0; i < model_->nodes.size(); ++i) {
        const baf::Node& node = model_->nodes[i];

        Object* object = static_cast<Object*>(
            ip->CreateInstance(GEOMOBJECT_CLASS_ID, BONE_OBJ_CLASSID));

        // Rueckfallebene: Point-Helper. Der braucht keine
        // Parameterblock-Kenntnis, sieht mit ShowBone wie ein Bone
        // aus, und alles Weitere behandelt ihn genauso.
        if (object == nullptr) {
            object = static_cast<Object*>(
                ip->CreateInstance(HELPER_CLASS_ID, Class_ID(POINTHELP_CLASS_ID, 0)));
        }
        if (object == nullptr) {
            ++failed;
            if (firstFailure.empty()) firstFailure = Latin1ToWide(node.name);
            continue;
        }

        INode* sceneNode = ip->CreateObjectNode(object);
        if (sceneNode == nullptr) {
            ++failed;
            if (firstFailure.empty()) firstFailure = Latin1ToWide(node.name);
            continue;
        }

        std::wstring name = Latin1ToWide(node.name);
        if (name.empty()) name = L"node" + std::to_wstring(i);
        sceneNode->SetName(const_cast<MCHAR*>(name.c_str()));

        // Matrix34 ist bereits so angeordnet, wie SetRow es haben
        // will: m[0..2] Basis, m[3] Translation. Deshalb hier nur
        // die Umwandlung double -> float, kein Umsortieren. Das
        // Blender-Addon setzt dieselben Vektoren als SPALTEN ein -
        // Max rechnet mit Zeilenvektoren, und die Anordnung in
        // Matrix34 ist schon die Transponierte davon.
        Matrix3 tm(1);
        for (int row = 0; row < 4; ++row) {
            tm.SetRow(row, Point3(static_cast<float>(world[i].m[row][0]),
                                  static_cast<float>(world[i].m[row][1]),
                                  static_cast<float>(world[i].m[row][2])));
        }
        sceneNode->SetNodeTM(0, tm);

        sceneNode->ShowBone(1);
        sceneNode->SetBoneNodeOnOff(TRUE, 0);
        sceneNode->SetRenderable(FALSE);

        // Breite und Hoehe der Bone-Kaesten. Max legt sie mit 4 an;
        // bei einem 1,8 Einheiten grossen Charakter verdeckt das
        // alles. 0 zeichnet blosse Linien.
        if (boneSize_ >= 0.0f) {
            Object* base = sceneNode->GetObjectRef();
            if (base != nullptr) base = base->FindBaseObject();
            if (base != nullptr) {
                // GetParamBlockByID, nicht GetParamBlock(0):
                // BaseObject erbt ein parameterloses
                // GetParamBlock(), das ein IParamArray* liefert und
                // die gewuenschte Fassung VERDECKT. Man stolpert
                // sonst ueber "akzeptiert keine 1 Argumente".
                IParamBlock2* block = base->GetParamBlockByID(0);
                if (block == nullptr) block = base->Animatable::GetParamBlock(0);
                if (block != nullptr) {
                    block->SetValue(0, 0, boneSize_);   // boneobj_width
                    block->SetValue(1, 0, boneSize_);   // boneobj_height
                }
            }
        }

        // Rohdaten an den Bone. Am ersten Bone stehen zusaetzlich
        // die Kopfangaben und die Clip-Metadaten - dort sucht man
        // sie, und ein eigenes Hilfsobjekt dafuer waere ein Objekt
        // mehr in der Szene.
        if (storeProps_) {
            std::string props = baf::NodePropText(*model_, i);
            if (i == 0) {
                props += baf::ModelPropText(*model_);
                // Der Archivpfad gehoert dazu, auch wenn er kein
                // Datenfeld der .baf ist: ohne ihn findet der
                // Export die Vorlage spaeter nicht wieder.
                if (!sourceEntry_.empty() && archive_.IsOpen()) {
                    props += "bafArchive = " + archive_.Path() + "\n";
                }
                for (size_t c = 0; c < model_->clips.size(); ++c) {
                    props += baf::ClipPropText(*model_, c);
                }
            }
            const std::wstring wide = Latin1ToWide(props);
            sceneNode->SetUserPropBuffer(wide.c_str());
        }

        made[i] = sceneNode;
        boneHandles_[i] = sceneNode->GetHandle();
        LogDetail(Msg() << L"Bone " << i << L" '" << name << L"' Eltern "
                        << node.parent);

        // Ein Elternteil steht in der Datei immer vor seinem Kind,
        // ist hier also schon angelegt.
        if (node.parent >= 0 && static_cast<size_t>(node.parent) < i) {
            INode* parentNode = made[static_cast<size_t>(node.parent)];
            if (parentNode != nullptr) {
                // keepTM = 1: die eben gesetzte Weltmatrix bleibt
                // stehen, Max rechnet die lokale daraus zurueck.
                parentNode->AttachChild(sceneNode, 1);
            }
        }
        ++created;
    }

    sceneModelName_ = Latin1ToWide(model_->name);
    timings_ = L"buildSkeleton: " + MillisSince(started);

    if (failed > 0) {
        warnings_ += (Msg() << failed << L" of " << model_->nodes.size()
                            << L" bones could not be created, the first being '"
                            << firstFailure << L"'\n").str();
    }

    // Doppelte Namen sind spaeter beim Zuordnen der Meshes ein
    // Problem, deshalb hier schon melden statt es dann zu suchen.
    std::set<std::wstring> seen;
    int duplicates = 0;
    for (const baf::Node& node : model_->nodes) {
        if (!seen.insert(Latin1ToWide(node.name)).second) ++duplicates;
    }
    if (duplicates > 0) {
        warnings_ += (Msg() << L"the model has " << duplicates
                            << L" duplicate node names\n").str();
    }

    Log(Msg() << L"skeleton built: " << created << L" bones, Faktor "
              << Latin1ToWide(baf::FormatFloat(scale_, 3)));
    return created;
}

int BafToolkitInterface::SceneMeshCount() {
    int alive = 0;
    for (ULONG handle : meshHandles_) {
        if (NodeFromHandle(handle) != nullptr) ++alive;
    }
    return alive;
}

int BafToolkitInterface::BuildMeshes() {
    ClearMessages();
    if (!model_) { lastError_ = L"buildMeshes: no model open"; return 0; }
    if (model_->meshes.empty()) { lastError_ = L"buildMeshes: the model has no meshes"; return 0; }
    if (boneHandles_.empty()) {
        lastError_ = L"buildMeshes: call buildSkeleton first";
        return 0;
    }

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) { lastError_ = L"buildMeshes: no core interface"; return 0; }

    Log(Msg() << L"buildMeshes: start, " << model_->meshes.size() << L" Meshes");
    const auto started = std::chrono::steady_clock::now();

    // Frueher angelegte Meshes ersetzen, das Skelett stehen lassen.
    for (size_t i = boundsHandles_.size(); i-- > 0;) {
        INode* node = NodeFromHandle(boundsHandles_[i]);
        if (node != nullptr) ip->DeleteNode(node, FALSE);
    }
    boundsHandles_.clear();
    for (size_t i = meshHandles_.size(); i-- > 0;) {
        INode* node = NodeFromHandle(meshHandles_[i]);
        if (node != nullptr) ip->DeleteNode(node, FALSE);
    }
    meshHandles_.assign(model_->meshes.size(), 0);

    SceneRedrawGuard redrawGuard(ip);
    HoldSuspendGuard holdGuard;

    std::vector<baf::Matrix34> world;
    baf::ComputeRestWorld(*model_, static_cast<double>(scale_), world);
    if (faceFront_) {
        const baf::Matrix34 turn = FrontRotation();
        for (baf::Matrix34& tm : world) tm = baf::Concat(turn, tm);
    }

    // Die Materialplaetze werden HIER schon gebraucht: die
    // Material-ID eines Faces zeigt auf einen Platz, nicht auf
    // einen Materialindex. buildMaterials rechnet dieselbe Tabelle
    // aus, deterministisch, also passen die Nummern zusammen -
    // auch wenn buildMaterials gar nicht laeuft.
    baf::BuildMaterialSlots(*model_, slots_);
    LogDetail(Msg() << L"material slots: " << slots_.size());

    int created = 0, skippedTotal = 0, orphans = 0;

    for (size_t meshIndex = 0; meshIndex < model_->meshes.size(); ++meshIndex) {
        baf::BuiltMesh built;
        baf::BuildMeshGeometry(*model_, meshIndex, static_cast<double>(scale_),
                               slots_, built);
        skippedTotal += static_cast<int>(built.skippedFaces.size());

        if (built.positions.empty() || built.tris.empty()) continue;

        TriObject* triObject = CreateNewTriObject();
        if (triObject == nullptr) {
            warnings_ += (Msg() << L"Mesh '" << Latin1ToWide(built.name)
                                << L"': could not create the TriObject\n").str();
            continue;
        }

        Mesh& maxMesh = triObject->GetMesh();
        const int vertexCount = static_cast<int>(built.positions.size());
        const int faceCount   = static_cast<int>(built.tris.size());
        maxMesh.setNumVerts(vertexCount);
        maxMesh.setNumFaces(faceCount);

        for (int i = 0; i < vertexCount; ++i) {
            maxMesh.setVert(i, Point3(built.positions[static_cast<size_t>(i)].x,
                                      built.positions[static_cast<size_t>(i)].y,
                                      built.positions[static_cast<size_t>(i)].z));
        }

        // Kanal 1 sind die UVs (Kanal 0 waere die Vertexfarbe).
        //
        // EINE Texturkoordinate JE ECKE, nicht je Vertex: die UVs
        // der Datei haengen am Face, nicht am Vertex, und die
        // Cel-Groesse - durch die geteilt wird - kann innerhalb
        // eines Mesh wechseln. Ein gemeinsamer TVert-Vorrat waere
        // hier schlicht falsch.
        maxMesh.setMapSupport(1, TRUE);
        maxMesh.setNumMapVerts(1, faceCount * 3);

        for (int f = 0; f < faceCount; ++f) {
            const baf::BuiltTri& tri = built.tris[static_cast<size_t>(f)];
            for (int corner = 0; corner < 3; ++corner) {
                maxMesh.setMapVert(1, f * 3 + corner,
                                   UVVert(tri.uv[corner][0], tri.uv[corner][1], 0.0f));
            }
            Face& face = maxMesh.faces[f];
            face.setVerts(static_cast<int>(tri.vertex[0]),
                          static_cast<int>(tri.vertex[1]),
                          static_cast<int>(tri.vertex[2]));
            face.setEdgeVisFlags(1, 1, 1);
            // Eine Glaettungsgruppe fuer alles. Ohne gesetzte
            // Gruppe waere das Modell durchgehend facettiert.
            face.setSmGroup(1);
            // Die Material-ID zeigt schon auf das Material der
            // Datei. Stufe 4 haengt daran das Multi/Sub-Object.
            face.setMatID(static_cast<MtlID>(tri.slot));
            maxMesh.mapFaces(1)[f].setTVerts(f * 3, f * 3 + 1, f * 3 + 2);
        }

        // ------------------------------------------------------
        //  Vorgegebene Vertexnormalen
        //
        //  Die Engine beleuchtet je Vertex (Gouraud), und die
        //  Normalen stehen in der Datei. Ohne sie rechnet Max
        //  eigene aus den Glaettungsgruppen - das Modell sieht
        //  dann anders aus als im Spiel, meist an den Uebergaengen
        //  zwischen Koerperteilen.
        //
        //  Die Zuordnung ist hier einfach: eine Normale je Vertex,
        //  jede Ecke zeigt auf ihren eigenen Vertexindex.
        // ------------------------------------------------------
        if (importNormals_ && built.normals.size() == built.positions.size()) {
            maxMesh.SpecifyNormals();
            MeshNormalSpec* normals = maxMesh.GetSpecifiedNormals();
            if (normals != nullptr) {
                normals->ClearAndFree();
                normals->SetNumFaces(faceCount);
                normals->SetNumNormals(vertexCount);
                for (int i = 0; i < vertexCount; ++i) {
                    const baf::Vec3& source = built.normals[static_cast<size_t>(i)];
                    Point3 normal(source.x, source.y, source.z);
                    // Eine Nullnormale kommt in den Daten vor.
                    // Normalisieren wuerde durch null teilen, also
                    // ein Ersatzwert - genauso macht es das Addon.
                    if (Length(normal) > 1e-6f) normal = Normalize(normal);
                    else normal = Point3(0.0f, 0.0f, 1.0f);
                    normals->Normal(i) = normal;
                    normals->SetNormalExplicit(i, true);
                }
                for (int f = 0; f < faceCount; ++f) {
                    for (int corner = 0; corner < 3; ++corner) {
                        normals->Face(f).SpecifyNormalID(
                            corner, maxMesh.faces[f].getVert(corner));
                    }
                }
                normals->SetFlag(MESH_NORMAL_NORMALS_BUILT, true);
                normals->CheckNormals();
            }
        }

        // Die Intensitaet je Vertex, als Kanal 2. Sie ist die
        // Alphaquelle des zweiten Ladedurchgangs und laesst sich so
        // im Viewport ansehen, statt nur als Text dazustehen.
        if (storeProps_ && meshIndex < model_->meshes.size()) {
            const std::vector<float>& intensities =
                model_->meshes[meshIndex].intensities;
            if (intensities.size() == static_cast<size_t>(vertexCount)) {
                maxMesh.setMapSupport(2, TRUE);
                maxMesh.setNumMapVerts(2, vertexCount);
                for (int i = 0; i < vertexCount; ++i) {
                    const float value = intensities[static_cast<size_t>(i)];
                    maxMesh.setMapVert(2, i, UVVert(value, value, value));
                }
                for (int f = 0; f < faceCount; ++f) {
                    maxMesh.mapFaces(2)[f].setTVerts(maxMesh.faces[f].getVert(0),
                                                     maxMesh.faces[f].getVert(1),
                                                     maxMesh.faces[f].getVert(2));
                }
            }
        }

        maxMesh.InvalidateGeomCache();

        INode* meshNode = ip->CreateObjectNode(triObject);
        if (meshNode == nullptr) {
            warnings_ += (Msg() << L"Mesh '" << Latin1ToWide(built.name)
                                << L"': could not create the node\n").str();
            continue;
        }

        std::wstring name = Latin1ToWide(built.name);
        if (name.empty()) name = L"mesh" + std::to_wstring(meshIndex);
        meshNode->SetName(const_cast<MCHAR*>(name.c_str()));

        // Die Vertices liegen knotenlokal. Das Objekt bekommt also
        // die Weltmatrix seines Bones, dann stimmt die Lage.
        const size_t owner = (built.nodeIndex >= 0)
            ? static_cast<size_t>(built.nodeIndex) : SIZE_MAX;
        if (owner < world.size()) {
            Matrix3 tm(1);
            for (int row = 0; row < 4; ++row) {
                tm.SetRow(row, Point3(static_cast<float>(world[owner].m[row][0]),
                                      static_cast<float>(world[owner].m[row][1]),
                                      static_cast<float>(world[owner].m[row][2])));
            }
            meshNode->SetNodeTM(0, tm);

            INode* boneNode = (owner < boneHandles_.size())
                ? NodeFromHandle(boneHandles_[owner]) : nullptr;
            // keepTM = 1: die eben gesetzte Weltmatrix bleibt stehen.
            if (boneNode != nullptr) boneNode->AttachChild(meshNode, 1);
        } else {
            ++orphans;
        }

        // geometryMode == 0 heisst: die Engine zeichnet das Mesh
        // nie - das sind die verstauten Umhaenge. Ausblenden statt
        // weglassen, sonst fehlt es beim Export.
        if (!built.drawn && hideUndrawn_) meshNode->Hide(TRUE);

        // Die Rohdaten der Datei an das Objekt haengen. Sie stehen
        // danach unter Objekteigenschaften -> Benutzerdefiniert.
        if (storeProps_) {
            const std::wstring props =
                Latin1ToWide(baf::MeshPropText(*model_, meshIndex, built));
            meshNode->SetUserPropBuffer(props.c_str());
        }

        // Die mitgelieferte Huellkugel als Helfer. Beim Kopf ist
        // das die Kugel, die der Blickrichtungs-Leser der KI
        // transformiert - er rechnet sie nicht nach.
        if (showBounds_ && meshIndex < model_->meshes.size()) {
            const baf::Mesh& source = model_->meshes[meshIndex];
            Object* helper = static_cast<Object*>(
                ip->CreateInstance(HELPER_CLASS_ID, Class_ID(POINTHELP_CLASS_ID, 0)));
            INode* boundsNode = (helper != nullptr) ? ip->CreateObjectNode(helper) : nullptr;
            if (boundsNode != nullptr) {
                const std::wstring boundsName = L"BOUNDS_" + name;
                boundsNode->SetName(const_cast<MCHAR*>(boundsName.c_str()));
                Matrix3 boundsTm = meshNode->GetNodeTM(0);
                // Mittelpunkt in Knotenkoordinaten, also mit
                // demselben Pivot-Versatz wie die Vertices.
                const baf::Node* owner = model_->NodeOfMesh(static_cast<int32_t>(meshIndex));
                const baf::Vec3 pivot = (owner != nullptr) ? owner->pivot : baf::Vec3{};
                const Point3 centre(
                    static_cast<float>((source.center.x + pivot.x) * scale_),
                    static_cast<float>((source.center.y + pivot.y) * scale_),
                    static_cast<float>((source.center.z + pivot.z) * scale_));
                boundsTm.PreTranslate(centre);
                boundsNode->SetNodeTM(0, boundsTm);
                boundsNode->SetRenderable(FALSE);
                boundsNode->SetUserPropBuffer(
                    (Msg() << L"bafBoundsRadius = "
                           << Latin1ToWide(baf::FormatFloat(source.radius * scale_))
                           << L"\n").str().c_str());
                meshNode->AttachChild(boundsNode, 1);
                boundsHandles_.push_back(boundsNode->GetHandle());
            }
        }

        meshHandles_[meshIndex] = meshNode->GetHandle();
        ++created;
        LogDetail(Msg() << L"Mesh " << meshIndex << L" '" << name << L"' an nodes "
                        << built.nodeIndex << L", " << built.positions.size()
                        << L" Vertices, " << built.tris.size() << L" triangles"
                        << (built.drawn ? L"" : L", ausgeblendet"));
    }

    timings_ = L"buildMeshes: " + MillisSince(started);
    if (skippedTotal > 0) {
        warnings_ += (Msg() << skippedTotal
                            << L" degenerate faces skipped (repeated vertex)\n").str();
    }
    if (orphans > 0) {
        warnings_ += (Msg() << orphans
                            << L" meshes belong to no node\n").str();
    }
    Log(Msg() << L"meshes built: " << created);
    return created;
}

int BafToolkitInterface::BuildDump(const MCHAR* outPath) {
    ClearMessages();
    if (!model_) { lastError_ = L"buildDump: no model open"; return 0; }
    if (!outPath) { lastError_ = L"buildDump: no path"; return 0; }

#if defined(_WIN32)
    std::ofstream out(outPath, std::ios::binary);
#else
    std::ofstream out(WideToUtf8(outPath), std::ios::binary);
#endif
    if (!out) {
        lastError_ = Msg() << L"buildDump: '" << outPath << L"' cannot be written";
        return 0;
    }
    std::vector<baf::MaterialSlot> slots;
    baf::BuildMaterialSlots(*model_, slots);
    for (size_t i = 0; i < model_->meshes.size(); ++i) {
        baf::BuiltMesh built;
        baf::BuildMeshGeometry(*model_, i, static_cast<double>(scale_), slots, built);
        baf::WriteBuildDump(built, out);
    }
    out.close();
    if (!out) { lastError_ = L"buildDump: write aborted"; return 0; }
    Log(Msg() << L"buildDump: " << model_->meshes.size() << L" meshes to '"
              << outPath << L"'");
    return 1;
}

int BafToolkitInterface::SceneMeshDump(const MCHAR* outPath) {
    ClearMessages();
    if (!model_) { lastError_ = L"sceneMeshDump: no model open"; return 0; }
    if (!outPath) { lastError_ = L"sceneMeshDump: no path"; return 0; }
    if (meshHandles_.empty()) { lastError_ = L"sceneMeshDump: no meshes built"; return 0; }

#if defined(_WIN32)
    std::ofstream out(outPath, std::ios::binary);
#else
    std::ofstream out(WideToUtf8(outPath), std::ios::binary);
#endif
    if (!out) {
        lastError_ = Msg() << L"sceneMeshDump: '" << outPath
                           << L"' cannot be written";
        return 0;
    }

    std::vector<baf::MaterialSlot> sceneSlots;
    baf::BuildMaterialSlots(*model_, sceneSlots);

    int missing = 0;
    for (size_t meshIndex = 0; meshIndex < meshHandles_.size(); ++meshIndex) {
        // Der Rahmen kommt aus den Dateidaten, der INHALT aus der
        // Szene. Sonst prueft der Vergleich nur, ob dieselbe
        // Funktion zweimal dasselbe ausrechnet.
        baf::BuiltMesh built;
        baf::BuildMeshGeometry(*model_, meshIndex, static_cast<double>(scale_),
                               sceneSlots, built);
        built.positions.clear();
        built.tris.clear();

        INode* node = NodeFromHandle(meshHandles_[meshIndex]);
        ObjectState state;
        TriObject* triObject = nullptr;
        if (node != nullptr) {
            state = node->EvalWorldState(0);
            if (state.obj != nullptr && state.obj->CanConvertToType(triObjectClassID)) {
                triObject = static_cast<TriObject*>(
                    state.obj->ConvertToType(0, triObjectClassID));
            }
        }
        if (triObject == nullptr) {
            if (node != nullptr) ++missing;
        } else {
            Mesh& maxMesh = triObject->GetMesh();
            built.positions.reserve(static_cast<size_t>(maxMesh.getNumVerts()));
            for (int i = 0; i < maxMesh.getNumVerts(); ++i) {
                const Point3 point = maxMesh.getVert(i);
                built.positions.push_back(baf::Vec3{ point.x, point.y, point.z });
            }
            const bool hasUv = maxMesh.mapSupport(1) && maxMesh.getNumMapVerts(1) > 0;
            for (int f = 0; f < maxMesh.getNumFaces(); ++f) {
                baf::BuiltTri tri;
                // Nicht const: Face::getVert und Face::getMatID sind
                // im SDK nicht als const deklariert. Ueber ein
                // const& sind sie deshalb nicht aufrufbar.
                Face& face = maxMesh.faces[f];
                for (int corner = 0; corner < 3; ++corner) {
                    tri.vertex[corner] = static_cast<uint32_t>(face.getVert(corner));
                    if (hasUv) {
                        const int mapIndex = maxMesh.mapFaces(1)[f].getTVert(corner);
                        if (mapIndex >= 0 && mapIndex < maxMesh.getNumMapVerts(1)) {
                            const UVVert uv = maxMesh.mapVerts(1)[mapIndex];
                            tri.uv[corner][0] = uv.x;
                            tri.uv[corner][1] = uv.y;
                        }
                    }
                }
                tri.slot = face.getMatID();
                // Der Materialindex ist kein Szenendatum - Max
                // kennt nur die Platznummer. Er kommt aus der
                // Tabelle zurueck.
                tri.material = (tri.slot < sceneSlots.size())
                    ? sceneSlots[tri.slot].material : 0u;
                built.tris.push_back(tri);
            }
            // Die Quellface-Nummer kennt die Szene nicht - sie ist
            // kein Szenendatum. Sie kommt aus der Aufbereitung
            // zurueck, damit die Zeilen vergleichbar bleiben.
            baf::BuiltMesh reference;
            baf::BuildMeshGeometry(*model_, meshIndex, static_cast<double>(scale_),
                                   sceneSlots, reference);
            for (size_t t = 0; t < built.tris.size() && t < reference.tris.size(); ++t) {
                built.tris[t].sourceFace = reference.tris[t].sourceFace;
            }
            if (triObject != state.obj) triObject->DeleteMe();
        }
        baf::WriteBuildDump(built, out);
    }

    out.close();
    if (!out) { lastError_ = L"sceneMeshDump: write aborted"; return 0; }
    if (missing > 0) {
        warnings_ = (Msg() << missing << L" meshes are no longer in the scene\n").str();
    }
    Log(Msg() << L"sceneMeshDump: " << meshHandles_.size() << L" Meshes, " << missing
              << L" missing, to '" << outPath << L"'");
    return 1;
}

int BafToolkitInterface::RestDump(const MCHAR* outPath) {
    ClearMessages();
    if (!model_) { lastError_ = L"restDump: no model open"; return 0; }
    if (!outPath) { lastError_ = L"restDump: no path"; return 0; }

#if defined(_WIN32)
    std::ofstream out(outPath, std::ios::binary);
#else
    std::ofstream out(WideToUtf8(outPath), std::ios::binary);
#endif
    if (!out) {
        lastError_ = Msg() << L"restDump: '" << outPath << L"' cannot be written";
        return 0;
    }

    std::vector<baf::Matrix34> world;
    baf::ComputeRestWorld(*model_, static_cast<double>(scale_), world);
    std::vector<std::string> names;
    names.reserve(model_->nodes.size());
    for (const baf::Node& node : model_->nodes) names.push_back(node.name);

    baf::WriteRestDump(world, names, out);
    out.close();
    if (!out) { lastError_ = L"restDump: write aborted"; return 0; }
    Log(Msg() << L"restDump: " << world.size() << L" lines to '" << outPath << L"'");
    return 1;
}

int BafToolkitInterface::SceneBoneDump(const MCHAR* outPath) {
    ClearMessages();
    if (!outPath) { lastError_ = L"sceneBoneDump: no path"; return 0; }
    if (boneHandles_.empty()) {
        lastError_ = L"sceneBoneDump: no skeleton built";
        return 0;
    }

#if defined(_WIN32)
    std::ofstream out(outPath, std::ios::binary);
#else
    std::ofstream out(WideToUtf8(outPath), std::ios::binary);
#endif
    if (!out) {
        lastError_ = Msg() << L"sceneBoneDump: '" << outPath
                           << L"' cannot be written";
        return 0;
    }

    // Aus der SZENE zurueckgelesen, nicht aus den Dateidaten neu
    // gerechnet. Sonst prueft der Vergleich nur, ob dieselbe
    // Funktion zweimal dasselbe ausrechnet - und nicht, ob in Max
    // tatsaechlich das steht, was da stehen soll.
    std::vector<baf::Matrix34> world;
    std::vector<std::string> names;
    world.reserve(boneHandles_.size());
    names.reserve(boneHandles_.size());

    int missing = 0;
    for (size_t i = 0; i < boneHandles_.size(); ++i) {
        baf::Matrix34 tm{};
        std::string name;
        INode* node = NodeFromHandle(boneHandles_[i]);
        if (node == nullptr) {
            ++missing;
            // Geloeschter Knoten: als Nullzeile ausgeben statt zu
            // ueberspringen. Sonst verschieben sich alle folgenden
            // Zeilen und der Vergleich meldet lauter Unterschiede,
            // wo nur einer ist.
        } else {
            const Matrix3 maxTm = node->GetNodeTM(0);
            for (int row = 0; row < 4; ++row) {
                const Point3 point = maxTm.GetRow(row);
                tm.m[row][0] = point.x;
                tm.m[row][1] = point.y;
                tm.m[row][2] = point.z;
            }
            if (i < model_->nodes.size()) name = model_->nodes[i].name;
        }
        world.push_back(tm);
        names.push_back(name);
    }

    baf::WriteRestDump(world, names, out);
    out.close();
    if (!out) { lastError_ = L"sceneBoneDump: write aborted"; return 0; }
    if (missing > 0) {
        warnings_ = (Msg() << missing << L" bones are no longer in the scene\n").str();
    }
    Log(Msg() << L"sceneBoneDump: " << world.size() << L" lines, " << missing
              << L" missing, to '" << outPath << L"'");
    return 1;
}

// ============================================================
//  Stufe 7 - Export
// ============================================================

const MCHAR* BafToolkitInterface::GetExportReport() {
    return Give(exportReport_.empty() ? std::wstring(L"nothing taken over yet")
                                      : exportReport_);
}

// ------------------------------------------------------------
//  Die Szene zurueck in das geladene Modell.
//
//  AN ORT UND STELLE: Positionen, Normalen und UVs werden
//  uebernommen, die Topologie nicht. Wer in Max Faces hinzufuegt
//  oder loescht, bekommt eine Meldung statt einer stillschweigend
//  falschen Datei.
//
//  Das ist Absicht und keine Faulheit. Ein Mesh mit geaenderter
//  Topologie braucht neue Face-Saetze, neue Indexbloecke und eine
//  neue Texturvertex-Tabelle - und dabei geht jedes Wortfeld
//  verloren, das nicht aus der Szene rekonstruierbar ist. Solange
//  der Rundlauf nicht in Max geprueft ist, waere das der falsche
//  Zeitpunkt dafuer.
// ------------------------------------------------------------
// ============================================================
//  Stufe 11 - die Szene untersuchen
//
//  Der Export darf nicht davon abhaengen, dass dieselbe
//  Max-Sitzung vorher importiert hat. Wer eine Szene speichert,
//  Max schliesst und am naechsten Tag weiterarbeitet, hat kein
//  Modell mehr im Speicher - aber die SZENE weiss weiter, woher
//  sie kommt: an jedem Bone stehen die Rohdaten aus Stufe 6, und
//  am ersten steht der Modellname und der Archivpfad.
//
//  scanScene liest das zurueck und stellt die Zuordnung wieder
//  her. Sie laeuft ueber bafNodeIndex und bafMeshIndex, nicht
//  ueber Handles - Handles gelten nur innerhalb einer Sitzung,
//  die Rohdaten ueberleben alles.
// ============================================================

// ------------------------------------------------------------
//  Eine Zeile im User-Prop-Puffer setzen.
//
//  Vorhandene Zeile ersetzen, sonst anhaengen. Der uebrige
//  Puffer bleibt unangetastet - dort stehen die Rohdaten aus
//  Stufe 6, und die sind der Weg zurueck.
// ------------------------------------------------------------
void BafToolkitInterface::SetProp(INode* node, const std::wstring& key,
                                  const std::wstring& value) {
    if (node == nullptr) return;
    MSTR buffer;
    node->GetUserPropBuffer(buffer);
    std::wstring props = (buffer.length() > 0) ? buffer.data() : L"";

    const std::wstring line = key + L" = " + value;
    const std::wstring needle = key + L" = ";

    size_t at = 0;
    bool replaced = false;
    while (at < props.size()) {
        size_t end = props.find(L'\n', at);
        if (end == std::wstring::npos) end = props.size();
        if (props.compare(at, needle.size(), needle) == 0) {
            props = props.substr(0, at) + line + props.substr(end);
            replaced = true;
            break;
        }
        at = end + 1;
    }
    if (!replaced) {
        if (!props.empty() && props.back() != L'\n') props += L"\n";
        props += line + L"\n";
    }
    node->SetUserPropBuffer(props.c_str());
}

bool BafToolkitInterface::ReadProp(const std::wstring& buffer, const wchar_t* key,
                                  std::wstring& value) {
    value.clear();
    const std::wstring needle = std::wstring(key) + L" = ";
    // Zeilenweise suchen, nicht irgendwo im Puffer: sonst faende
    // "bafMeshIndex" auch in "bafSubMeshIndex".
    size_t at = 0;
    while (at < buffer.size()) {
        size_t end = buffer.find(L'\n', at);
        if (end == std::wstring::npos) end = buffer.size();
        const std::wstring line = buffer.substr(at, end - at);
        if (line.compare(0, needle.size(), needle) == 0) {
            value = line.substr(needle.size());
            while (!value.empty() && (value.back() == L'\r' || value.back() == L' ')) {
                value.pop_back();
            }
            return true;
        }
        at = end + 1;
    }
    return false;
}

namespace {

// Alle Knoten der Szene einsammeln, samt Kindern.
void CollectNodes(INode* node, std::vector<INode*>& out) {
    if (node == nullptr) return;
    if (!node->IsRootNode()) out.push_back(node);
    const int count = node->NumberOfChildren();
    for (int i = 0; i < count; ++i) CollectNodes(node->GetChildNode(i), out);
}

} // namespace

int BafToolkitInterface::ScanScene() {
    ClearMessages();
    Log(L"scanScene: start");

    sceneOrigin_.clear();
    sceneArchive_.clear();
    sceneClip_.clear();
    sceneReport_.clear();
    boneHandles_.clear();
    meshHandles_.clear();

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) { lastError_ = L"scanScene: no core interface"; return 0; }

    std::vector<INode*> nodes;
    CollectNodes(ip->GetRootNode(), nodes);

    // Erst sammeln, dann einsortieren: die hoechste Nummer
    // bestimmt die Groesse der Tabellen, und die kennt man erst
    // am Ende.
    std::vector<std::pair<size_t, ULONG>> bones, meshes;
    int withoutProps = 0;

    for (INode* node : nodes) {
        MSTR buffer;
        node->GetUserPropBuffer(buffer);
        if (buffer.length() == 0) { ++withoutProps; continue; }
        const std::wstring props = buffer.data();

        std::wstring value;
        // Ein Bone traegt bafNodeIndex, ein Mesh bafMeshName.
        // Beide haben ein bafMeshIndex - beim Bone ist es die
        // Nummer des Mesh, das ihm gehoert.
        if (ReadProp(props, L"bafNodeIndex", value) && !value.empty()) {
            bones.emplace_back(static_cast<size_t>(_wtoi(value.c_str())), node->GetHandle());
            if (sceneOrigin_.empty()) {
                std::wstring name;
                if (ReadProp(props, L"bafModelName", name)) sceneOrigin_ = name;
                std::wstring archive;
                if (ReadProp(props, L"bafArchive", archive)) sceneArchive_ = archive;
                std::wstring clip;
                if (ReadProp(props, L"bafSceneClip", clip)) sceneClip_ = clip;
            }
        } else if (ReadProp(props, L"bafMeshName", value)) {
            std::wstring index;
            if (ReadProp(props, L"bafMeshIndex", index) && !index.empty()) {
                meshes.emplace_back(static_cast<size_t>(_wtoi(index.c_str())),
                                    node->GetHandle());
            }
        } else {
            ++withoutProps;
        }
    }

    size_t maxBone = 0, maxMesh = 0;
    for (const auto& entry : bones)  maxBone = (entry.first + 1 > maxBone) ? entry.first + 1 : maxBone;
    for (const auto& entry : meshes) maxMesh = (entry.first + 1 > maxMesh) ? entry.first + 1 : maxMesh;
    boneHandles_.assign(maxBone, 0);
    meshHandles_.assign(maxMesh, 0);
    for (const auto& entry : bones)  boneHandles_[entry.first]  = entry.second;
    for (const auto& entry : meshes) meshHandles_[entry.first] = entry.second;

    const int found = static_cast<int>(bones.size() + meshes.size());
    sceneReport_ = (Msg() << bones.size() << L" bones, " << meshes.size()
                          << L" meshes with raw data found\n").str();
    if (!sceneOrigin_.empty()) {
        sceneReport_ += L"origin: " + sceneOrigin_;
        if (!sceneClip_.empty()) sceneReport_ += L", clip in the scene: " + sceneClip_;
        sceneReport_ += L"\n";
    } else if (found > 0) {
        sceneReport_ += L"origin unknown - the model name is missing from the raw data\n";
    } else {
        sceneReport_ += L"No BAF raw data in the scene. Exporting needs a "
                        L"template.\n";
    }
    if (withoutProps > 0) {
        sceneReport_ += (Msg() << withoutProps
                               << L" further objects without raw data (not "
                                  L"touched)\n").str();
    }

    Log(L"scanScene: " + sceneReport_);
    return found;
}

const MCHAR* BafToolkitInterface::GetSceneOrigin()  { return Give(sceneOrigin_); }
const MCHAR* BafToolkitInterface::GetSceneArchive() { return Give(sceneArchive_); }
const MCHAR* BafToolkitInterface::GetSceneClip()    { return Give(sceneClip_); }
const MCHAR* BafToolkitInterface::GetSceneReport()  {
    return Give(sceneReport_.empty() ? std::wstring(L"not scanned yet") : sceneReport_);
}

// ------------------------------------------------------------
//  Die Vorlage selbst nachladen.
//
//  Klappt nur, wenn die Szene ihre Herkunft kennt UND das Archiv
//  noch dort liegt, wo es beim Import lag. Sonst waehlt der
//  Anwender die Vorlage - und das ist derselbe Weg, den ein
//  eigenes Modell nimmt.
// ------------------------------------------------------------
int BafToolkitInterface::AutoLoadTemplate() {
    ClearMessages();
    if (sceneOrigin_.empty()) {
        lastError_ = L"autoLoadTemplate: the scene does not know its origin";
        return 0;
    }

    // Ist das richtige Modell schon geladen? Dann nichts tun -
    // der Anwender koennte inzwischen daran gearbeitet haben.
    if (model_ && Latin1ToWide(model_->name) == sceneOrigin_) {
        Log(L"autoLoadTemplate: '" + sceneOrigin_ + L"' is already loaded");
        return 1;
    }

    if (sceneArchive_.empty()) {
        lastError_ = L"autoLoadTemplate: the scene names no archive";
        return 0;
    }

    // Die Handles nicht verlieren: openArchive setzt sie nicht
    // zurueck, aber openModel leert slots_ - deshalb hinterher
    // noch einmal untersuchen.
    const std::wstring wantedOrigin = sceneOrigin_;
    const std::wstring wantedArchive = sceneArchive_;

    if (OpenArchive(wantedArchive.c_str()) < 0) {
        lastError_ = L"autoLoadTemplate: the archive '" + wantedArchive +
                     L"' cannot be opened. Please pick the template by hand.";
        Log(L"autoLoadTemplate: " + lastError_);
        return 0;
    }
    if (OpenModel(wantedOrigin.c_str()) < 0) {
        lastError_ = L"autoLoadTemplate: '" + wantedOrigin + L"' is not in '" +
                     wantedArchive + L"'";
        Log(L"autoLoadTemplate: " + lastError_);
        return 0;
    }

    // openModel hat die Szenenzuordnung nicht angefasst, aber
    // sicher ist sicher: die Tabellen stammen aus scanScene und
    // muessen zum frisch geladenen Modell passen.
    ScanScene();
    sceneOrigin_ = wantedOrigin;
    sceneArchive_ = wantedArchive;

    Log(L"autoLoadTemplate: '" + wantedOrigin + L"' aus '" + wantedArchive + L"' geladen");
    return 1;
}

// ------------------------------------------------------------
//  Traegt die Szene eine Animation?
//
//  Gezaehlt wird ueber die Bones, die scanScene oder
//  buildSkeleton zugeordnet hat. Ein einziger Key genuegt: dann
//  ist Frame 0 der erste Frame einer Bewegung und nicht die
//  Ruhelage.
// ------------------------------------------------------------
int BafToolkitInterface::SceneIsAnimated() {
    int animated = 0;
    for (ULONG handle : boneHandles_) {
        INode* node = NodeFromHandle(handle);
        if (node == nullptr) continue;
        Control* controller = node->GetTMController();
        if (controller != nullptr && CountKeys(controller) > 0) ++animated;
    }
    return animated;
}

int BafToolkitInterface::SetForceRestPose(int on) {
    forceRestPose_ = (on != 0);
    if (forceRestPose_) {
        Log(L"WARNING: the rest pose will be overwritten from frame 0, even if "
            L"the bones carry keys. Only right when a new rest pose is really "
            L"meant.");
    }
    return forceRestPose_ ? 1 : 0;
}

float BafToolkitInterface::SetKeyTolerance(float degrees) {
    keyTolerance_ = (degrees < 0.0f) ? 0.0f : degrees;
    Log(keyTolerance_ > 0.0f
        ? (Msg() << L"key tolerance: " << Latin1ToWide(baf::FormatFloat(keyTolerance_, 2))
                 << L" degrees").str()
        : std::wstring(L"key tolerance 0 - one key per frame"));
    return keyTolerance_;
}

int BafToolkitInterface::SetRebuild(int on) {
    rebuild_ = (on != 0);
    Log(rebuild_ ? L"topology rebuild ALLOWED"
                 : L"topology rebuild blocked (in place only)");
    return rebuild_ ? 1 : 0;
}

int BafToolkitInterface::SetRecomputeBounds(int on) {
    recomputeBounds_ = (on != 0);
    if (recomputeBounds_) {
        Log(L"WARNING: bounding spheres get recomputed. All 3,131 shipped "
            L"spheres are larger than the tightly fitting one, so recomputing "
            L"shrinks them - and on the head node that freezes the AI look-at direction.");
    }
    return recomputeBounds_ ? 1 : 0;
}

// ------------------------------------------------------------
//  Ein Mesh vollstaendig aus der Szene neu bauen.
//
//  Nichts aus den urspruenglichen Arrays bleibt stehen: nichts
//  darin gehoert noch zu dieser Topologie. Erhalten bleiben nur
//  die undurchsichtigen Wortfelder, geerbt vom ersten Face des
//  Mesh - naeher an dessen ueblichem Wert als null.
// ------------------------------------------------------------
bool BafToolkitInterface::RebuildMeshFromScene(size_t meshIndex, Mesh& maxMesh,
                                              const std::vector<baf::MaterialSlot>& slots,
                                              std::wstring& note) {
    baf::Mesh& target = model_->meshes[meshIndex];
    const double scale = (scale_ > 0.0f) ? static_cast<double>(scale_) : 1.0;
    const baf::Node* owner = model_->NodeOfMesh(static_cast<int32_t>(meshIndex));
    const baf::Vec3 pivot = (owner != nullptr) ? owner->pivot : baf::Vec3{};

    const int vertexCount = maxMesh.getNumVerts();
    const int faceCount   = maxMesh.getNumFaces();
    if (vertexCount <= 0 || faceCount <= 0) {
        note = L"empty mesh";
        return false;
    }

    // ---- Positionen ----
    std::vector<baf::Vec3> positions(static_cast<size_t>(vertexCount));
    for (int i = 0; i < vertexCount; ++i) {
        const Point3 point = maxMesh.getVert(i);
        positions[static_cast<size_t>(i)] = baf::Vec3{
            static_cast<float>(point.x / scale - pivot.x),
            static_cast<float>(point.y / scale - pivot.y),
            static_cast<float>(point.z / scale - pivot.z)};
    }

    // ---- Normalen ----
    //
    // Immer neu normalisieren. Ein Decimate oder eine Bearbeitung
    // hinterlaesst Vektoren, die nicht mehr die Laenge eins haben,
    // und ein Vertex, den kein Face beruehrt, hat gar keine. Die
    // Engine teilt durch die Laenge - das NaN daraus landet als
    // Zeigerindex im Speicher.
    std::vector<baf::Vec3> normals(static_cast<size_t>(vertexCount),
                                   baf::Vec3{ 0.0f, 0.0f, 1.0f });
    MeshNormalSpec* specified = maxMesh.GetSpecifiedNormals();
    if (specified != nullptr && specified->GetNumNormals() == vertexCount) {
        for (int i = 0; i < vertexCount; ++i) {
            const Point3 normal = specified->Normal(i);
            normals[static_cast<size_t>(i)] =
                baf::NormalizeOrUp(baf::Vec3{ normal.x, normal.y, normal.z });
        }
    } else {
        maxMesh.buildNormals();
        for (int i = 0; i < vertexCount; ++i) {
            const Point3 normal = maxMesh.getNormal(i);
            normals[static_cast<size_t>(i)] =
                baf::NormalizeOrUp(baf::Vec3{ normal.x, normal.y, normal.z });
        }
    }

    // ---- Intensitaeten ----
    // Aus Kanal 2, wo der Import sie abgelegt hat. Ohne den Kanal
    // null: das heisst "kein Alpha, nicht vollhell", und das ist
    // der harmlose Wert.
    std::vector<float> intensities(static_cast<size_t>(vertexCount), 0.0f);
    if (maxMesh.mapSupport(2) && maxMesh.getNumMapVerts(2) == vertexCount) {
        for (int i = 0; i < vertexCount; ++i) {
            intensities[static_cast<size_t>(i)] = maxMesh.mapVerts(2)[i].x;
        }
    }

    // ---- Faces ----
    // Ein Max-Dreieck wird ein Face mit drei Ecken. Die
    // Wortfelder erbt es vom ersten Face des Mesh.
    const bool hasTemplate = !target.faces.empty();
    const baf::Face templateFace = hasTemplate ? target.faces[0] : baf::Face();
    const bool hasUv = maxMesh.mapSupport(1) && maxMesh.getNumMapVerts(1) > 0;

    baf::TexVertPool pool;
    std::vector<baf::Face> faces;
    faces.reserve(static_cast<size_t>(faceCount));
    int withoutMaterial = 0;

    for (int f = 0; f < faceCount; ++f) {
        // Nicht const, siehe oben: getVert und getMatID sind im
        // SDK nicht const.
        Face& source = maxMesh.faces[f];

        // Die Material-ID zeigt auf einen PLATZ, nicht auf ein
        // Material der Datei - so hat buildMeshes sie gesetzt.
        const MtlID slotId = source.getMatID();
        uint32_t material = 0xffffu;
        if (slotId < slots.size()) {
            material = slots[slotId].material;
        } else {
            ++withoutMaterial;
        }

        baf::Face face;
        face.material     = material;
        face.flags        = templateFace.flags;
        face.geometryMode = templateFace.geometryMode;
        face.lightMode    = templateFace.lightMode;
        face.textureMode  = templateFace.textureMode;
        face.extraLight   = templateFace.extraLight;
        face.v0           = 0.0f;
        // Der Satz entsteht neu - es gibt keine Vorlage dafuer,
        // die zu diesem Face gehoeren wuerde.
        face.raw.assign(baf::kFaceBytes, 0);

        face.vertIdx = { static_cast<uint32_t>(source.getVert(0)),
                         static_cast<uint32_t>(source.getVert(1)),
                         static_cast<uint32_t>(source.getVert(2)) };

        // Die Flaechennormale aus den drei Ecken. Der Raum ist der
        // des Mesh, es wird nur verschoben und nie gedreht - die
        // Normale braucht also keinen Basiswechsel.
        const baf::Vec3& a = positions[face.vertIdx[0]];
        const baf::Vec3& b = positions[face.vertIdx[1]];
        const baf::Vec3& c = positions[face.vertIdx[2]];
        const double ux = b.x - a.x, uy = b.y - a.y, uz = b.z - a.z;
        const double vx = c.x - a.x, vy = c.y - a.y, vz = c.z - a.z;
        face.normal = baf::NormalizeOrUp(baf::Vec3{
            static_cast<float>(uy * vz - uz * vy),
            static_cast<float>(uz * vx - ux * vz),
            static_cast<float>(ux * vy - uy * vx)});

        // Texturindizes nur, wenn das Material sie verlangt -
        // dieselbe Bedingung wie beim Lesen und beim Schreiben.
        const bool textured = (material != 0xffffu) &&
            (material < model_->materials.size()) &&
            (model_->materials[material].matType & baf::kMatTextured);
        if (textured && hasUv) {
            uint32_t celWidth = 256, celHeight = 256;
            model_->MaterialCelSize(material, celWidth, celHeight);
            for (int corner = 0; corner < 3; ++corner) {
                float u = 0.0f, v = 0.0f;
                const int mapIndex = maxMesh.mapFaces(1)[f].getTVert(corner);
                if (mapIndex >= 0 && mapIndex < maxMesh.getNumMapVerts(1)) {
                    const UVVert uv = maxMesh.mapVerts(1)[mapIndex];
                    u =  uv.x * static_cast<float>(celWidth);
                    v = -uv.y * static_cast<float>(celHeight);
                }
                face.texIdx.push_back(pool.IndexOf(u, v));
            }
        }
        faces.push_back(std::move(face));
    }

    // ---- Grenzen der Engine ----
    // Gewarnt, nicht abgelehnt: geschrieben wird trotzdem, aber
    // der Loader des Spiels nimmt die Datei so nicht an.
    if (static_cast<uint32_t>(vertexCount) > baf::kMaxMeshVerts) {
        note += (Msg() << L"  " << vertexCount << L" vertices, the engine takes "
                       << baf::kMaxMeshVerts << L"\n").str();
    }
    if (static_cast<uint32_t>(faceCount) > baf::kMaxMeshFaces) {
        note += (Msg() << L"  " << faceCount << L" faces, the engine takes "
                       << baf::kMaxMeshFaces << L"\n").str();
    }
    if (pool.TexVerts().size() > baf::kMaxMeshTexVerts) {
        note += (Msg() << L"  " << pool.TexVerts().size()
                       << L" texture vertices, the engine takes "
                       << baf::kMaxMeshTexVerts << L"\n").str();
    }
    if (withoutMaterial > 0) {
        note += (Msg() << L"  " << withoutMaterial
                       << L" faces with no valid material, written untextured\n").str();
    }

    // ---- Huellkugel ----
    const int32_t head = baf::HeadNodeIndex(*model_);
    const bool isHead = (owner != nullptr) && (head >= 0) && (owner->num == head);
    if (recomputeBounds_ && !isHead) {
        baf::ComputeBounds(positions, target.radius, target.center);
    } else if (recomputeBounds_ && isHead) {
        note += L"  head node: bounding sphere KEPT despite the recompute switch. "
                L"A shrunken sphere on the head freezes the AI look-at "
                L"direction.\n";
    }

    // ---- Einsetzen ----
    target.positions   = std::move(positions);
    target.normals     = std::move(normals);
    target.intensities = std::move(intensities);
    target.faces       = std::move(faces);
    target.texVerts    = pool.TexVerts();
    // Das tote Feld waechst mit der Vertexzahl mit.
    target.dead.assign(target.positions.size() * 4, 0);

    baf::ApplyLoadPasses(target, model_->materials);
    return true;
}

int BafToolkitInterface::ApplyScene() {
    ClearMessages();
    exportReport_.clear();
    Log(L"applyScene: start");
    if (!model_) { lastError_ = L"applyScene: no model open"; return -1; }

    const auto started = std::chrono::steady_clock::now();
    const double scale = (scale_ > 0.0f) ? static_cast<double>(scale_) : 1.0;

    int bonesTaken = 0, meshesTaken = 0, skipped = 0, rebuilt = 0;
    std::wstring notes;

    // ------------------------------------------------------
    //  Die Ruhelage nicht aus einer Pose lesen.
    //
    //  applyScene nimmt die Bones bei FRAME 0. Das ist die
    //  Ruhelage - solange dort auch die Ruhelage steht. Wer eine
    //  Animation von Frame 0 bis 26 gebaut hat, hat dort den
    //  ERSTEN FRAME DER BEWEGUNG.
    //
    //  Passiert ist genau das: 17 von 48 Knoten bekamen eine
    //  neue Ruhelage, der linke Oberarm um 70 Grad verdreht.
    //  Danach lief im Spiel keine einzige Animation mehr - die
    //  Ruhelage ist das Fundament fuer jeden Knoten, den ein Clip
    //  nicht anfasst.
    //
    //  Deshalb wird hier gezaehlt, nicht gehofft. Traegt auch nur
    //  ein Bone Keys, bleibt die Ruhelage stehen.
    // ------------------------------------------------------
    const int animatedBones = SceneIsAnimated();
    const bool keepRest = (animatedBones > 0) && !forceRestPose_;
    if (keepRest) {
        notes += (Msg() << animatedBones
                        << L" bones carry keys, so frame 0 is not a rest pose - "
                           L"the rest pose is LEFT UNTOUCHED.\n"
                           L"That is almost always what you want. Use "
                           L"setForceRestPose 1 if a new rest pose is really "
                           L"meant.\n").str();
        Log(Msg() << L"applyScene: rest pose kept, " << animatedBones
                  << L" bones carry keys");
    }

    // ---- Bones ----
    for (size_t i = 0; !keepRest && i < boneHandles_.size() && i < model_->nodes.size(); ++i) {
        INode* node = NodeFromHandle(boneHandles_[i]);
        if (node == nullptr) continue;

        // Die LOKALE Lage, also relativ zum Elternteil - das ist,
        // was bindPos und bindEuler in der Datei bedeuten.
        Matrix3 tm = node->GetNodeTM(0);
        INode* parent = node->GetParentNode();
        if (parent != nullptr && !parent->IsRootNode()) {
            tm = tm * Inverse(parent->GetNodeTM(0));
        }
        double right[3], forward[3], up[3];
        for (int axis = 0; axis < 3; ++axis) {
            right[axis]   = tm.GetRow(0)[axis];
            forward[axis] = tm.GetRow(1)[axis];
            up[axis]      = tm.GetRow(2)[axis];
        }
        // Die Drehung, die der Import aufgesetzt hat, hier wieder
        // heraus - sonst stuende sie in der Datei. Nur beim
        // Wurzelknoten, genau wie beim Aufsetzen.
        baf::Matrix34 localTm{};
        localTm.m[0][0] = right[0];   localTm.m[0][1] = right[1];   localTm.m[0][2] = right[2];
        localTm.m[1][0] = forward[0]; localTm.m[1][1] = forward[1]; localTm.m[1][2] = forward[2];
        localTm.m[2][0] = up[0];      localTm.m[2][1] = up[1];      localTm.m[2][2] = up[2];
        const Point3 rawTranslation = tm.GetRow(3);
        localTm.m[3][0] = rawTranslation.x;
        localTm.m[3][1] = rawTranslation.y;
        localTm.m[3][2] = rawTranslation.z;

        if (faceFront_ && model_->nodes[i].parent < 0) {
            localTm = baf::Concat(FrontRotationInverse(), localTm);
            for (int axis = 0; axis < 3; ++axis) {
                right[axis]   = localTm.m[0][axis];
                forward[axis] = localTm.m[1][axis];
                up[axis]      = localTm.m[2][axis];
            }
        }

        double pitch = 0.0, yaw = 0.0, roll = 0.0;
        baf::BasisToEuler(right, forward, up, pitch, yaw, roll);

        baf::Node& target = model_->nodes[i];
        target.bindEuler = baf::Vec3{ static_cast<float>(pitch), static_cast<float>(yaw),
                                      static_cast<float>(roll) };
        target.bindPos = baf::Vec3{ static_cast<float>(localTm.m[3][0] / scale),
                                    static_cast<float>(localTm.m[3][1] / scale),
                                    static_cast<float>(localTm.m[3][2] / scale) };
        ++bonesTaken;
    }

    // ---- Meshes ----
    std::vector<baf::MaterialSlot> slots;
    baf::BuildMaterialSlots(*model_, slots);

    for (size_t meshIndex = 0; meshIndex < meshHandles_.size() &&
                               meshIndex < model_->meshes.size(); ++meshIndex) {
        INode* node = NodeFromHandle(meshHandles_[meshIndex]);
        if (node == nullptr) continue;

        baf::Mesh& target = model_->meshes[meshIndex];
        baf::BuiltMesh built;
        baf::BuildMeshGeometry(*model_, meshIndex, scale, slots, built);

        ObjectState state = node->EvalWorldState(0);
        if (state.obj == nullptr || !state.obj->CanConvertToType(triObjectClassID)) {
            ++skipped;
            notes += (Msg() << L"'" << Latin1ToWide(target.name)
                            << L"' is no longer a mesh\n").str();
            continue;
        }
        TriObject* triObject =
            static_cast<TriObject*>(state.obj->ConvertToType(0, triObjectClassID));
        if (triObject == nullptr) { ++skipped; continue; }
        Mesh& maxMesh = triObject->GetMesh();

        // Die Topologiepruefung. Sie ist der Grund, warum dieser
        // Export sicher ist: passt die Zahl nicht, wird gar nichts
        // uebernommen statt die Haelfte.
        if (static_cast<size_t>(maxMesh.getNumVerts()) != target.positions.size() ||
            static_cast<size_t>(maxMesh.getNumFaces()) != built.tris.size()) {
            if (!rebuild_) {
                ++skipped;
                notes += (Msg() << L"'" << Latin1ToWide(target.name)
                                << L"': topology changed ("
                                << maxMesh.getNumVerts() << L"/" << maxMesh.getNumFaces()
                                << L" statt " << target.positions.size() << L"/"
                                << built.tris.size()
                                << L") - skipped. Mit setRebuild 1 wird es "
                                   L"rebuilt instead.").str();
                if (triObject != state.obj) triObject->DeleteMe();
                continue;
            }
            std::wstring note;
            const bool ok = RebuildMeshFromScene(meshIndex, maxMesh, slots, note);
            if (triObject != state.obj) triObject->DeleteMe();
            if (!ok) {
                ++skipped;
                notes += (Msg() << L"'" << Latin1ToWide(target.name) << L"': " << note
                                << L"\n").str();
                continue;
            }
            ++rebuilt;
            Log(Msg() << L"rebuilt: '" << Latin1ToWide(target.name) << L"', "
                      << target.positions.size() << L" Vertices, "
                      << target.faces.size() << L" Faces, "
                      << target.texVerts.size() << L" Texturvertices");
            if (!note.empty()) {
                notes += (Msg() << L"'" << Latin1ToWide(target.name) << L"' rebuilt:\n"
                                << note).str();
            }
            continue;
        }

        // Pivot herausrechnen: v_roh = v_lokal / scale - pivot.
        const baf::Node* owner = model_->NodeOfMesh(static_cast<int32_t>(meshIndex));
        const baf::Vec3 pivot = (owner != nullptr) ? owner->pivot : baf::Vec3{};
        for (int i = 0; i < maxMesh.getNumVerts(); ++i) {
            const Point3 point = maxMesh.getVert(i);
            target.positions[static_cast<size_t>(i)] = baf::Vec3{
                static_cast<float>(point.x / scale - pivot.x),
                static_cast<float>(point.y / scale - pivot.y),
                static_cast<float>(point.z / scale - pivot.z)};
        }

        // Normalen, wenn Max welche vorgegeben hat. Ohne
        // vorgegebene Normalen bleiben die der Datei stehen - die
        // von Max berechneten waeren eine Verschlechterung, keine
        // Aktualisierung.
        MeshNormalSpec* normals = maxMesh.GetSpecifiedNormals();
        if (normals != nullptr && normals->GetNumNormals() == maxMesh.getNumVerts()) {
            for (int i = 0; i < maxMesh.getNumVerts(); ++i) {
                const Point3 normal = normals->Normal(i);
                target.normals[static_cast<size_t>(i)] =
                    baf::Vec3{ normal.x, normal.y, normal.z };
            }
        }

        // UVs zurueck in Texel. Geteilt wurde durch die Cel-Groesse
        // des Materials DIESES Face - also muss hier mit derselben
        // Zahl multipliziert werden, sonst rutscht die Textur.
        if (maxMesh.mapSupport(1) && maxMesh.getNumMapVerts(1) > 0) {
            for (size_t t = 0; t < built.tris.size(); ++t) {
                const baf::BuiltTri& tri = built.tris[t];
                if (tri.sourceFace >= target.faces.size()) continue;
                const baf::Face& face = target.faces[tri.sourceFace];
                if (face.texIdx.empty()) continue;

                uint32_t celWidth = 256, celHeight = 256;
                model_->MaterialCelSize(face.material, celWidth, celHeight);

                for (int corner = 0; corner < 3; ++corner) {
                    const int mapIndex =
                        maxMesh.mapFaces(1)[static_cast<int>(t)].getTVert(corner);
                    if (mapIndex < 0 || mapIndex >= maxMesh.getNumMapVerts(1)) continue;
                    const UVVert uv = maxMesh.mapVerts(1)[mapIndex];

                    // Welche Ecke des Quellfaces ist das? Die
                    // Faechertriangulierung ging von Ecke 0 aus,
                    // also gehoert Dreiecksecke k zu einer der
                    // Ecken, die tri.vertex nennt.
                    for (size_t k = 0; k < face.vertIdx.size(); ++k) {
                        if (face.vertIdx[k] != tri.vertex[corner]) continue;
                        if (k >= face.texIdx.size()) break;
                        const uint32_t texel = face.texIdx[k];
                        if (texel >= target.texVerts.size()) break;
                        target.texVerts[texel][0] = uv.x * static_cast<float>(celWidth);
                        target.texVerts[texel][1] = -uv.y * static_cast<float>(celHeight);
                        break;
                    }
                }
            }
        }

        if (triObject != state.obj) triObject->DeleteMe();
        ++meshesTaken;
    }

    // Die Ladedurchgaenge neu fahren: die Intensitaeten koennen
    // sich geaendert haben, und flagsLoaded haengt daran.
    for (baf::Mesh& mesh : model_->meshes) {
        baf::ApplyLoadPasses(mesh, model_->materials);
    }

    timings_ = L"applyScene: " + MillisSince(started);
    exportReport_ = (Msg() << bonesTaken << L" bones, " << meshesTaken
                           << L" meshes taken over, " << rebuilt << L" rebuilt, "
                           << skipped << L" skipped\n" << notes).str();
    if (!notes.empty()) warnings_ += notes;
    Log(L"applyScene: " + exportReport_ + timings_);
    return bonesTaken + meshesTaken + rebuilt;
}

void BafToolkitInterface::SetPendingExportPath(const std::wstring& path) {
    pendingExportPath_ = path;
}

const MCHAR* BafToolkitInterface::TakePendingExportPath() {
    returnBuffer_ = pendingExportPath_;
    pendingExportPath_.clear();
    return returnBuffer_.c_str();
}

// ------------------------------------------------------------
//  Einen neuen, leeren Clip anlegen.
//
//  Tracks fuer ALLE Knoten, aber ohne Keys - die kommen mit
//  captureAnim aus der Szene. Ein Track je Knoten ist die
//  Form, die auch die Retail-Clips haben.
//
//  Die undurchsichtigen Teile werden von einem vorhandenen Clip
//  GEKLONT statt erfunden. Nachgemessen an allen 2.281 Clips des
//  Bestands gibt es genau drei solche Felder:
//
//    Descriptor + 0x04   in 2.276 von 2.281 Clips null,
//                        sonst eine Zahl in Zeigergroesse
//    Key-Header + 0x3c   Adressen zwischen 12 und 116 Millionen
//    Track      + 0x28   dasselbe, ein Feld weiter
//
//  Das sind alte Zeiger aus dem Prozess von 1999. Die Engine
//  schreibt sie beim Laden neu - anders koennte sie die Datei
//  gar nicht benutzen. Sie trotzdem zu klonen statt zu nullen
//  ist die vorsichtigere Wahl: eine Null waere eine Behauptung
//  darueber, was die Engine dort erwartet.
//
//  Die uebrigen Werte des neuen Clips kommen ebenfalls von der
//  Vorlage - fade, loop, trackFlags, specialUsage. Nur die
//  Typmaske wird auf 0xFFFF gesetzt, "alle Koerperteile": das
//  ist mit 1.613 von 2.281 Clips der haeufigste Wert und die
//  einzige Wahl, die keinen Knoten stillschweigend auslaesst.
// ------------------------------------------------------------
int BafToolkitInterface::AddClip(const MCHAR* name, float fps, int frames) {
    ClearMessages();
    if (!model_) { lastError_ = L"addClip: no model open"; return -1; }
    if (name == nullptr || name[0] == _T('\0')) {
        lastError_ = L"addClip: no name";
        return -1;
    }
    if (fps <= 0.0f) { lastError_ = L"addClip: fps must be greater than zero"; return -1; }
    if (frames < 1) { lastError_ = L"addClip: at least one frame"; return -1; }
    if (model_->nodes.empty()) { lastError_ = L"addClip: the model has no nodes"; return -1; }

    baf::Clip clip;
    clip.name      = WideToLatin1(name);
    clip.fps       = fps;
    clip.numFrames = frames;
    // Alle Koerperteile. Sonst laesst der Clip stillschweigend
    // Knoten aus, und man sucht spaeter, warum ein Arm steht.
    clip.typeMask  = 0xFFFFu;

    if (!model_->clips.empty()) {
        const baf::Clip& pattern = model_->clips[0];
        clip.rawDesc      = pattern.rawDesc;
        clip.rawKeyHdr    = pattern.rawKeyHdr;
        clip.fadeSeconds  = pattern.fadeSeconds;
        clip.var1Loop     = pattern.var1Loop;
        clip.var2Frame    = pattern.var2Frame;
        clip.trackFlags   = pattern.trackFlags;
        clip.specialUsage = pattern.specialUsage;
    } else {
        clip.rawDesc.assign(baf::kDescBytes, 0);
        clip.rawKeyHdr.assign(baf::kKeyHdrBytes, 0);
    }

    // Ein Track je Knoten, in Knotenreihenfolge. Die Bindung
    // laeuft ueber den INDEX - der Name steht nur zur
    // Verstaendlichkeit dabei.
    const std::vector<uint8_t>* trackPattern = nullptr;
    if (!model_->clips.empty() && !model_->clips[0].tracks.empty()) {
        trackPattern = &model_->clips[0].tracks[0].raw;
    }
    clip.tracks.reserve(model_->nodes.size());
    for (size_t i = 0; i < model_->nodes.size(); ++i) {
        baf::Track track;
        track.name = model_->nodes[i].name;
        track.jointIndex = static_cast<int32_t>(i);
        if (trackPattern != nullptr && trackPattern->size() == baf::kKeyNodeBytes) {
            track.raw = *trackPattern;
        } else {
            track.raw.assign(baf::kKeyNodeBytes, 0);
        }
        clip.tracks.push_back(std::move(track));
    }

    model_->clips.push_back(std::move(clip));
    // Die Tabellen fuer Start und Ende wachsen mit, sonst greift
    // clipStart spaeter daneben.
    clipStart_.resize(model_->clips.size(), -1);
    clipEnd_.resize(model_->clips.size(), -1);

    const int index = static_cast<int>(model_->clips.size()) - 1;
    exportReport_ = (Msg() << L"Clip " << index << L" '" << name << L"' angelegt: "
                           << Latin1ToWide(baf::FormatFloat(fps, 1)) << L" fps, "
                           << frames << L" Frames, " << model_->nodes.size()
                           << L" tracks without keys").str();
    Log(L"addClip: " + exportReport_);
    return index;
}

int BafToolkitInterface::DeleteClip(int clipIndex) {
    ClearMessages();
    if (!model_) { lastError_ = L"deleteClip: no model open"; return 0; }
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= model_->clips.size()) {
        lastError_ = L"deleteClip: clip number out of range";
        return 0;
    }
    const std::wstring name = Latin1ToWide(model_->clips[static_cast<size_t>(clipIndex)].name);
    model_->clips.erase(model_->clips.begin() + clipIndex);
    // Alle Nummern dahinter verschieben sich. Die Tabellen sind
    // danach nicht mehr zu gebrauchen, also leeren statt
    // umsortieren - ein falscher Startframe waere schlimmer als
    // gar keiner.
    clipStart_.assign(model_->clips.size(), -1);
    clipEnd_.assign(model_->clips.size(), -1);
    Log(L"deleteClip: '" + name + L"' entfernt");
    return 1;
}

int BafToolkitInterface::SetClipInfo(int clipIndex, const MCHAR* name, float fps,
                                     int frames) {
    ClearMessages();
    if (!model_) { lastError_ = L"setClipInfo: no model open"; return 0; }
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= model_->clips.size()) {
        lastError_ = L"setClipInfo: clip number out of range";
        return 0;
    }
    baf::Clip& clip = model_->clips[static_cast<size_t>(clipIndex)];
    if (name != nullptr && name[0] != _T('\0')) clip.name = WideToLatin1(name);
    if (fps > 0.0f) clip.fps = fps;
    if (frames >= 1) clip.numFrames = frames;
    Log(Msg() << L"setClipInfo " << clipIndex << L": '" << Latin1ToWide(clip.name)
              << L"', " << Latin1ToWide(baf::FormatFloat(clip.fps, 1)) << L" fps, "
              << clip.numFrames << L" Frames");
    return 1;
}

// ------------------------------------------------------------
//  Die Bewegung aus der Szene zurueck in einen Clip.
//
//  Das Gegenstueck zu buildAnim. applyScene fasst die Animation
//  bewusst nicht an - es nimmt die RUHELAGE der Bones, und die
//  liest es bei Frame 0. Waer das anders, wuerde jede
//  Ruhelage-Aenderung von der gerade eingestellten Pose
//  ueberschrieben.
//
//  Hier wird dagegen ueber die ganze Laenge des Clips abgetastet,
//  ein Key je Frame. Das ist die Umkehrung des Backens beim
//  Import und trifft die Bewegung genau, egal womit sie in Max
//  erzeugt wurde - von Hand, mit IK, mit einem Constraint.
// ------------------------------------------------------------
int BafToolkitInterface::CaptureAnim(int clipIndex) {
    ClearMessages();
    if (!model_) { lastError_ = L"captureAnim: no model open"; return -1; }
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= model_->clips.size()) {
        lastError_ = L"captureAnim: clip number out of range";
        return -1;
    }
    if (boneHandles_.empty()) {
        lastError_ = L"captureAnim: no skeleton in the scene - run scanScene first";
        return -1;
    }

    baf::Clip& clip = model_->clips[static_cast<size_t>(clipIndex)];
    const int ticks = GetTicksPerFrame();
    const double scale = (scale_ > 0.0f) ? static_cast<double>(scale_) : 1.0;
    const int frameCount = (clip.numFrames > 0) ? clip.numFrames : 1;

    // Wo liegt der Clip in der Szene? Im Sequenzmodus hinter den
    // vorigen, sonst bei null.
    const int startFrame =
        (clipIndex < static_cast<int>(clipStart_.size()) && clipStart_[clipIndex] >= 0)
            ? clipStart_[clipIndex] : 0;

    Log(Msg() << L"captureAnim " << clipIndex << L" '" << Latin1ToWide(clip.name)
              << L"': " << frameCount << L" frames from scene frame " << startFrame);

    const baf::Matrix34 turnBack = FrontRotationInverse();
    int keysWritten = 0, missing = 0;

    for (baf::Track& track : clip.tracks) {
        size_t joint = 0;
        if (track.jointIndex >= 0 &&
            static_cast<size_t>(track.jointIndex) < model_->nodes.size()) {
            joint = static_cast<size_t>(track.jointIndex);
        }
        INode* node = (joint < boneHandles_.size())
            ? NodeFromHandle(boneHandles_[joint]) : nullptr;
        if (node == nullptr) { ++missing; continue; }

        const bool turnThis = faceFront_ && (model_->nodes[joint].parent < 0);

        std::vector<baf::Key> keys;
        keys.reserve(static_cast<size_t>(frameCount));
        double previous[3] = { 0.0, 0.0, 0.0 };

        for (int frame = 0; frame < frameCount; ++frame) {
            const double sceneFrame =
                static_cast<double>(startFrame) + ClipFrameToScene(frame, clip.fps);
            const TimeValue time = static_cast<TimeValue>(sceneFrame + 0.5) * ticks;

            Matrix3 tm = node->GetNodeTM(time);
            INode* parent = node->GetParentNode();
            if (parent != nullptr && !parent->IsRootNode()) {
                tm = tm * Inverse(parent->GetNodeTM(time));
            }

            baf::Matrix34 local{};
            for (int row = 0; row < 4; ++row) {
                const Point3 point = tm.GetRow(row);
                local.m[row][0] = point.x;
                local.m[row][1] = point.y;
                local.m[row][2] = point.z;
            }
            if (turnThis) local = baf::Concat(turnBack, local);

            double pitch = 0.0, yaw = 0.0, roll = 0.0;
            baf::BasisToEuler(local.m[0], local.m[1], local.m[2], pitch, yaw, roll);

            // ------------------------------------------------
            //  Die Winkel NICHT entwrappen.
            //
            //  Bis 1.7.1 wurde jeder Winkel um Vielfache von 360
            //  auf den vorigen zugezogen, damit eine Drehung ueber
            //  die +-180-Grenze nicht zurueckschnappt. Das war
            //  gut gemeint und passt nicht zum Format.
            //
            //  Nachgemessen am Bestand: bei einer Differenz von
            //  359,39 Grad steht als Steigung -0,61 - das Format
            //  nimmt also den KUERZEREN WEG und laesst die Winkel
            //  im Bereich aus atan2. Die Entwrappung erzeugte
            //  stattdessen Betraege bis 361 Grad, die dort sonst
            //  nirgends vorkommen.
            //
            //  Das Zurueckschnappen verhindert nicht die
            //  Entwrappung, sondern die gewrappte Steigung weiter
            //  unten: sie zeigt immer den kurzen Weg.
            // ------------------------------------------------
            const double angles[3] = { pitch, yaw, roll };
            for (int axis = 0; axis < 3; ++axis) previous[axis] = angles[axis];

            baf::Key key;
            key.frame = static_cast<float>(frame);
            // Beide Interpolationsbits: zwischen zwei Keys wird
            // gerechnet, nicht gehalten.
            key.flags = baf::kKeyLerpPos | baf::kKeyLerpOrient;
            key.pos = baf::Vec3{ static_cast<float>(local.m[3][0] / scale),
                                 static_cast<float>(local.m[3][1] / scale),
                                 static_cast<float>(local.m[3][2] / scale) };
            key.orient = baf::Vec3{ static_cast<float>(angles[0]),
                                    static_cast<float>(angles[1]),
                                    static_cast<float>(angles[2]) };
            // Ein Key-Satz ist 0x38 Byte und wird vollstaendig
            // beschrieben - hier braucht es keine Vorlage.
            key.raw.assign(baf::kKeyBytes, 0);
            keys.push_back(key);
        }

        // ----------------------------------------------------
        //  Ausduennen.
        //
        //  Ein Key je Frame ist nicht das, was das Format tut.
        //  Nachgezaehlt: im Bestand kommen im Mittel 0,25 Keys
        //  auf einen Frame, im Median 0,06 - der Normalfall ist
        //  ein Key alle paar Frames, oft nur einer fuer den
        //  ganzen Track.
        //
        //  Ein einziger dicht abgetasteter Clip liess obiwan.baf
        //  um 123 KB wachsen und die Gesamtzahl der Keys von
        //  18.254 auf 20.453 steigen - mehr, als irgendein
        //  Modell des Spiels mitbringt.
        //
        //  Behalten wird ein Frame nur, wenn die gerade Linie
        //  zwischen den benachbarten Keys ihn um mehr als die
        //  Toleranz verfehlt. Das ist verlustbehaftet, aber
        //  kontrolliert: die Abweichung bleibt unter dem, was
        //  eingestellt ist.
        // ----------------------------------------------------
        if (keyTolerance_ > 0.0f && keys.size() > 2) {
            const double angleTol = keyTolerance_;
            // Die Laengentoleranz haengt am Winkel: ein halbes
            // Grad an einem Arm von 0,3 Einheiten sind rund
            // 0,003. Ein eigener Regler dafuer waere ein Regler
            // zu viel.
            const double posTol = keyTolerance_ * 0.005;

            std::vector<baf::Key> thin;
            thin.push_back(keys.front());
            size_t anchor = 0;
            for (size_t i = 1; i + 1 < keys.size(); ++i) {
                // Passt der Frame dazwischen noch auf die Gerade
                // von anchor nach i+1?
                bool fits = true;
                for (size_t m = anchor + 1; m <= i && fits; ++m) {
                    const double t = static_cast<double>(m - anchor) /
                                     static_cast<double>(i + 1 - anchor);
                    for (int axis = 0; axis < 3 && fits; ++axis) {
                        const float* a = &keys[anchor].pos.x;
                        const float* b = &keys[i + 1].pos.x;
                        const float* c = &keys[m].pos.x;
                        if (std::fabs(a[axis] + (b[axis] - a[axis]) * t - c[axis]) > posTol) {
                            fits = false;
                        }
                        const float* ao = &keys[anchor].orient.x;
                        const float* bo = &keys[i + 1].orient.x;
                        const float* co = &keys[m].orient.x;
                        // Ueber die +-180-Grenze hinweg rechnen,
                        // sonst sieht ein Sprung wie eine grosse
                        // Abweichung aus.
                        double span = bo[axis] - ao[axis];
                        while (span >  180.0) span -= 360.0;
                        while (span < -180.0) span += 360.0;
                        double have = co[axis] - ao[axis];
                        while (have >  180.0) have -= 360.0;
                        while (have < -180.0) have += 360.0;
                        if (std::fabs(ao[axis] + span * t - (ao[axis] + have)) > angleTol) {
                            fits = false;
                        }
                    }
                }
                if (!fits) {
                    thin.push_back(keys[i]);
                    anchor = i;
                }
            }
            thin.push_back(keys.back());
            keys = std::move(thin);
        }

        // ----------------------------------------------------
        //  Die Steigungen.
        //
        //  Die Engine rechnet pos + dpos * t mit t in Frames DES
        //  CLIPS. Bei Keys, die mehrere Frames auseinanderliegen,
        //  ist die Steigung also die Differenz GETEILT DURCH DEN
        //  ABSTAND - nachgemessen an 21.607 Keypaaren des
        //  Bestands mit Abstand ueber 1, und in keinem einzigen
        //  Fall die blosse Differenz.
        //
        //  Bis 1.7.1 stand hier die Differenz. Das ging gut,
        //  solange jeder Frame einen Key hatte, und wird mit dem
        //  Ausduennen oben sofort falsch.
        //
        //  Der Winkelabstand nimmt den KUERZEREN WEG, genau wie
        //  im Bestand: eine Differenz von 359,39 Grad wird zu
        //  -0,61.
        // ----------------------------------------------------
        for (size_t i = 0; i + 1 < keys.size(); ++i) {
            const double gap = static_cast<double>(keys[i + 1].frame - keys[i].frame);
            if (gap <= 0.0) continue;
            keys[i].dpos = baf::Vec3{
                static_cast<float>((keys[i + 1].pos.x - keys[i].pos.x) / gap),
                static_cast<float>((keys[i + 1].pos.y - keys[i].pos.y) / gap),
                static_cast<float>((keys[i + 1].pos.z - keys[i].pos.z) / gap)};
            float slope[3];
            const float* a = &keys[i].orient.x;
            const float* b = &keys[i + 1].orient.x;
            for (int axis = 0; axis < 3; ++axis) {
                double d = b[axis] - a[axis];
                while (d >  180.0) d -= 360.0;
                while (d < -180.0) d += 360.0;
                slope[axis] = static_cast<float>(d / gap);
            }
            keys[i].dorient = baf::Vec3{ slope[0], slope[1], slope[2] };
        }
        // Der letzte Key hat keinen naechsten: Steigung null,
        // sonst liefe die Bewegung ueber das Ende hinaus weiter.
        // Und Flags null, wie bei 46.824 der Retail-Tracks - es
        // gibt nichts mehr, wohin interpoliert werden koennte.
        if (!keys.empty()) {
            keys.back().dpos = baf::Vec3{};
            keys.back().dorient = baf::Vec3{};
            keys.back().flags = 0;
        }

        keysWritten += static_cast<int>(keys.size());
        track.keys = std::move(keys);
    }

    if (missing > 0) {
        warnings_ += (Msg() << missing
                            << L" tracks with no bone in the scene - left untouched\n").str();
    }
    exportReport_ = (Msg() << L"Clip '" << Latin1ToWide(clip.name) << L"': "
                           << keysWritten << L" keys taken from the scene").str();
    Log(L"captureAnim: " + exportReport_);
    return keysWritten;
}

int BafToolkitInterface::ExportFile(const MCHAR* path) {
    ClearMessages();
    if (!model_) { lastError_ = L"exportFile: no model open"; return 0; }
    if (!path) { lastError_ = L"exportFile: no path"; return 0; }

    const auto started = std::chrono::steady_clock::now();
    std::vector<uint8_t> bytes;
    std::string error;
    if (!baf::BuildBaf(*model_, bytes, error)) {
        lastError_ = Latin1ToWide(error);
        Log(L"exportFile FEHLER: " + lastError_);
        return 0;
    }

    const std::string utf8 = WideToUtf8(path);
    std::FILE* handle = baf::OpenBinaryWrite(utf8);
    if (handle == nullptr) {
        lastError_ = Msg() << L"exportFile: '" << path << L"' cannot be written";
        return 0;
    }
    const size_t written = std::fwrite(bytes.data(), 1, bytes.size(), handle);
    std::fclose(handle);
    if (written != bytes.size()) {
        lastError_ = L"exportFile: written incompletely - disk full?";
        Log(L"exportFile FEHLER: " + lastError_);
        return 0;
    }

    timings_ = L"exportFile: " + MillisSince(started);
    const size_t difference = baf::FirstDifference(sourceBytes_, bytes);
    Log(Msg() << L"exportFile: " << bytes.size() << L" bytes to '" << path << L"', "
              << ((difference == SIZE_MAX) ? std::wstring(L"byte for byte identical to the original")
                                           : (L"first difference at " + HexText(difference)))
              << L", " << timings_);
    return 1;
}

int BafToolkitInterface::ExportToArchive() {
    ClearMessages();
    if (!model_) { lastError_ = L"exportToArchive: no model open"; return 0; }
    if (!archive_.IsOpen()) {
        lastError_ = L"exportToArchive: no archive open";
        return 0;
    }
    if (sourceEntry_.empty()) {
        lastError_ = L"exportToArchive: this model did not come from the archive - "
                     L"please use exportFile";
        return 0;
    }

    Log(Msg() << L"exportToArchive: '" << Latin1ToWide(sourceEntry_) << L"' in '"
              << Latin1ToWide(archive_.Path()) << L"'");
    const auto started = std::chrono::steady_clock::now();

    std::vector<uint8_t> bytes;
    std::string error;
    if (!baf::BuildBaf(*model_, bytes, error)) {
        lastError_ = Latin1ToWide(error);
        Log(L"exportToArchive FEHLER: " + lastError_);
        return 0;
    }

    // Der Pfad muss gemerkt werden, BEVOR das Archiv geschlossen
    // wird - Close() leert ihn.
    const std::string archivePath = archive_.Path();
    const std::string entryName = sourceEntry_;
    // Das Archiv loslassen, sonst haelt der eigene Lesezugriff die
    // Datei offen, waehrend sie ersetzt werden soll.
    archive_.Close();
    modelNames_.clear();

    const baf::LabWriteResult written =
        baf::ReplaceEntry(archivePath, entryName, bytes, true);

    // Wieder oeffnen, egal wie es ausgegangen ist - sonst steht die
    // Oberflaeche ohne Modellliste da.
    const baf::LabResult reopened = archive_.Open(archivePath);
    if (reopened.ok) modelNames_ = archive_.NamesWithExtension("baf");

    if (!written.ok) {
        lastError_ = Latin1ToWide(written.error);
        Log(L"exportToArchive FEHLER: " + lastError_);
        return 0;
    }
    sourceBytes_ = bytes;
    timings_ = L"exportToArchive: " + MillisSince(started);
    exportReport_ = Latin1ToWide(written.report);
    Log(L"exportToArchive: " + exportReport_ + timings_);
    return 1;
}

// ------------------------------------------------------------
//  Eine fertige .baf von der Platte ins Archiv legen.
//
//  Ohne Umweg ueber Import und Export - die Bytes gehen so
//  hinein, wie sie auf der Platte liegen. Gedacht zum Eingrenzen:
//  wer wissen will, ob ein Fehler am Schreibweg oder am Inhalt
//  liegt, legt eine unveraenderte Datei hinein und sieht im Spiel
//  nach.
//
//  Geprueft wird trotzdem: was hier hineingeht, muss sich als
//  .baf lesen lassen. Eine kaputte Datei ins Archiv zu legen
//  waere sonst zu leicht.
// ------------------------------------------------------------
int BafToolkitInterface::PutFile(const MCHAR* entryName, const MCHAR* path) {
    ClearMessages();
    if (!archive_.IsOpen()) { lastError_ = L"putFile: no archive open"; return 0; }
    if (entryName == nullptr || entryName[0] == _T('\0')) {
        lastError_ = L"putFile: no entry name";
        return 0;
    }
    if (path == nullptr || path[0] == _T('\0')) { lastError_ = L"putFile: no path"; return 0; }

    std::vector<uint8_t> bytes;
    {
        std::FILE* handle = baf::OpenBinaryRead(WideToUtf8(path));
        if (handle == nullptr) {
            lastError_ = Msg() << L"putFile: '" << path << L"' cannot be read";
            return 0;
        }
        std::fseek(handle, 0, SEEK_END);
        const long size = std::ftell(handle);
        std::fseek(handle, 0, SEEK_SET);
        if (size <= 0) { std::fclose(handle); lastError_ = L"putFile: file is empty"; return 0; }
        bytes.resize(static_cast<size_t>(size));
        const bool ok = std::fread(bytes.data(), 1, bytes.size(), handle) == bytes.size();
        std::fclose(handle);
        if (!ok) { lastError_ = L"putFile: file could not be read completely"; return 0; }
    }

    // Erst lesen, dann legen. Was nicht als .baf durchgeht, hat
    // im Archiv nichts zu suchen.
    baf::BafFile probe;
    const std::string name = WideToLatin1(entryName);
    const baf::ReadResult check = baf::ReadBafBuffer(bytes.data(), bytes.size(), name, probe);
    if (!check.ok) {
        lastError_ = L"putFile: not a readable .baf - " + Latin1ToWide(check.error);
        Log(L"putFile: " + lastError_);
        return 0;
    }

    // Vorher nachsehen, ob es den Eintrag ueberhaupt gibt. Die
    // Meldung aus ReplaceEntry nennt nur den Namen; hilfreicher
    // ist, gleich zu sagen, dass hier ein ARCHIVNAME hingehoert
    // und kein Dateiname.
    if (archive_.Find(name) == nullptr) {
        lastError_ = Msg() << L"putFile: '" << entryName
                           << L"' is not in the archive. The entry name is the "
                              L"name INSIDE the archive - obiwan.baf, for "
                              L"instance - not the name of the file on disk.";
        Log(L"putFile: " + lastError_);
        return 0;
    }

    const std::string archivePath = archive_.Path();
    archive_.Close();
    modelNames_.clear();

    const baf::LabWriteResult written = baf::ReplaceEntry(archivePath, name, bytes, true);

    const baf::LabResult reopened = archive_.Open(archivePath);
    if (reopened.ok) modelNames_ = archive_.NamesWithExtension("baf");

    if (!written.ok) {
        lastError_ = Latin1ToWide(written.error);
        Log(L"putFile FAILED: " + lastError_);
        return 0;
    }
    exportReport_ = Latin1ToWide(written.report);
    Log(Msg() << L"putFile: '" << path << L"' -> '" << entryName << L"', "
              << bytes.size() << L" bytes, " << probe.clips.size() << L" clips\n"
              << exportReport_);
    return 1;
}

int BafToolkitInterface::RestoreArchive() {
    ClearMessages();
    if (!archive_.IsOpen()) { lastError_ = L"restoreArchive: no archive open"; return 0; }

    const std::string archivePath = archive_.Path();
    archive_.Close();
    modelNames_.clear();
    model_.reset();
    sourceBytes_.clear();
    sourceEntry_.clear();

    const baf::LabWriteResult restored = baf::RestoreBackup(archivePath);

    const baf::LabResult reopened = archive_.Open(archivePath);
    if (reopened.ok) modelNames_ = archive_.NamesWithExtension("baf");

    if (!restored.ok) {
        lastError_ = Latin1ToWide(restored.error);
        Log(L"restoreArchive FEHLER: " + lastError_);
        return 0;
    }
    exportReport_ = Latin1ToWide(restored.report);
    Log(L"restoreArchive: " + exportReport_);
    return 1;
}

int BafToolkitInterface::VerifyRoundTrip() {
    ClearMessages();
    if (!model_) { lastError_ = L"verifyRoundTrip: no model open"; return 0; }
    if (sourceBytes_.empty()) {
        lastError_ = L"verifyRoundTrip: the source bytes are not known";
        return 0;
    }
    std::vector<uint8_t> bytes;
    std::string error;
    if (!baf::BuildBaf(*model_, bytes, error)) {
        lastError_ = Latin1ToWide(error);
        return 0;
    }
    const size_t difference = baf::FirstDifference(sourceBytes_, bytes);
    if (difference == SIZE_MAX) {
        Log(Msg() << L"verifyRoundTrip: byte-gleich (" << bytes.size() << L" Byte)");
        return 1;
    }
    // Der Offset ist das Wertvolle daran: er sagt, in welchem
    // Block es auseinanderlaeuft.
    lastError_ = (Msg() << L"first difference at Offset " << HexText(difference)
                        << L" (" << sourceBytes_.size() << L" gegen "
                        << bytes.size() << L" Byte)").str();
    Log(L"verifyRoundTrip: " + lastError_);
    return 0;
}

// ============================================================
//  Stufe 6 - Schalter, Rohdaten, Grenzen
// ============================================================

int BafToolkitInterface::SetImportNormals(int on) {
    importNormals_ = (on != 0);
    Log(importNormals_ ? L"use the vertex normals from the file"
                       : L"let Max compute the vertex normals");
    return importNormals_ ? 1 : 0;
}

int BafToolkitInterface::SetHideUndrawn(int on) {
    hideUndrawn_ = (on != 0);
    return hideUndrawn_ ? 1 : 0;
}

int BafToolkitInterface::SetStoreProps(int on) {
    storeProps_ = (on != 0);
    if (!storeProps_) {
        Log(L"WARNING: the raw data is NOT stored. A later export "
            L"will then not be able to reconstruct the file.");
    }
    return storeProps_ ? 1 : 0;
}

// ------------------------------------------------------------
//  Die Blickrichtung
//
//  Die Engine schaut nach +Y - nachgeprueft an der
//  Fussgeometrie: die Zehen liegen bei allen geprueften Modellen
//  auf der +Y-Seite des Knoechels.
//
//  Max' Frontansicht blickt ebenfalls nach +Y. Ein Charakter mit
//  Blickrichtung +Y steht dort also mit dem Ruecken zum
//  Betrachter. Das ist keine Eigenheit dieses Importers - in
//  Blender ist es genauso, und das Addon dreht ebenfalls nichts.
//
//  Trotzdem will man das Modell von vorne sehen. Deshalb wird
//  das ganze Rig um 180 Grad um Z gedreht, und beim
//  Zurueckschreiben wieder heraus. Die Datei bleibt unveraendert.
// ------------------------------------------------------------
baf::Matrix34 BafToolkitInterface::FrontRotation() const {
    baf::Matrix34 tm{};
    if (!faceFront_) {
        tm.m[0][0] = tm.m[1][1] = tm.m[2][2] = 1.0;
        return tm;
    }
    // 180 Grad um Z: right -> -right, forward -> -forward.
    tm.m[0][0] = -1.0;
    tm.m[1][1] = -1.0;
    tm.m[2][2] =  1.0;
    return tm;
}

baf::Matrix34 BafToolkitInterface::FrontRotationInverse() const {
    // Bei einer reinen Drehung ist die Umkehrung die
    // Transponierte. Bei 180 Grad um Z ist das dieselbe Matrix,
    // aber das steht hier nicht als Annahme drin.
    const baf::Matrix34 forward = FrontRotation();
    baf::Matrix34 tm{};
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            tm.m[row][column] = forward.m[column][row];
        }
    }
    return tm;
}

int BafToolkitInterface::SetFaceFront(int on) {
    faceFront_ = (on != 0);
    Log(faceFront_ ? L"rig turned to face the front view (180 degrees about Z)"
                   : L"rig keeps the orientation of the file (looking along +Y)");
    return faceFront_ ? 1 : 0;
}

int BafToolkitInterface::SetShowBounds(int on) {
    showBounds_ = (on != 0);
    return showBounds_ ? 1 : 0;
}

const MCHAR* BafToolkitInterface::GetLimitsReport() {
    if (!model_) return Give(L"no model open");
    baf::LimitReport report;
    baf::CheckLimits(*model_, report);
    return Give(Latin1ToWide(report.text));
}

int BafToolkitInterface::LimitsExceeded() {
    if (!model_) return 0;
    baf::LimitReport report;
    baf::CheckLimits(*model_, report);
    return report.exceeded;
}

// ============================================================
//  Stufe 5 - Animationen
// ============================================================


int BafToolkitInterface::SetBake(int on) {
    bake_ = (on != 0);
    Log(bake_ ? L"mode: bake every frame"
              : L"mode: take only the keys from the file");
    return bake_ ? 1 : 0;
}

int BafToolkitInterface::ClipSceneFrames(int clipIndex) {
    if (!model_ || clipIndex < 0 ||
        static_cast<size_t>(clipIndex) >= model_->clips.size()) return 0;
    const baf::Clip& clip = model_->clips[static_cast<size_t>(clipIndex)];
    const double last = ClipFrameToScene(
        static_cast<double>(clip.numFrames > 0 ? clip.numFrames - 1 : 0), clip.fps);
    return static_cast<int>(last + 0.5) + 1;
}

int BafToolkitInterface::ClipStart(int clipIndex) {
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= clipStart_.size()) return -1;
    return clipStart_[static_cast<size_t>(clipIndex)];
}

int BafToolkitInterface::ClipEnd(int clipIndex) {
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= clipEnd_.size()) return -1;
    return clipEnd_[static_cast<size_t>(clipIndex)];
}

int BafToolkitInterface::ClearAnim() {
    int cleared = 0;
    for (ULONG handle : boneHandles_) {
        INode* node = NodeFromHandle(handle);
        if (node == nullptr) continue;
        Control* controller = node->GetTMController();
        if (controller == nullptr) continue;
        // Auf dem TM-Controller ansetzen, nicht auf den beiden
        // Unter-Controllern: die Rekursion erreicht von dort auch
        // die Skalierung und alles, was Max sonst noch
        // darunterhaengt.
        ClearKeys(controller);
        ++cleared;
    }
    clipStart_.clear();
    clipEnd_.clear();
    Log(Msg() << L"clearAnim: " << cleared << L" Bones geleert");
    return cleared;
}

// ------------------------------------------------------------
//  Die Keys EINES Clips auf das Skelett legen.
//
//  restKeys: an beiden Enden des Clips einen Key setzen, auch
//  fuer Bones, die dieser Clip gar nicht anfasst. Im
//  Sequenzmodus ist das noetig - sonst interpoliert Max quer
//  durch die Luecke zwischen zwei Clips und der Arm wandert
//  durch den Koerper.
// ------------------------------------------------------------
int BafToolkitInterface::ApplyClip(size_t clipIndex, int startFrame, bool restKeys) {
    if (!model_ || clipIndex >= model_->clips.size()) return 0;
    const baf::Clip& clip = model_->clips[clipIndex];
    const int ticks = GetTicksPerFrame();

    // OHNE DAS ENTSTEHT KEIN EINZIGER KEY. Siehe AnimateGuard in
    // baftoolkit.h - SetValue setzt sonst nur den konstanten Wert
    // des Controllers, ohne Fehlermeldung.
    AnimateGuard animateGuard;

    // Welche Bones fasst dieser Clip an? Fuer die uebrigen kommen
    // spaeter Ruhelage-Keys.
    std::vector<char> touched(boneHandles_.size(), 0);

    int keysSet = 0;
    for (size_t trackIndex = 0; trackIndex < clip.tracks.size(); ++trackIndex) {
        const baf::Track& track = clip.tracks[trackIndex];

        // Bindung ueber den INDEX, nicht ueber den Namen: in
        // einigen Modellen widersprechen sich die beiden.
        size_t joint = static_cast<size_t>(trackIndex);
        if (track.jointIndex >= 0 &&
            static_cast<size_t>(track.jointIndex) < model_->nodes.size()) {
            joint = static_cast<size_t>(track.jointIndex);
        }
        if (joint >= boneHandles_.size()) continue;

        INode* node = NodeFromHandle(boneHandles_[joint]);
        if (node == nullptr) continue;
        Control* controller = node->GetTMController();
        if (controller == nullptr) continue;
        Control* positionControl = controller->GetPositionController();
        Control* rotationControl = controller->GetRotationController();
        if (positionControl == nullptr || rotationControl == nullptr) continue;

        touched[joint] = 1;

        // Die abzutastenden Zeitpunkte, in Frames des CLIPS.
        std::vector<double> sampleFrames;
        if (bake_) {
            const int count = (clip.numFrames > 0) ? clip.numFrames : 1;
            sampleFrames.reserve(static_cast<size_t>(count));
            for (int frame = 0; frame < count; ++frame) {
                sampleFrames.push_back(static_cast<double>(frame));
            }
        } else {
            sampleFrames.reserve(track.keys.size() * 2);
            for (size_t k = 0; k < track.keys.size(); ++k) {
                const baf::Key& key = track.keys[k];
                sampleFrames.push_back(static_cast<double>(key.frame));

                // Ein Key OHNE sein Interpolationsbit HAELT seinen
                // Wert bis zum naechsten. Max wuerde dazwischen
                // interpolieren, also wird kurz vor dem naechsten
                // Key derselbe Wert noch einmal gesetzt - das ergibt
                // dieselbe Stufe, ohne an den Tangenten zu drehen.
                //
                // Das ist eine Naeherung, und sie steht hier als
                // solche: liegen zwei Keys weniger als einen Frame
                // auseinander, passt kein Zwischenkey mehr hinein
                // und Max interpoliert doch. Wer es genau braucht,
                // nimmt den Backmodus - der ist deshalb der
                // Standard.
                const uint32_t bothBits = baf::kKeyLerpPos | baf::kKeyLerpOrient;
                if ((key.flags & bothBits) == bothBits) continue;
                if (k + 1 >= track.keys.size()) continue;
                const double nextFrame = static_cast<double>(track.keys[k + 1].frame);
                if (nextFrame - static_cast<double>(key.frame) > 1.0) {
                    sampleFrames.push_back(nextFrame - 1.0);
                }
            }
        }

        // Nur ein WURZELknoten muss gedreht werden. Ein Kind liegt
        // relativ zu seinem Elternteil, und der ist schon gedreht -
        // wer es trotzdem drehte, drehte doppelt.
        const bool turnThis = faceFront_ && (model_->nodes[joint].parent < 0);
        const baf::Matrix34 turn = FrontRotation();

        Quat previous;
        bool havePrevious = false;
        for (double clipFrame : sampleFrames) {
            baf::Matrix34 local =
                baf::SampleLocalMatrix(track, clipFrame, static_cast<double>(scale_));
            if (turnThis) local = baf::Concat(turn, local);

            Matrix3 tm(1);
            for (int row = 0; row < 4; ++row) {
                tm.SetRow(row, Point3(static_cast<float>(local.m[row][0]),
                                      static_cast<float>(local.m[row][1]),
                                      static_cast<float>(local.m[row][2])));
            }

            // Quaternion direkt uebernehmen, NICHT konjugieren.
            // Die SDK-Referenz zu Control::SetValue ist eindeutig:
            // bei CTRL_ABSOLUTE soll der Controller den Wert
            // schlicht speichern.
            Quat rotation(tm);
            // DotProd ist fuer Point3 und Point4 ueberladen, nicht
            // fuer Quat - deshalb von Hand. Vier Komponenten, nicht
            // drei: das w gehoert dazu, sonst stimmt das Vorzeichen
            // gerade dort nicht, wo es zaehlt.
            const float dot = rotation.x * previous.x + rotation.y * previous.y +
                              rotation.z * previous.z + rotation.w * previous.w;
            if (havePrevious && dot < 0.0f) {
                // Das Vorzeichen eines Quaternions ist frei, aber
                // ein Sprung darin nimmt bei der Interpolation den
                // langen Weg herum - der Arm dreht sich dann einmal
                // komplett.
                rotation = -rotation;
            }
            previous = rotation;
            havePrevious = true;

            const double sceneFrame =
                static_cast<double>(startFrame) + ClipFrameToScene(clipFrame, clip.fps);
            const TimeValue time =
                static_cast<TimeValue>((sceneFrame + (sceneFrame < 0 ? -0.5 : 0.5))) * ticks;

            Point3 position(static_cast<float>(local.m[3][0]),
                            static_cast<float>(local.m[3][1]),
                            static_cast<float>(local.m[3][2]));
            positionControl->SetValue(time, &position, TRUE, CTRL_ABSOLUTE);
            rotationControl->SetValue(time, &rotation, TRUE, CTRL_ABSOLUTE);
            ++keysSet;
        }
    }

    if (!restKeys) return keysSet;

    // Ruhelage-Keys an beiden Enden fuer alles, was dieser Clip
    // nicht anfasst.
    const int lastFrame = startFrame + ClipSceneFrames(static_cast<int>(clipIndex)) - 1;
    for (size_t i = 0; i < boneHandles_.size(); ++i) {
        if (touched[i]) continue;
        INode* node = NodeFromHandle(boneHandles_[i]);
        if (node == nullptr || i >= model_->nodes.size()) continue;
        Control* controller = node->GetTMController();
        if (controller == nullptr) continue;
        Control* positionControl = controller->GetPositionController();
        Control* rotationControl = controller->GetRotationController();
        if (positionControl == nullptr || rotationControl == nullptr) continue;

        baf::Matrix34 local =
            baf::NodeLocalMatrix(model_->nodes[i], static_cast<double>(scale_));
        if (faceFront_ && model_->nodes[i].parent < 0) {
            local = baf::Concat(FrontRotation(), local);
        }
        Matrix3 tm(1);
        for (int row = 0; row < 4; ++row) {
            tm.SetRow(row, Point3(static_cast<float>(local.m[row][0]),
                                  static_cast<float>(local.m[row][1]),
                                  static_cast<float>(local.m[row][2])));
        }
        Quat rotation(tm);
        Point3 position(static_cast<float>(local.m[3][0]),
                        static_cast<float>(local.m[3][1]),
                        static_cast<float>(local.m[3][2]));
        for (int frame : {startFrame, lastFrame}) {
            const TimeValue time = frame * ticks;
            positionControl->SetValue(time, &position, TRUE, CTRL_ABSOLUTE);
            rotationControl->SetValue(time, &rotation, TRUE, CTRL_ABSOLUTE);
            ++keysSet;
        }
        if (lastFrame == startFrame) break;
    }
    return keysSet;
}

int BafToolkitInterface::BuildAnim(int clipIndex) {
    ClearMessages();
    if (!model_) { lastError_ = L"buildAnim: no model open"; return 0; }
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= model_->clips.size()) {
        lastError_ = L"buildAnim: clip number out of range";
        Log(L"buildAnim FEHLER: " + lastError_);
        return 0;
    }
    if (boneHandles_.empty()) {
        lastError_ = L"buildAnim: call buildSkeleton first";
        Log(L"buildAnim FEHLER: " + lastError_);
        return 0;
    }
    Interface* ip = GetCOREInterface();
    if (ip == nullptr) { lastError_ = L"buildAnim: no core interface"; return 0; }

    const baf::Clip& clip = model_->clips[static_cast<size_t>(clipIndex)];
    Log(Msg() << L"buildAnim " << clipIndex << L" '" << Latin1ToWide(clip.name)
              << L"': " << Latin1ToWide(baf::FormatFloat(clip.fps, 1)) << L" fps, "
              << clip.numFrames << L" Frames -> "
              << ClipSceneFrames(clipIndex) << L" frames at "
              << static_cast<int>(SceneFps()) << L" fps, "
              << (bake_ ? L"gebacken" : L"nur Keys"));

    const auto started = std::chrono::steady_clock::now();
    SceneRedrawGuard redrawGuard(ip);
    HoldSuspendGuard holdGuard;

    ClearAnim();
    const int keysSet = ApplyClip(static_cast<size_t>(clipIndex), 0, false);

    // ------------------------------------------------------
    //  Der Animationsbereich braucht MINDESTENS EINEN FRAME.
    //
    //  152 der 2.281 Clips im Bestand haben nur einen einzigen
    //  Frame - "temp.key" etwa, oder "pushblok". Daraus wuerde
    //  Interval(0, 0), und Max rechnet intern durch die Laenge
    //  des Bereichs. Das Ergebnis ist eine
    //  EXCEPTION_INT_DIVIDE_BY_ZERO mitten in Max, die wie ein
    //  Fehler im Plugin aussieht.
    // ------------------------------------------------------
    const int length = ClipSceneFrames(clipIndex);
    const int lastFrame = (length > 1) ? (length - 1) : 1;
    ip->SetAnimRange(Interval(0, lastFrame * GetTicksPerFrame()));

    clipStart_.assign(model_->clips.size(), -1);
    clipEnd_.assign(model_->clips.size(), -1);
    clipStart_[static_cast<size_t>(clipIndex)] = 0;
    clipEnd_[static_cast<size_t>(clipIndex)] = (length > 0 ? length - 1 : 0);
    Log(Msg() << L"animation range: 0 to " << lastFrame);

    // ------------------------------------------------------
    //  Festhalten, welcher Clip jetzt in der Szene steht.
    //
    //  Sonst muss der Anwender ihn beim Export in einer Liste
    //  mit 114 Eintraegen selbst wiederfinden - und ein Griff
    //  daneben ueberschreibt den falschen Clip.
    //
    //  Der Name landet am ersten Bone, bei den uebrigen
    //  Rohdaten. Damit ueberlebt er Speichern und Neustart.
    // ------------------------------------------------------
    if (!boneHandles_.empty()) {
        SetProp(NodeFromHandle(boneHandles_[0]), L"bafSceneClip",
                Latin1ToWide(clip.name));
        sceneClip_ = Latin1ToWide(clip.name);
    }

    timings_ = L"buildAnim: " + MillisSince(started);
    Log(Msg() << L"buildAnim: " << keysSet << L" Keys, " << timings_);
    return keysSet;
}

int BafToolkitInterface::BuildSequence(int spacing) {
    ClearMessages();
    if (!model_) { lastError_ = L"buildSequence: no model open"; return 0; }
    if (model_->clips.empty()) { lastError_ = L"buildSequence: the model has no clips"; return 0; }
    if (boneHandles_.empty()) {
        lastError_ = L"buildSequence: call buildSkeleton first";
        Log(L"buildSequence FAILED: " + lastError_);
        return 0;
    }
    Interface* ip = GetCOREInterface();
    if (ip == nullptr) { lastError_ = L"buildSequence: no core interface"; return 0; }
    if (spacing < 0) spacing = 0;

    Log(Msg() << L"buildSequence: " << model_->clips.size() << L" Clips, Abstand "
              << spacing << L", " << (bake_ ? L"gebacken" : L"nur Keys"));

    const auto started = std::chrono::steady_clock::now();
    SceneRedrawGuard redrawGuard(ip);
    HoldSuspendGuard holdGuard;

    ClearAnim();
    clipStart_.assign(model_->clips.size(), -1);
    clipEnd_.assign(model_->clips.size(), -1);

    int cursor = 0;
    int keysSet = 0;
    for (size_t i = 0; i < model_->clips.size(); ++i) {
        const int length = ClipSceneFrames(static_cast<int>(i));
        clipStart_[i] = cursor;
        clipEnd_[i]   = cursor + (length > 0 ? length - 1 : 0);
        keysSet += ApplyClip(i, cursor, true);
        LogDetail(Msg() << L"Clip " << i << L" '"
                        << Latin1ToWide(model_->clips[i].name) << L"' Frames "
                        << clipStart_[i] << L"-" << clipEnd_[i]);
        cursor = clipEnd_[i] + 1 + spacing;
    }

    const int total = (cursor > 0) ? cursor - spacing : 0;
    // Auch hier mindestens ein Frame, siehe buildAnim.
    const int lastFrame = (total > 1) ? (total - 1) : 1;
    ip->SetAnimRange(Interval(0, lastFrame * GetTicksPerFrame()));

    // Im Sequenzmodus steht kein EINZELNER Clip in der Szene.
    // Die Marke waere dann eine Behauptung, also weg damit.
    if (!boneHandles_.empty()) {
        SetProp(NodeFromHandle(boneHandles_[0]), L"bafSceneClip", L"");
        sceneClip_.clear();
    }

    timings_ = L"buildSequence: " + MillisSince(started);
    Log(Msg() << L"buildSequence: " << keysSet << L" Keys, " << total
              << L" Frames gesamt, " << timings_);
    return total;
}

int BafToolkitInterface::AnimDump(int clipIndex, const MCHAR* outPath) {
    ClearMessages();
    if (!model_) { lastError_ = L"animDump: no model open"; return 0; }
    if (!outPath) { lastError_ = L"animDump: no path"; return 0; }
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= model_->clips.size()) {
        lastError_ = L"animDump: clip number out of range";
        return 0;
    }
#if defined(_WIN32)
    std::ofstream out(outPath, std::ios::binary);
#else
    std::ofstream out(WideToUtf8(outPath), std::ios::binary);
#endif
    if (!out) {
        lastError_ = Msg() << L"animDump: '" << outPath << L"' cannot be written";
        return 0;
    }
    baf::WriteAnimDump(*model_, static_cast<size_t>(clipIndex),
                       static_cast<double>(scale_), out);
    out.close();
    if (!out) { lastError_ = L"animDump: write aborted"; return 0; }
    Log(Msg() << L"animDump: Clip " << clipIndex << L" to '" << outPath << L"'");
    return 1;
}

int BafToolkitInterface::SceneAnimDump(int clipIndex, const MCHAR* outPath) {
    ClearMessages();
    if (!model_) { lastError_ = L"sceneAnimDump: no model open"; return 0; }
    if (!outPath) { lastError_ = L"sceneAnimDump: no path"; return 0; }
    if (clipIndex < 0 || static_cast<size_t>(clipIndex) >= model_->clips.size()) {
        lastError_ = L"sceneAnimDump: clip number out of range";
        return 0;
    }
    if (clipIndex >= static_cast<int>(clipStart_.size()) ||
        clipStart_[static_cast<size_t>(clipIndex)] < 0) {
        lastError_ = L"sceneAnimDump: that clip is not in the scene";
        return 0;
    }
#if defined(_WIN32)
    std::ofstream out(outPath, std::ios::binary);
#else
    std::ofstream out(WideToUtf8(outPath), std::ios::binary);
#endif
    if (!out) {
        lastError_ = Msg() << L"sceneAnimDump: '" << outPath
                           << L"' cannot be written";
        return 0;
    }

    const baf::Clip& clip = model_->clips[static_cast<size_t>(clipIndex)];
    const int startFrame = clipStart_[static_cast<size_t>(clipIndex)];
    const int ticks = GetTicksPerFrame();

    out << "anim " << clipIndex << " name=" << clip.name << " fps="
        << baf::FormatFloat(clip.fps) << " frames=" << clip.numFrames
        << " tracks=" << clip.tracks.size() << "\n";

    for (size_t trackIndex = 0; trackIndex < clip.tracks.size(); ++trackIndex) {
        const baf::Track& track = clip.tracks[trackIndex];
        size_t joint = trackIndex;
        if (track.jointIndex >= 0 &&
            static_cast<size_t>(track.jointIndex) < model_->nodes.size()) {
            joint = static_cast<size_t>(track.jointIndex);
        }
        out << "  at " << trackIndex << " joint=" << joint << " name=" << track.name
            << " keys=" << track.keys.size() << "\n";

        INode* node = (joint < boneHandles_.size())
            ? NodeFromHandle(boneHandles_[joint]) : nullptr;

        for (size_t keyIndex = 0; keyIndex < track.keys.size(); ++keyIndex) {
            const double clipFrame = static_cast<double>(track.keys[keyIndex].frame);
            const double sceneFrame =
                static_cast<double>(startFrame) + ClipFrameToScene(clipFrame, clip.fps);
            const TimeValue time =
                static_cast<TimeValue>(sceneFrame + 0.5) * ticks;

            // Aus der SZENE: die Weltmatrix des Bones abzueglich
            // der seines Elternteils - das ist die lokale Lage,
            // also dasselbe, was der Dump aus den Dateidaten zeigt.
            baf::Matrix34 local{};
            local.m[0][0] = local.m[1][1] = local.m[2][2] = 1.0;
            if (node != nullptr) {
                Matrix3 tm = node->GetNodeTM(time);
                INode* parent = node->GetParentNode();
                if (parent != nullptr && !parent->IsRootNode()) {
                    tm = tm * Inverse(parent->GetNodeTM(time));
                }
                for (int row = 0; row < 4; ++row) {
                    const Point3 point = tm.GetRow(row);
                    local.m[row][0] = point.x;
                    local.m[row][1] = point.y;
                    local.m[row][2] = point.z;
                }
            }
            out << "    ak " << keyIndex << " f="
                << baf::FormatFloat(track.keys[keyIndex].frame)
                << " flags=" << track.keys[keyIndex].flags;
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 3; ++column) {
                    out << " " << baf::FormatFloat(local.m[row][column]);
                }
            }
            out << "\n";
        }
    }
    out.close();
    if (!out) { lastError_ = L"sceneAnimDump: write aborted"; return 0; }
    Log(Msg() << L"sceneAnimDump: Clip " << clipIndex << L" to '" << outPath << L"'");
    return 1;
}

// ============================================================
//  Stufe 4 - Texturen und Materialien
// ============================================================

Mtl* BafToolkitInterface::MaterialFromHandle() const {
    if (materialHandle_ == 0) return nullptr;
    Animatable* anim = Animatable::GetAnimByHandle(materialHandle_);
    if (anim == nullptr) return nullptr;
    if (anim->SuperClassID() != MATERIAL_CLASS_ID) return nullptr;
    return static_cast<Mtl*>(anim);
}

// ------------------------------------------------------------
//  Ein Texturordner, auch wenn keiner gewaehlt wurde.
//
//  Vorher war es so: ohne gewaehlten Ordner wurden keine TGA
//  geschrieben, und die Materialien bekamen keine Bitmap. Das
//  Modell war dann grau, ohne dass irgendwo stand warum - man
//  muss erst wissen, dass da ein Ordner fehlt.
//
//  Jetzt wird ein Standardordner benutzt: der Bilderordner des
//  Projekts, mit einem Unterordner je Modell. Wer einen anderen
//  will, waehlt ihn weiterhin selbst.
// ------------------------------------------------------------
std::wstring BafToolkitInterface::EnsureTextureDir() {
    if (!textureDir_.empty()) return textureDir_;

    Interface* ip = GetCOREInterface();
    std::wstring base;
    if (ip != nullptr) {
        // ------------------------------------------------------
        //  Erst in eine MSTR, dann ueber .data() weiter.
        //
        //  Ab 3ds Max 2025 liefert GetDir eine MSTR ALS WERT,
        //  vorher einen const MCHAR*. Und WStr hat seine
        //  Umwandlung nach const wchar_t* fuer Rvalues
        //  ABSICHTLICH geloescht - die Doku sagt dazu:
        //
        //    "Delete these two overloads to prevent accidental
        //     dangling pointers from temporary strings."
        //
        //  Ein direkt zugewiesener Zeiger zeigte naemlich auf
        //  eine Zeichenkette, die am Ende der Zeile schon
        //  freigegeben ist. Der Compiler verhindert das jetzt,
        //  und die Meldung
        //    C2280: WStr::operator const wchar_t*(void) &&
        //  ist genau dieser Schutz.
        //
        //  Die MSTR benannt zu halten loest es fuer BEIDE Seiten:
        //  bei den alten SDKs wird sie aus dem Zeiger gebaut, bei
        //  den neuen kopiert, und in beiden Faellen lebt sie lang
        //  genug.
        // ------------------------------------------------------
        MSTR images;
        images = ip->GetDir(APP_IMAGE_DIR);
        if (images.length() > 0) base = images.data();
        if (base.empty()) {
            MSTR temporary;
            temporary = ip->GetDir(APP_TEMP_DIR);
            if (temporary.length() > 0) base = temporary.data();
        }
    }
    if (base.empty()) return std::wstring();

    while (!base.empty() && (base.back() == L'\\' || base.back() == L'/')) base.pop_back();

    std::wstring name = model_ ? Latin1ToWide(baf::SafeFileName(model_->name, "baf"))
                              : std::wstring(L"baf");
    const size_t dot = name.find_last_of(L'.');
    if (dot != std::wstring::npos) name = name.substr(0, dot);
    if (name.empty()) name = L"baf";

    const std::wstring folder = base + L"\\BAF_" + name;
    // Fehlschlag ist kein Grund abzubrechen: gibt es den Ordner
    // schon, meldet CreateDirectory ebenfalls false.
    CreateDirectoryW(folder.c_str(), nullptr);
    textureDir_ = folder;
    Log(L"texture folder (automatic): " + textureDir_);
    return textureDir_;
}

const MCHAR* BafToolkitInterface::SetTextureDir(const MCHAR* path) {
    // Ein leerer Pfad setzt NICHT zurueck, sondern fragt nur ab -
    // so kann die Oberflaeche den automatisch gewaehlten Ordner
    // anzeigen, ohne ihn zu kennen.
    if (path != nullptr && path[0] != _T('\0')) {
        textureDir_ = path;
        Log(L"texture folder: " + textureDir_);
    }
    return Give(textureDir_);
}

int BafToolkitInterface::SlotCount() {
    return static_cast<int>(slots_.size());
}

const MCHAR* BafToolkitInterface::GetSlotInfo(int index) {
    if (index < 0 || static_cast<size_t>(index) >= slots_.size()) {
        return Give(std::wstring());
    }
    const baf::MaterialSlot& slot = slots_[static_cast<size_t>(index)];
    const wchar_t* blendName = (slot.blend == 0) ? L"deckend"
                             : (slot.blend == 2) ? L"additiv" : L"transparent";
    std::wstring name;
    if (slot.material < model_->materials.size()) {
        name = Latin1ToWide(model_->materials[slot.material].name);
    }
    return Give((Msg() << index << L": " << name << L"  " << blendName
                       << (slot.twoSided ? L", zweiseitig" : L"")
                       << (slot.fullbright ? L", vollhell" : L"")
                       << (slot.envMap ? L", Umgebung" : L"")
                       << L", Alpha " << Latin1ToWide(baf::FormatFloat(slot.alpha, 2))).str());
}

int BafToolkitInterface::ExportTextures() {
    ClearMessages();
    Log(L"exportTextures: start");
    if (!model_) { lastError_ = L"exportTextures: no model open"; return 0; }
    if (EnsureTextureDir().empty()) {
        lastError_ = L"exportTextures: no texture folder, and none could "
                     L"be created either";
        return 0;
    }

    const auto started = std::chrono::steady_clock::now();
    std::wstring folder = textureDir_;
    while (!folder.empty() && (folder.back() == L'\\' || folder.back() == L'/')) folder.pop_back();

    int written = 0, skipped = 0;
    for (size_t materialIndex = 0; materialIndex < model_->materials.size(); ++materialIndex) {
        const baf::Material& material = model_->materials[materialIndex];
        for (size_t celIndex = 0; celIndex < material.cels.size(); ++celIndex) {
            baf::DecodedCel decoded;
            if (!baf::DecodeCel(*model_, materialIndex, celIndex, decoded)) {
                ++skipped;
                LogDetail(Msg() << L"Material " << materialIndex << L" Cel " << celIndex
                                << L": cannot be decoded (bit depth "
                                << material.bitDepth << L")");
                continue;
            }
            // Der Name kommt aus einer Spieldatei. Ungeprueft an
            // einen Pfad gehaengt schriebe der Export irgendwohin -
            // ein Materialname darf "..\" enthalten.
            std::string safe = baf::SafeFileName(material.name,
                                                 "mat" + std::to_string(materialIndex));
            const size_t dot = safe.find_last_of('.');
            if (dot != std::string::npos) safe = safe.substr(0, dot);
            if (celIndex > 0) safe += "_cel" + std::to_string(celIndex);

            const std::wstring target = folder + L"\\" + Latin1ToWide(safe) + L".tga";
            std::string writeError;
            if (baf::WriteTga(decoded, WideToUtf8(target), writeError)) {
                ++written;
                LogDetail(Msg() << target << L"  " << decoded.width << L"x"
                                << decoded.height
                                << (decoded.keyed ? L", with transparency" : L""));
            } else {
                ++skipped;
                warnings_ += Latin1ToWide(writeError) + L"\n";
                Log(L"exportTextures: " + Latin1ToWide(writeError));
            }
        }
    }
    timings_ = L"exportTextures: " + MillisSince(started);
    Log(Msg() << L"exportTextures: " << written << L" written, " << skipped
              << L" skipped, " << timings_);
    return written;
}

int BafToolkitInterface::BuildMaterials() {
    ClearMessages();
    Log(L"buildMaterials: start");
    if (!model_) { lastError_ = L"buildMaterials: no model open"; return 0; }

    Interface* ip = GetCOREInterface();
    if (ip == nullptr) { lastError_ = L"buildMaterials: no core interface"; return 0; }

    const auto started = std::chrono::steady_clock::now();
    baf::BuildMaterialSlots(*model_, slots_);
    if (slots_.empty()) { lastError_ = L"buildMaterials: no material slots"; return 0; }

    // Max' Material-ID ist ein unsigned short. Mehr als 65.535
    // Plaetze kann es also gar nicht geben; in den Retail-Daten
    // sind es hoechstens ein paar Dutzend.
    if (slots_.size() > 0xFFFFu) {
        lastError_ = L"buildMaterials: more material slots than Max IDs";
        return 0;
    }

    MultiMtl* multi = NewDefaultMultiMtl();
    if (multi == nullptr) { lastError_ = L"buildMaterials: could not create the MultiMtl"; return 0; }
    multi->SetNumSubMtls(static_cast<int>(slots_.size()));

    std::wstring folder = EnsureTextureDir();
    while (!folder.empty() && (folder.back() == L'\\' || folder.back() == L'/')) folder.pop_back();

    int withTexture = 0;
    for (size_t i = 0; i < slots_.size(); ++i) {
        const baf::MaterialSlot& slot = slots_[i];
        StdMat2* standard = NewDefaultStdMat();
        if (standard == nullptr) continue;

        std::wstring materialName;
        if (slot.material < model_->materials.size()) {
            materialName = Latin1ToWide(model_->materials[slot.material].name);
        }
        if (materialName.empty()) materialName = L"mat" + std::to_wstring(slot.material);

        // Der Name traegt die Renderklasse mit. Zwei Plaetze mit
        // demselben Materialnamen sind sonst im Materialeditor
        // nicht auseinanderzuhalten.
        std::wstring label = materialName;
        label += (slot.blend == 0) ? L"" : (slot.blend == 2 ? L"_add" : L"_tr");
        if (slot.twoSided) label += L"_2s";
        if (slot.fullbright) label += L"_fb";
        standard->SetName(label.c_str());

        standard->SetTwoSided(slot.twoSided);
        // Die Engine beleuchtet je Vertex und kennt kein Specular.
        standard->SetShinStr(0.0f, 0);
        standard->SetShininess(0.0f, 0);
        if (slot.fullbright) standard->SetSelfIllum(1.0f, 0);
        if (slot.blend != 0) standard->SetOpacity(slot.alpha, 0);

        // Textur, wenn der Export sie geschrieben hat.
        if (!folder.empty() && slot.material < model_->materials.size()) {
            std::string safe = baf::SafeFileName(model_->materials[slot.material].name,
                                                 "mat" + std::to_string(slot.material));
            const size_t dot = safe.find_last_of('.');
            if (dot != std::string::npos) safe = safe.substr(0, dot);
            const std::wstring target = folder + L"\\" + Latin1ToWide(safe) + L".tga";

            BitmapTex* texture = NewDefaultBitmapTex();
            if (texture != nullptr) {
                texture->SetMapName(const_cast<MCHAR*>(target.c_str()));
                texture->SetName(Latin1ToWide(safe).c_str());
                // Punktabtastung: 8 Bit palettisierte Kunst von 1999.
                // Gefiltert sieht sie verwaschen aus und die
                // Farbschluessel-Kante franst aus.
                texture->SetFilterType(FILTER_NADA);

                // Der Alphakanal kommt aus der Datei, und der
                // Mono-Ausgang liefert ihn ebenfalls.
                //
                // Der zweite Aufruf ist der entscheidende: Max
                // nimmt fuer eine Transparenz-Map standardmaessig
                // die RGB-HELLIGKEIT, nicht das Alpha. Eine dunkle
                // Textur wird damit durchsichtig und eine helle
                // fast deckend - genau das war in 1.2.4 zu sehen.
                texture->SetAlphaSource(ALPHA_FILE);
                texture->SetAlphaAsMono(TRUE);

                standard->SetSubTexmap(ID_DI, texture);
                standard->EnableMap(ID_DI, TRUE);

                // Ohne diese beiden Zeilen HAT das Material die
                // Textur, aber der Viewport zeigt sie nicht - das
                // Modell bleibt grau, und im Materialeditor sieht
                // alles richtig aus. Das ist der Knopf "Show
                // Shaded Material in Viewport", nur von hier aus.
                standard->SetActiveTexmap(texture);
                standard->SetMtlFlag(MTL_TEX_DISPLAY_ENABLED, TRUE);

                // Eine Transparenz-Map NUR, wenn das Cel wirklich
                // einen Farbschluessel traegt.
                //
                // Vorher bekam sie jedes deckende Material - und
                // ein deckendes Material braucht keine. Der
                // Alphakanal einer nicht geschluesselten TGA ist
                // ueberall 255, das waere zwar harmlos, aber der
                // Mono-Ausgang stand auf RGB-Helligkeit und machte
                // daraus Durchsichtigkeit nach Farbwert.
                bool keyed = false;
                for (const baf::Cel& cel : model_->materials[slot.material].cels) {
                    if (cel.Keyed()) { keyed = true; break; }
                }
                if (keyed) {
                    standard->SetSubTexmap(ID_OP, texture);
                    standard->EnableMap(ID_OP, TRUE);
                }
                ++withTexture;
            }
        }

        multi->SetSubMtlAndName(static_cast<int>(i), standard, standard->GetName());
        LogDetail(Msg() << L"Platz " << i << L": " << label
                        << ((slot.blend == 0) ? L", deckend"
                            : (slot.blend == 2 ? L", additiv" : L", transparent")));
    }

    std::wstring multiName = sceneModelName_.empty()
        ? Latin1ToWide(model_->name) : sceneModelName_;
    if (multiName.empty()) multiName = L"BAF";
    multi->SetName(multiName.c_str());
    // MultiMtl hat kein GetHandle(). Der Weg zu einem AnimHandle
    // fuehrt ueber die statische Funktion.
    materialHandle_ = Animatable::GetHandleByAnim(multi);

    // Auf alle gebauten Meshes legen. Die Material-IDs der Faces
    // zeigen bereits auf den richtigen Platz - das hat buildMeshes
    // beim Anlegen gesetzt.
    int assigned = 0;
    for (ULONG handle : meshHandles_) {
        INode* node = NodeFromHandle(handle);
        if (node == nullptr) continue;
        node->SetMtl(multi);
        ++assigned;
    }

    timings_ = L"buildMaterials: " + MillisSince(started);
    Log(Msg() << L"buildMaterials: " << slots_.size() << L" slots, "
              << withTexture << L" with a texture, " << assigned
              << L" Objekten zugewiesen, " << timings_);
    if (assigned == 0) {
        warnings_ += L"no mesh in the scene - the material hangs off nothing\n";
    }
    return static_cast<int>(slots_.size());
}

// ============================================================
//  Allgemein
// ============================================================

// ------------------------------------------------------------
//  Uebergabe aus Max' Dateidialogen
//
//  Der Dialog laesst genau EINE Datei waehlen. Fuer BAF passt
//  das gut: ein Charakter ist eine .baf. Wer stattdessen das
//  Archiv waehlt, bekommt die Modellliste - dann ist die Auswahl
//  im Dialog nur noch die Frage, WO das Archiv liegt.
// ------------------------------------------------------------
void BafToolkitInterface::SetPendingPath(const std::wstring& path) {
    pendingPath_ = path;
}

const MCHAR* BafToolkitInterface::TakePendingPath() {
    returnBuffer_ = pendingPath_;
    pendingPath_.clear();
    return returnBuffer_.c_str();
}

// ------------------------------------------------------------
//  MAXScript aus C++ aufrufen
//
//  Die Signatur hat sich mit Max 2022 geaendert. Bis 2021:
//
//      BOOL ExecuteMAXScriptScript(MCHAR* s, BOOL quietErrors,
//                                  FPValue* fpv)
//
//  Ab 2022 steht an zweiter Stelle ein MAXScript::ScriptSource -
//  ein Namensraum, den es vorher gar nicht gab. Deshalb eine
//  Weiche statt eines Aufrufs.
//
//  MAX_RELEASE kommt aus plugapi.h und ist damit immer die
//  Wahrheit ueber das SDK, gegen das gerade uebersetzt wird:
//  2016 = 18000, 2022 = 24000, je Jahrgang 1000 mehr.
// ------------------------------------------------------------
// Eine Meldung anzeigen. Ueber MAXScript, weil ein MessageBox
// von Hand das falsche Elternfenster bekaeme und hinter Max
// verschwinden koennte.
void BafToolkitInterface::ShowMessage(const std::wstring& text) {
    // Die Version gehoert in JEDE Meldung.
    //
    // Beim Bauen und Installieren geht leicht etwas durcheinander -
    // die .dlu ist gesperrt, weil Max noch laeuft, oder es wurde
    // nur installiert und nicht neu gebaut. Dann sieht man eine
    // Meldung aus einer alten Fassung und sucht den Fehler im
    // neuen Code.
    //
    // Das ist hier dreimal passiert. Steht die Nummer im Text,
    // faellt es in dem Moment auf, in dem man die Meldung liest.
    const std::wstring stamped =
        text + L"\n\n[BafToolkit.dlu " + std::wstring(BAFTOOLKIT_VERSION_STR) + L"]";

    std::wstring escaped;
    for (wchar_t character : stamped) {
        if (character == L'"' || character == L'\\') escaped += L'\\';
        if (character == L'\n') { escaped += L"\\n"; continue; }
        escaped += character;
    }
    const std::wstring script =
        L"messageBox \"" + escaped + L"\" title:\"BAF Import\"";
#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
    ExecuteMAXScriptScript(script.c_str(), MAXScript::ScriptSource::NonEmbedded, TRUE);
#else
    ExecuteMAXScriptScript(script.c_str(), TRUE);
#endif
}

void BafToolkitInterface::RunExportMacro() {
    const MCHAR* script =
        _T("try (macros.run \"DH Tools\" \"BafExport_Open\") ")
        _T("catch (format \"[BAF] Exportmakro nicht gefunden.\\n\")");
#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
    ExecuteMAXScriptScript(script, MAXScript::ScriptSource::NonEmbedded, TRUE);
#else
    ExecuteMAXScriptScript(script, TRUE);
#endif
}

void BafToolkitInterface::RunMacro() {
    // Ueber das Makro, nicht ueber ein eigenes Fenster: zwei
    // leicht abweichende Fassungen derselben Oberflaeche waeren
    // die naechste Fehlerquelle, und die eine bekaeme dann
    // Korrekturen, die der anderen fehlen.
    const MCHAR* script =
        _T("try (macros.run \"DH Tools\" \"BafImport_Open\") ")
        _T("catch (format \"[BAF] Makro nicht gefunden.\\n\")");
#if defined(MAX_RELEASE) && (MAX_RELEASE >= 24000)
    ExecuteMAXScriptScript(script, MAXScript::ScriptSource::NonEmbedded, TRUE);
#else
    ExecuteMAXScriptScript(script, TRUE);
#endif
}

bool BafToolkitInterface::ExportFromDialog(const MCHAR* path) {
    // Kein Modell im Speicher? Dann die Szene fragen. Sie weiss
    // in aller Regel, woher sie kommt - das ueberlebt Speichern
    // und einen Neustart von Max.
    if (!model_) {
        if (ScanScene() > 0 && !sceneOrigin_.empty()) {
            AutoLoadTemplate();
        }
    }
    if (!model_) {
        lastError_ = L"Export: no model open.";
        Log(L"DoExport: " + lastError_);
        // Eine eigene Meldung mit dem echten Grund. Max haengt
        // danach noch sein "Export module failure" an - das kommt
        // von einem Rueckgabewert 0 und laesst sich nicht
        // abstellen, aber wenigstens steht die Ursache davor.
        ShowMessage(
            L"Exporting needs the model loaded in the plugin.\n\n"
            L"The writer patches the changes back into the original bytes of "
            L"the .baf. Without that template there is nothing for it to "
            L"write into - the scene alone is not enough.\n\n"
            L"So, in this order:\n"
            L"  1. File > Import, load the model\n"
            L"  2. change it in the scene\n"
            L"  3. File > Export\n\n"
            L"This scene does not know its origin - either it did not come "
            L"from a BAF import, or the archive is no longer where "
            L"it used to be.\n\n"
            L"The export window is about to open. A template can be picked "
            L"by hand there: header, palette and materials come from it, "
            L"skeleton and geometry from the scene.");
        return false;
    }
    // Erst die Szene uebernehmen, dann schreiben. Wer ueber den
    // Dialog exportiert, erwartet den Stand der SZENE - nicht den
    // der Datei, aus der importiert wurde.
    ApplyScene();
    return ExportFile(path) == 1;
}

const MCHAR* BafToolkitInterface::GetVersion()   { return BAFTOOLKIT_VERSION_STR; }
const MCHAR* BafToolkitInterface::GetLastError() { return Give(lastError_); }
const MCHAR* BafToolkitInterface::GetWarnings()  { return Give(warnings_); }
const MCHAR* BafToolkitInterface::GetLog()       { return Give(log_); }
const MCHAR* BafToolkitInterface::GetTimings()   { return Give(timings_); }

int BafToolkitInterface::SetDebug(int on) {
    debug_ = (on != 0);
    Log(debug_ ? L"verbose log ON" : L"verbose log OFF");
    return debug_ ? 1 : 0;
}

int BafToolkitInterface::ClearLog() {
    log_.clear();
    return 1;
}

int BafToolkitInterface::SaveLog(const MCHAR* outPath) {
    if (!outPath) { lastError_ = L"saveLog: no path"; return 0; }
#if defined(_WIN32)
    std::ofstream out(outPath, std::ios::binary);
#else
    std::ofstream out(WideToUtf8(outPath), std::ios::binary);
#endif
    if (!out) {
        lastError_ = Msg() << L"saveLog: '" << outPath << L"' cannot be written";
        return 0;
    }
    // Als UTF-8, damit die Datei ueberall lesbar ist.
    const std::string text = WideToUtf8(log_);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();
    return out ? 1 : 0;
}
