// ============================================================
//  .baf Parser - Header
//
//  Eine .baf haelt EINEN kompletten Charakter: Palette,
//  Texturen, Meshes, Skelett und alle Animationsclips. Es ist
//  die binaere Serialisierung eines Sith-Engine-.3do mit
//  eingebetteten .mat-Texturen und .key-Animationen.
//
//  Blockaufbau (Offsets aus dem Header, alle relativ zu 0x120):
//      0x000              Header, 0x120 Bytes
//      0x120              Palettenblock  (hdr+0xdc Bytes)
//      +                  Materialblock   (hdr+0xd0 Bytes)
//      0x120 + hdr+0xe0   Geometrieblock  (hdr+0xc0 Bytes)
//                            Geo-Header 0x84
//                            Meshes: erst ALLE Saetze, dann alle Daten
//                            Knotentabelle am Ende
//      0x120 + hdr+0xe4   Animationsblock (hdr+0xd4 Bytes)
//                            je Clip: Descriptor 0x40 + Key-Header 0x84
//                            erst alle Track-Koepfe, dann alle Keys
//                            danach die Sound-Event-Tabelle bis EOF
//
//  Die Bloecke muessen die Datei LUECKENLOS kacheln - das ist
//  die staerkste Pruefung, die es hier gibt, und sie steht ganz
//  am Anfang. Passt die Kachelung nicht, wird gar nicht erst
//  weitergelesen.
//
//  WICHTIG: haengt bewusst NICHT am 3ds Max SDK. Siehe die
//  gleiche Notiz in baf_lab.h.
// ============================================================

#pragma once

#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace baf {

// ---- Groessen der Saetze, alle aus dem Retail-Loader ----
inline constexpr size_t kHeaderBytes    = 0x120;
inline constexpr size_t kGeoHeaderBytes = 0x84;
inline constexpr size_t kMeshBytes      = 0x70;
inline constexpr size_t kFaceBytes      = 0x4c;
inline constexpr size_t kNodeBytes      = 0xb4;
inline constexpr size_t kDescBytes      = 0x40;
inline constexpr size_t kKeyHdrBytes    = 0x84;
inline constexpr size_t kKeyNodeBytes   = 0x2c;
inline constexpr size_t kKeyBytes       = 0x38;
inline constexpr size_t kEventBytes     = 0x60;
inline constexpr size_t kMatHeaderBytes = 0x4c;
inline constexpr size_t kMatCelBytes    = 0x18;

// ---- Key-Flags ----
inline constexpr uint32_t kKeyLerpPos    = 0x01;  // ohne das Bit wird die Position GEHALTEN
inline constexpr uint32_t kKeyLerpOrient = 0x02;  // ohne das Bit wird die Drehung GEHALTEN

// ---- Materialtypen ----
inline constexpr uint32_t kMatOwnPalette = 0x01;  // 768 Bytes eigene Palette hinter den Cels
inline constexpr uint32_t kMatTextured   = 0x02;  // die Faces tragen ein texIdx-Array
inline constexpr uint32_t kMatBlend      = 0x08;
inline constexpr uint32_t kMatAdditive   = 0x10;

// ---- Face-Flags ----
inline constexpr uint32_t kFaceTwoSided   = 0x0001;
inline constexpr uint32_t kFaceBlend      = 0x0002;  // beim Laden aus der Vertex-Intensitaet
inline constexpr uint32_t kFaceEnvMap     = 0x0008;  // von der -10.0-Marke
inline constexpr uint32_t kFaceAdditive   = 0x0010;
inline constexpr uint32_t kFaceSentinel20 = 0x0090;  // von der -20.0-Marke (scrollende UVs)

struct Vec3 {
    float x = 0.0f, y = 0.0f, z = 0.0f;
};

struct Cel {
    uint32_t             width = 0, height = 0;
    uint32_t             colorKeyEnable = 0, numMips = 0;
    std::vector<uint8_t> pixels;
    std::vector<uint8_t> raw;      // der 0x18-Satz, unveraendert

    bool Keyed() const { return (colorKeyEnable & 1u) != 0; }
};

struct Material {
    std::string          name;
    uint32_t             matType = 0, bitDepth = 0;
    std::vector<Cel>     cels;
    std::vector<uint8_t> ownPalette;   // leer, wenn kMatOwnPalette nicht gesetzt ist
    std::vector<uint8_t> rawName;      // das 0x20-Namensfeld
    std::vector<uint8_t> rawHeader;    // der 0x4c-Kopf
    std::vector<uint8_t> rawRecords;   // die Saetze dahinter, unangetastet

    bool Textured() const { return (matType & kMatTextured) != 0; }
};

struct Face {
    uint32_t material = 0, flags = 0;
    uint32_t geometryMode = 0, lightMode = 0, textureMode = 0;
    float    extraLight = 0.0f;
    float    v0 = 0.0f;              // wird beim Laden ueberschrieben, nie als Alpha lesen
    Vec3     normal;
    std::vector<uint32_t> vertIdx;
    std::vector<uint32_t> texIdx;

    // Ergebnis der beiden Ladedurchgaenge (siehe ApplyLoadPasses)
    uint32_t flagsLoaded = 0, lightModeLoaded = 0;
    float    v0Loaded = 0.0f;

    std::vector<uint8_t> raw;      // der 0x4c-Satz
};

struct Mesh {
    std::string name;
    uint32_t    geometryMode = 0;    // 0 = wird nie gezeichnet
    uint32_t    lightingMode = 0, textureMode = 0;
    std::vector<Vec3>  positions;
    std::vector<Vec3>  normals;
    std::vector<float> intensities;
    std::vector<std::array<float, 2>> texVerts;
    std::vector<Face>  faces;

    // Die Huellkugel, die die Engine in der Datei mitliefert.
    // Der AI-Blickrichtungs-Leser transformiert sie, er rechnet
    // sie nicht nach. Beim Kopf falsch = das bekannte Einfrieren.
    float radius = 0.0f;
    Vec3  center;

    std::vector<uint8_t> raw;      // der 0x70-Satz
    // vertCount * 4 Bytes, die nie beschrieben werden. Sie stehen
    // trotzdem in der Datei und gehoeren zur Byte-Gleichheit.
    std::vector<uint8_t> dead;

    bool Drawn() const { return geometryMode != 0; }
};

struct Node {
    std::string name;
    uint32_t    flags = 0;
    int32_t     num = 0;
    uint32_t    type = 0;            // Koerperteil-Bit, wird gegen clip.typeMask geANDet
    int32_t     meshIndex = -1;      // -1 = keine Geometrie
    int32_t     parent = -1;
    int32_t     numChildren = 0, child = -1, sibling = -1;
    Vec3        pivot;
    Vec3        bindPos;
    Vec3        bindEuler;           // Grad: pitch, yaw, roll

    std::vector<uint8_t> raw;      // der 0xb4-Satz
};

struct Key {
    float    frame = 0.0f;
    uint32_t flags = 0;
    Vec3     pos, orient;
    Vec3     dpos, dorient;          // Steigungen, nicht der naechste Wert

    std::vector<uint8_t> raw;      // der 0x38-Satz
};

struct Track {
    std::string name;
    int32_t     jointIndex = -1;
    std::vector<Key> keys;

    std::vector<uint8_t> raw;      // der 0x2c-Kopf
};

struct SoundEvent {
    uint32_t    frame = 0, typeBits = 0;
    std::string wav;
    uint32_t    callFlags = 0;
    float       volume = 0.0f, rangeU = 0.0f, minDistU = 0.0f, maxDistU = 0.0f;
    uint32_t    priority = 0;
    int32_t     soundId = 0;

    std::vector<uint8_t> raw;      // der 0x60-Satz
};

struct Clip {
    std::string name;
    float       fadeSeconds = 0.0f;
    uint32_t    var1Loop = 0, var2Frame = 0, trackFlags = 0, specialUsage = 0;
    uint32_t    typeMask = 0;
    float       fps = 0.0f;
    int32_t     numFrames = 0;
    std::vector<Track>      tracks;
    std::vector<SoundEvent> events;

    std::vector<uint8_t> rawDesc;    // der 0x40-Descriptor
    std::vector<uint8_t> rawKeyHdr;  // der 0x84-Key-Header
};

struct BafFile {
    std::string source;              // Dateiname, nur fuer Meldungen

    std::string name;                // hdr+0x08
    float       scale = 0.0f;        // hdr+0xac
    float       radiusU = 0.0f;      // hdr+0xec
    float       heightU = 0.0f;      // hdr+0xf4
    std::array<uint8_t, 4> nodeRefs{};// hdr+0xf0: Huefte, Nacken, Waffe, Zweitwaffe

    std::vector<uint8_t> palette;    // 768 Bytes oder leer

    // ---- Rohbloecke fuer den Rueckweg ----
    //
    // Der Schreiber baut die Datei nicht neu, sondern PATCHT die
    // dekodierten Felder in die urspruenglichen Bytes zurueck. Nur
    // so ueberlebt jedes undurchsichtige Byte - alte Zeiger, die
    // Fuellbytes des Compilers von 1999, der Muell hinter einer
    // NUL im Namensfeld. Ohne das waere schon ein Import-Export
    // ohne jede Aenderung nicht mehr dieselbe Datei.
    std::vector<uint8_t> rawHeader;       // 0x120
    std::vector<uint8_t> paletteBlock;    // der GANZE Palettenblock
    std::vector<uint8_t> rawGeoHeader;    // 0x84

    std::vector<Material> materials;
    std::vector<Mesh>     meshes;
    std::vector<Node>     nodes;
    std::vector<Clip>     clips;

    // Welche Palette gilt: die eigene des Materials, sonst die des Modells.
    const std::vector<uint8_t>* MaterialPalette(size_t materialIndex) const;
    // Der Knoten, dem ein Mesh gehoert. Jedes Mesh gehoert genau einem.
    const Node* NodeOfMesh(int32_t meshIndex) const;
    // Die Cel-Groesse, durch die die UVs dieses Materials geteilt werden.
    // UVs sind Texel - Leser und Schreiber muessen sich auf dieselbe Zahl
    // einigen, sonst rutscht die Textur.
    void MaterialCelSize(uint32_t materialIndex, uint32_t& width, uint32_t& height) const;
};

// ------------------------------------------------------------
//  Texturen
//
//  Ein Cel ist 8 Bit palettisiert. Aufgeloest wird gegen die
//  eigene Palette des Materials, sonst gegen die des Modells.
//
//  Index 0 ist NUR dann durchsichtig, wenn das Key-Bit des Cels
//  es sagt. Ihn bedingungslos auszustanzen reisst Loecher in
//  hunderte Texturen.
//
//  Die Zeilen kommen in der Reihenfolge der Datei heraus, also
//  von oben nach unten. Das Umdrehen macht erst der TGA-Schreiber.
// ------------------------------------------------------------
struct DecodedCel {
    uint32_t             width = 0, height = 0;
    bool                 keyed = false;
    std::vector<uint8_t> rgba;    // width * height * 4, Zeile 0 = oben
};

bool DecodeCel(const BafFile& file, size_t materialIndex, size_t celIndex,
               DecodedCel& out);

// Schreibt ein DecodedCel als unkomprimiertes 32-Bit-TGA.
//
// TGA statt PNG mit Absicht: PNG braucht Deflate, und dafuer
// entweder zlib als Abhaengigkeit oder eine eigene Umsetzung -
// beides fuer ein Bild, das 3ds Max ohnehin nativ liest. TGA
// speichert von unten nach oben, deshalb dreht der Schreiber die
// Zeilen um.
bool WriteTga(const DecodedCel& cel, const std::string& path, std::string& error);

// Ein Name aus der Datei, der sich gefahrlos an einen Ordner
// haengen laesst.
//
// Ein Materialname kommt aus einer Spieldatei und darf alles
// enthalten - auch "..\" oder einen Doppelpunkt. Ungeprueft an
// einen Pfad gehaengt schreibt der Export dann irgendwohin.
std::string SafeFileName(const std::string& name, const std::string& fallback);

// ------------------------------------------------------------
//  Werkzeuge fuer den Umbau der Topologie
// ------------------------------------------------------------

// Die Huellkugel, die die Engine im Mesh-Satz mitliefert:
// Mittelpunkt ist die Mitte des achsenparallelen Kastens, Radius
// der Abstand zum entferntesten Vertex. Positionen sind
// knotenlokal, also geht weder Pivot noch Faktor ein.
void ComputeBounds(const std::vector<Vec3>& positions, float& radius, Vec3& center);

// ------------------------------------------------------------
//  Der Knoten, auf den die Blickrichtung der KI zielt.
//
//  Steht als Nackenverweis im Kopf (hdr+0xf1). -1, wenn er nicht
//  gesetzt oder ausserhalb ist.
//
//  Warum das hier steht: dieses Mesh umzubauen ist der EINE Fall,
//  der das bekannte Einfrieren ausloest. Der Leser der
//  Blickrichtung transformiert die mitgelieferte Huellkugel, er
//  rechnet sie nicht nach - eine falsche Kugel am Kopf, und die
//  KI bleibt haengen. Deshalb bleibt seine Kugel beim Umbau
//  standardmaessig unangetastet.
// ------------------------------------------------------------
int32_t HeadNodeIndex(const BafFile& file);

// ------------------------------------------------------------
//  Der Texturvertex-Vorrat eines Mesh.
//
//  Beim Umbau entsteht er neu, Ecke fuer Ecke. Gleiche Texel
//  bekommen denselben Index - auf 6 Nachkommastellen verglichen,
//  wie im Blender-Addon. Ohne das Zusammenfassen waere der Vorrat
//  so gross wie die Zahl der Ecken und riebe schnell an der
//  Grenze von 0x300.
// ------------------------------------------------------------
class TexVertPool {
public:
    uint32_t IndexOf(float u, float v);
    const std::vector<std::array<float, 2>>& TexVerts() const { return texVerts_; }

private:
    std::vector<std::array<float, 2>> texVerts_;
};

// Normalisieren mit Ersatzwert.
//
// Eine Normale der Laenge null ist kein Schoenheitsfehler: die
// Engine teilt durch die Laenge, und das NaN daraus landet als
// Zeigerindex im Speicher. Ein Vertex, den kein Face beruehrt,
// liefert genau so eine Normale.
Vec3 NormalizeOrUp(const Vec3& normal);

// ------------------------------------------------------------
//  Schreiben
//
//  Baut aus einem BafFile wieder eine .baf. Alle abhaengigen
//  Groessen und Offsets werden neu gerechnet, alles andere aus
//  den Rohbloecken uebernommen.
//
//  Zusicherung: wurde nichts geaendert, ist das Ergebnis
//  BYTE-GLEICH mit der Eingabe. Das ist die schaerfste Pruefung,
//  die es fuer einen Schreiber gibt, und sie laeuft ueber alle
//  303 Modelle des Archivs.
// ------------------------------------------------------------
bool BuildBaf(const BafFile& file, std::vector<uint8_t>& out, std::string& error);

// Zum Vergleichen zweier Fassungen: die erste abweichende
// Byteposition, oder SIZE_MAX wenn beide gleich sind.
size_t FirstDifference(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b);

struct ReadResult {
    bool        ok = false;
    std::string error;      // leer, wenn ok
    std::string warnings;   // nicht-fatale Hinweise, "\n"-getrennt
};

// Liest eine .baf vollstaendig aus einem Puffer.
ReadResult ReadBafBuffer(const uint8_t* data, size_t size, const std::string& source,
                         BafFile& out);
// Bequemlichkeit: dieselbe Funktion auf einer losen Datei.
ReadResult ReadBafFile(const std::string& path, BafFile& out);

// Die beiden Ladedurchgaenge der Engine ueber die Faces eines Mesh.
// Wird von ReadBafBuffer selbst aufgerufen; oeffentlich, damit ein
// neu gebautes Mesh dieselbe Behandlung bekommen kann.
void ApplyLoadPasses(Mesh& mesh, const std::vector<Material>& materials);

// Die Renderklasse eines Faces, gelesen NACH beiden Durchgaengen.
struct RenderClass {
    enum Blend { Opaque, Translucent, Additive };
    Blend blend = Opaque;
    float alpha = 1.0f;
    bool  twoSided = false, envMap = false, fullbright = false;
};
RenderClass FaceRenderClass(const Face& face, float thingAlpha = 1.0f);

// ------------------------------------------------------------
//  Eine 3x4-Matrix.
//
//  m[0] = right, m[1] = forward, m[2] = up, m[3] = Translation.
//
//  ACHTUNG, das ist die Stelle, an der sich die Konventionen
//  kreuzen: das Blender-Addon setzt right/forward/up als SPALTEN
//  ein (Spaltenvektor-Konvention). 3ds Max rechnet mit
//  Zeilenvektoren, dort sind es ZEILEN. Diese Anordnung ist also
//  schon die Transponierte der Blender-Matrix und laesst sich
//  ohne Umsortieren an Matrix3::SetRow durchreichen.
//
//  In double, nicht float: die Weltmatrizen entstehen durch
//  Verkettung entlang der Knotenkette, und bei 48 Knoten
//  Kettenlaenge sammelt sich in float sichtbarer Fehler an. Die
//  Python-Referenz rechnet aus demselben Grund in double.
// ------------------------------------------------------------
struct Matrix34 {
    double m[4][3];
};

// Die lokale Matrix eines Knotens: Basis aus bind_euler,
// Translation aus bind_pos * scale.
Matrix34 NodeLocalMatrix(const Node& node, double scale);

// Verkettung in der Konvention der Engine: erst local, dann parent.
Matrix34 Concat(const Matrix34& parent, const Matrix34& local);

// Die Weltmatrizen aller Knoten in Ruhelage. Ein Elternteil steht
// in der Datei immer VOR seinem Kind, deshalb genuegt ein
// Durchgang. Ein Vorwaertsverweis wird als Wurzel behandelt -
// genau wie im Blender-Addon.
void ComputeRestWorld(const BafFile& file, double scale,
                      std::vector<Matrix34>& out);

// ------------------------------------------------------------
//  Aufbereitete Geometrie eines Mesh.
//
//  Zwischen Datei und 3ds Max liegt mehr, als es aussieht:
//
//  * Der Pivot-Versatz:  v_lokal = v_roh + node.pivot
//  * Der Laengenfaktor
//  * N-Gons. Die Datei kennt Faces bis 12 Ecken, Max' Mesh nur
//    Dreiecke. Faechertriangulierung ab Ecke 0.
//  * UVs sind TEXEL, keine 0..1-Koordinaten. Geteilt wird durch
//    die Groesse von Cel 0 des Materials, das dieses FACE nennt -
//    und die kann innerhalb eines Mesh wechseln. Deshalb liegen
//    die UVs hier je Ecke und nicht je Vertex.
//  * Entartete Faces (ein Vertex doppelt) werden uebersprungen
//    und ihre Nummern gemerkt, damit ein spaeterer Export sie
//    unveraendert wieder herausschreiben kann.
//
//  Das alles steht bewusst hier und nicht in der Max-Schicht:
//  hier laesst es sich gegen die Python-Referenz halten, bevor
//  3ds Max ueberhaupt startet.
// ------------------------------------------------------------
struct BuiltTri {
    uint32_t vertex[3] = {0, 0, 0};
    float    uv[3][2]  = {{0.0f, 0.0f}, {0.0f, 0.0f}, {0.0f, 0.0f}};
    uint32_t material  = 0;
    uint32_t slot      = 0;    // Platz im Multi/Sub-Object, siehe MaterialSlot
    uint32_t sourceFace = 0;   // das Face der Datei, aus dem dieses Dreieck stammt
};

// ------------------------------------------------------------
//  Ein Materialplatz.
//
//  Geschluesselt ueber (Material, Blendmodus, zweiseitig,
//  vollhell) - NICHT ueber den Materialindex allein. Ein
//  Engine-Material traegt genau einen Blendmodus und ein
//  Zweiseitig-Bit, und diese Angaben stehen am FACE, nicht am
//  Material. Nach dem Index allein gruppiert kollidieren sie.
//
//  Das Alpha gehoert bewusst NICHT in den Schluessel: es kommt
//  aus der Vertex-Intensitaet und ist praktisch stufenlos, ein
//  Schluessel darueber ergaebe fast so viele Plaetze wie Faces.
//  Uebernommen wird das Alpha des ersten Faces dieses Platzes -
//  so macht es das Blender-Addon auch.
// ------------------------------------------------------------
struct MaterialSlot {
    uint32_t material   = 0;
    int      blend      = 0;   // RenderClass::Blend
    bool     twoSided   = false;
    bool     fullbright = false;
    bool     envMap     = false;
    float    alpha      = 1.0f;
};

// Die Materialplaetze einer Datei, in der Reihenfolge ihres
// ersten Auftretens. Deterministisch, damit die Nummern zwischen
// zwei Laeufen dieselben bleiben.
void BuildMaterialSlots(const BafFile& file, std::vector<MaterialSlot>& slots);

// Der Platz eines Faces in dieser Tabelle, oder 0.
uint32_t SlotOfFace(const std::vector<MaterialSlot>& slots, const Face& face);

struct BuiltMesh {
    std::string name;
    int32_t     meshIndex = -1;
    int32_t     nodeIndex = -1;      // Besitzerknoten, -1 = keiner gefunden
    bool        drawn = true;        // geometryMode != 0
    std::vector<Vec3>     positions; // schon mit Pivot und Faktor
    std::vector<Vec3>     normals;
    std::vector<BuiltTri> tris;
    std::vector<uint32_t> skippedFaces;
};

// Bereitet ein Mesh auf. meshIndex muss gueltig sein.
// slots darf leer sein; dann bleibt tri.slot auf 0.
void BuildMeshGeometry(const BafFile& file, size_t meshIndex, double scale,
                       const std::vector<MaterialSlot>& slots, BuiltMesh& out);

// ------------------------------------------------------------
//  Rohdaten fuer den Rueckweg.
//
//  Alles, was beim Import sonst verlorengeht: die Wortfelder je
//  Face, die Intensitaet je Vertex, die uebersprungenen Faces,
//  die Zuordnung Dreieck -> Quellface, die Kopfangaben, die
//  Clip-Metadaten.
//
//  Der Punkt daran: diese Angaben lassen sich SPAETER NICHT
//  REKONSTRUIEREN. Wer heute importiert und in einem halben Jahr
//  exportieren will, hat sie sonst nicht mehr. Sie kosten beim
//  Import fast nichts und sind hinterher nicht nachzuruesten.
//
//  Das Format ist "Schluessel = Wert" je Zeile - genau das, was
//  in 3ds Max unter Objekteigenschaften -> Benutzerdefiniert
//  steht und sich von Hand ansehen laesst.
// ------------------------------------------------------------
std::string ModelPropText(const BafFile& file);
std::string NodePropText(const BafFile& file, size_t nodeIndex);
std::string MeshPropText(const BafFile& file, size_t meshIndex, const BuiltMesh& built);
std::string ClipPropText(const BafFile& file, size_t clipIndex);

// ------------------------------------------------------------
//  Die Grenzen der Engine.
//
//  Der Loader lehnt ein Modell ab, das darueber liegt - beim
//  Modden merkt man das sonst erst, wenn das Spiel das Modell
//  nicht mehr annimmt. Die Zahlen stammen aus dem Retail-Loader.
// ------------------------------------------------------------
inline constexpr uint32_t kMaxModelNodes   = 48;
inline constexpr uint32_t kMaxMeshVerts    = 0x200;
inline constexpr uint32_t kMaxMeshFaces    = 0x200;
inline constexpr uint32_t kMaxMeshTexVerts = 0x300;
inline constexpr uint32_t kMaxFaceVerts    = 0x18;   // 24

// Ein Bericht ueber die Grenzen. Leer, wenn alles darunter liegt.
// exceeded zaehlt die echten Ueberschreitungen; text ist die
// Ausgabe fuer die Oberflaeche, immer gefuellt.
struct LimitReport {
    int         exceeded = 0;
    std::string text;
};
void CheckLimits(const BafFile& file, LimitReport& out);


// Die Rotationsbasis der Engine.
// (pitch, yaw, roll) drehen um (X = rechts, Z = oben, Y = vorne);
// v' = v.x*right + v.y*forward + v.z*up.
// Gegen die vorgebackene Ruhematrix der Engine (Knoten+0x84) in
// 1516 von 1516 Knoten bestaetigt, wo das Feld echt gefuellt ist.
void EulerBasis(float pitchDeg, float yawDeg, float rollDeg,
                Vec3& right, Vec3& forward, Vec3& up);
// Die Umkehrung: aus einer Basis wieder (pitch, yaw, roll) in
// Grad. Braucht der Export, um eine in Max gedrehte Lage zurueck
// in die Darstellung der Datei zu bringen.
//
// Achtung Kardanblockade: steht die Blickachse senkrecht, sind
// Gierung und Rollen nicht mehr zu trennen. Dann wird das Rollen
// in die Gierung gefaltet - dieselbe Drehung, andere Schreibweise.
void BasisToEuler(const double right[3], const double forward[3], const double up[3],
                  double& pitchDeg, double& yawDeg, double& rollDeg);

// Dieselbe Basis in double. Die float-Fassung oben ruft sie auf.
void EulerBasisD(double pitchDeg, double yawDeg, double rollDeg,
                 double right[3], double forward[3], double up[3]);

// Der Cursor der Engine: der LETZTE Key mit frame <= t, nicht der
// naechstgelegene. Genau das macht einen Key ohne sein
// Interpolationsbit bis zum naechsten haltend.
size_t CursorAt(const Track& track, float frame);

// Die lokale Matrix eines Knotens zu einem Zeitpunkt eines Clips.
//
// Das ist der Wert, der in 3ds Max direkt in den Positions- und
// Rotationscontroller geht: ein PRS-Controller haelt die Lage
// RELATIV ZUM ELTERNTEIL, und genau das liefert ein Key auch.
// Der Umweg ueber die Ruhelage, den das Blender-Addon gehen muss
// (dort ist der Bone-Raum ein anderer), entfaellt hier.
Matrix34 SampleLocalMatrix(const Track& track, double frame, double scale);
// (pos, orient) an einem Frame, nach der Regel des Compositors.
void SampleTrack(const Track& track, float frame, Vec3& pos, Vec3& orient);

// ------------------------------------------------------------
//  Text-Dump zum Gegenpruefen gegen die Python-Referenz.
//  Zeilenweise und deterministisch, damit ein schlichtes diff
//  genuegt. Gleitkommazahlen mit std::to_chars und fester
//  Nachkommastelle - das ist locale-fest, ein deutsches Max
//  wuerde sonst Dezimalkommata schreiben.
// ------------------------------------------------------------
enum DumpDetail : uint32_t {
    kDumpCounts   = 0,
    kDumpVertices = 1u << 0,   // jeder Vertex, jedes Face einzeln
    kDumpKeys     = 1u << 1,   // jeder Keyframe einzeln
    kDumpRest     = 1u << 2,   // die Weltmatrizen der Ruhelage
    kDumpBuild    = 1u << 3,   // die aufbereitete Geometrie
    kDumpAnim     = 1u << 4,   // die abgetasteten lokalen Matrizen
    kDumpProps    = 1u << 5,   // die Rohdaten fuer den Rueckweg, plus Grenzen
    kDumpAll      = kDumpVertices | kDumpKeys | kDumpRest | kDumpBuild | kDumpAnim
                  | kDumpProps
};
void WriteDump(const BafFile& file, const std::string& label, std::ostream& out,
               uint32_t detail, double scale = 1.0);

// Nur die Ruhelage-Zeilen, ohne alles andere. Damit laesst sich das
// aus 3ds Max ZURUECKGELESENE Skelett gegen die Referenz halten:
// beide Seiten schreiben dieselben Zeilen, ein diff genuegt.
void WriteRestDump(const std::vector<Matrix34>& world,
                   const std::vector<std::string>& names,
                   std::ostream& out);

// Nur die Animationszeilen eines Clips. Wie bei den anderen
// Dumps: damit laesst sich das aus 3ds Max zurueckgelesene
// Ergebnis gegen die Referenz halten.
void WriteAnimDump(const BafFile& file, size_t clipIndex, double scale,
                   std::ostream& out);

// Nur die Geometriezeilen eines Mesh. Damit laesst sich das aus
// 3ds Max ZURUECKGELESENE Mesh gegen die Referenz halten.
void WriteBuildDump(const BuiltMesh& built, std::ostream& out);
std::string MakeSummary(const BafFile& file);

// Locale-feste Zahlenausgabe, auch ausserhalb des Dumps brauchbar.
// Nimmt double: ein float-Argument wird dabei verlustfrei
// heraufgestuft, die Ausgabe fuer float-Werte aendert sich also
// nicht.
std::string FormatFloat(double value, int decimals = 6);

} // namespace baf
