#include "baf_reader.h"
#include "baf_lab.h"   // OpenBinaryRead: UTF-8-Pfade auch unter Windows

#include <cmath>
#include <cstdio>
#include <cstring>
#include <ostream>
#include <stdexcept>

namespace baf {
namespace {

constexpr float kSentinel10 = -10.0f;
constexpr float kSentinel11 = -11.0f;
constexpr float kSentinel20 = -20.0f;

// ------------------------------------------------------------
//  Der Cursor.
//
//  Jeder einzelne Lesezugriff geht durch Need() und damit durch
//  eine Bereichspruefung. Es gibt in dieser Datei bewusst KEINE
//  Zeigerarithmetik ausserhalb dieser Klasse und keinen
//  reinterpret_cast auf einen Zahlentyp - gelesen wird immer per
//  memcpy, sonst waeren Alignment und Aliasing formal undefiniert.
//
//  Intern wird geworfen, nach aussen nie: ReadBafBuffer faengt
//  alles und gibt ein ReadResult zurueck. Eine Exception, die aus
//  einem Plugin in den Max-Prozess laeuft, ist ein Absturz.
// ------------------------------------------------------------
class ParseError : public std::runtime_error {
public:
    explicit ParseError(const std::string& text) : std::runtime_error(text) {}
};

class Cursor {
public:
    Cursor(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    size_t Tell() const { return pos_; }
    size_t Size() const { return size_; }

    void Seek(size_t position) {
        if (position > size_) Fail(position, 0);
        pos_ = position;
    }
    void Skip(size_t count) { Need(count); pos_ += count; }

    uint32_t U32()  { uint32_t v; Take(&v, sizeof v); return v; }
    int32_t  I32()  { int32_t  v; Take(&v, sizeof v); return v; }
    float    F32()  { float    v; Take(&v, sizeof v); return v; }
    Vec3     V3()   { Vec3 v; v.x = F32(); v.y = F32(); v.z = F32(); return v; }

    // Feste Feldbreite, NUL-terminiert. Der Cursor rueckt um die
    // ganze Breite weiter, nicht nur um die Textlaenge.
    std::string CStr(size_t fieldWidth) {
        Need(fieldWidth);
        const char* start = reinterpret_cast<const char*>(data_ + pos_);
        const size_t length = ::strnlen(start, fieldWidth);
        pos_ += fieldWidth;
        return std::string(start, length);
    }

    void Bytes(std::vector<uint8_t>& out, size_t count) {
        Need(count);
        out.assign(data_ + pos_, data_ + pos_ + count);
        pos_ += count;
    }

    // Lesen an absoluter Stelle, ohne den Cursor zu bewegen.
    uint32_t U32At(size_t offset) const {
        if (offset + sizeof(uint32_t) > size_) Fail(offset, sizeof(uint32_t));
        uint32_t value;
        std::memcpy(&value, data_ + offset, sizeof value);
        return value;
    }
    int32_t I32At(size_t offset) const {
        return static_cast<int32_t>(U32At(offset));
    }
    float F32At(size_t offset) const {
        const uint32_t bits = U32At(offset);
        float value;
        std::memcpy(&value, &bits, sizeof value);
        return value;
    }
    Vec3 V3At(size_t offset) const {
        Vec3 v; v.x = F32At(offset); v.y = F32At(offset + 4); v.z = F32At(offset + 8);
        return v;
    }
    std::string CStrAt(size_t offset, size_t fieldWidth) const {
        if (offset + fieldWidth > size_) Fail(offset, fieldWidth);
        const char* start = reinterpret_cast<const char*>(data_ + offset);
        return std::string(start, ::strnlen(start, fieldWidth));
    }
    // Ein Stueck Rohdaten an absoluter Stelle. Fuer den Rueckweg:
    // der Schreiber patcht die dekodierten Felder da wieder hinein.
    std::vector<uint8_t> RawAt(size_t offset, size_t count) const {
        if (count > size_ || offset > size_ - count) Fail(offset, count);
        return std::vector<uint8_t>(data_ + offset, data_ + offset + count);
    }
    uint8_t U8At(size_t offset) const {
        if (offset >= size_) Fail(offset, 1);
        return data_[offset];
    }

private:
    void Need(size_t count) const {
        if (count > size_ || pos_ > size_ - count) Fail(pos_, count);
    }
    template <typename T> void Take(T* out, size_t count) {
        Need(count);
        std::memcpy(out, data_ + pos_, count);
        pos_ += count;
    }
    [[noreturn]] void Fail(size_t at, size_t count) const {
        char text[160];
        std::snprintf(text, sizeof text,
                      "Lesen ueber das Dateiende hinaus: %zu Byte(s) an Offset %zu, "
                      "die Datei hat %zu", count, at, size_);
        throw ParseError(text);
    }

    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    size_t pos_  = 0;
};

// Die Groesse eines Cel-Pixelblocks inklusive Mipmaps.
// Als uint64_t gerechnet: width und height kommen aus der Datei,
// und width*height*mips in 32 Bit koennte ueberlaufen und dann
// eine viel zu kleine, plausibel aussehende Zahl ergeben.
uint64_t CelPixelBytes(uint32_t width, uint32_t height, uint32_t numMips, uint32_t bitDepth) {
    const uint32_t levels = (numMips > 0) ? numMips : 1u;
    const uint64_t bytesPerPixel = bitDepth / 8u;
    uint64_t total = 0;
    for (uint32_t level = 0; level < levels; ++level) {
        const uint64_t w = (width  >> level) ? (width  >> level) : 1u;
        const uint64_t h = (height >> level) ? (height >> level) : 1u;
        total += w * h * bytesPerPixel;
    }
    return total;
}

} // namespace

// ============================================================
//  BafFile - kleine Helfer
// ============================================================

const std::vector<uint8_t>* BafFile::MaterialPalette(size_t materialIndex) const {
    if (materialIndex < materials.size() && !materials[materialIndex].ownPalette.empty()) {
        return &materials[materialIndex].ownPalette;
    }
    return palette.empty() ? nullptr : &palette;
}

const Node* BafFile::NodeOfMesh(int32_t meshIndex) const {
    for (const Node& node : nodes) {
        if (node.meshIndex == meshIndex) return &node;
    }
    return nullptr;
}

void BafFile::MaterialCelSize(uint32_t materialIndex, uint32_t& width, uint32_t& height) const {
    width = 256; height = 256;
    if (materialIndex < materials.size()) {
        const std::vector<Cel>& cels = materials[materialIndex].cels;
        if (!cels.empty() && cels[0].width && cels[0].height) {
            width = cels[0].width; height = cels[0].height;
        }
    }
}

// ============================================================
//  Die beiden Ladedurchgaenge
// ============================================================

void ApplyLoadPasses(Mesh& mesh, const std::vector<Material>& materials) {
    if (mesh.faces.empty()) return;

    // Durchgang 1, die extraLight-Marken. Face 0 entscheidet fuer
    // das GANZE Mesh: die beiden Vergleiche stehen im Loader vor
    // dem Schleifenkopf. Wer sie pro Face auswertet, macht ein
    // Face von 64 additiv statt alle 64.
    const float firstExtraLight = mesh.faces[0].extraLight;
    const bool sentinel10 = (firstExtraLight == kSentinel10);
    const bool sentinel20 = (firstExtraLight == kSentinel20);

    for (Face& face : mesh.faces) {
        uint32_t flags = face.flags;
        uint32_t lightMode = face.lightMode;

        if (sentinel10 && face.extraLight != kSentinel11) flags |= kFaceEnvMap;
        if (sentinel20) flags |= kFaceSentinel20;

        // Durchgang 2, das Alpha pro Face:
        // face.v0 := |intensities[vertIdx[0]]|
        float v0 = 0.0f;
        if (!face.vertIdx.empty() && !mesh.intensities.empty() &&
            face.vertIdx[0] < mesh.intensities.size()) {
            v0 = mesh.intensities[face.vertIdx[0]];
            if (v0 != 0.0f) {
                flags |= kFaceBlend;
                if (v0 < 0.0f) { flags |= kFaceAdditive; v0 = -v0; }
                lightMode = 0;                      // das Face ist vollhell
            }
        }
        if (face.material < materials.size()) {
            const uint32_t matType = materials[face.material].matType;
            if (matType & kMatBlend) {
                flags |= kFaceBlend;
                if (matType & kMatAdditive) flags |= kFaceAdditive;
            }
        }
        face.flagsLoaded = flags;
        face.lightModeLoaded = lightMode;
        face.v0Loaded = v0;
    }
}

RenderClass FaceRenderClass(const Face& face, float thingAlpha) {
    RenderClass result;
    const float v0 = face.v0Loaded;
    const float base = (v0 > 0.0f && v0 < 1.0f) ? v0 : 1.0f;
    const int alpha255 = static_cast<int>(base * thingAlpha * 255.0f);

    if (alpha255 == 255)                            result.blend = RenderClass::Opaque;
    else if (face.flagsLoaded & kFaceAdditive)      result.blend = RenderClass::Additive;
    else                                            result.blend = RenderClass::Translucent;

    result.alpha      = alpha255 / 255.0f;
    result.twoSided   = (face.flagsLoaded & kFaceTwoSided) != 0;
    result.envMap     = (face.flagsLoaded & kFaceEnvMap) != 0;
    result.fullbright = (face.lightModeLoaded == 0);
    return result;
}

// ============================================================
//  Rotationsbasis und Track-Auswertung
// ============================================================

namespace {

// Kommagetrennte Liste. Fuer Face-Arrays: kompakt und von Hand
// lesbar, was bei einem Feld in den Objekteigenschaften zaehlt.
template <typename Fn>
std::string JoinList(size_t count, Fn value) {
    std::string out;
    for (size_t i = 0; i < count; ++i) {
        if (i) out += ",";
        out += value(i);
    }
    return out;
}

std::string Vec3Text(const Vec3& v) {
    return FormatFloat(v.x) + " " + FormatFloat(v.y) + " " + FormatFloat(v.z);
}

} // namespace

std::string ModelPropText(const BafFile& file) {
    std::string out;
    out += "bafModelName = " + file.name + "\n";
    out += "bafHeaderScale = " + FormatFloat(file.scale) + "\n";
    out += "bafRadiusU = " + FormatFloat(file.radiusU) + "\n";
    out += "bafHeightU = " + FormatFloat(file.heightU) + "\n";
    out += "bafNodeRefs = " + std::to_string(file.nodeRefs[0]) + "," +
           std::to_string(file.nodeRefs[1]) + "," + std::to_string(file.nodeRefs[2]) +
           "," + std::to_string(file.nodeRefs[3]) + "\n";
    out += "bafClipCount = " + std::to_string(file.clips.size()) + "\n";
    return out;
}

std::string NodePropText(const BafFile& file, size_t nodeIndex) {
    if (nodeIndex >= file.nodes.size()) return std::string();
    const Node& node = file.nodes[nodeIndex];
    std::string out;
    out += "bafNodeIndex = " + std::to_string(nodeIndex) + "\n";
    out += "bafNodeName = " + node.name + "\n";
    // Das Koerperteil-Bit. Es wird gegen die typeMask eines Clips
    // geANDet und entscheidet, ob der Clip diesen Knoten anfasst.
    out += "bafNodeType = " + std::to_string(node.type) + "\n";
    out += "bafNodeFlags = " + std::to_string(node.flags) + "\n";
    out += "bafMeshIndex = " + std::to_string(node.meshIndex) + "\n";
    out += "bafParent = " + std::to_string(node.parent) + "\n";
    // Der Pivot geht in die Vertexpositionen ein und ist danach
    // aus der Szene nicht mehr herauszulesen.
    out += "bafPivot = " + Vec3Text(node.pivot) + "\n";
    out += "bafBindPos = " + Vec3Text(node.bindPos) + "\n";
    out += "bafBindEuler = " + Vec3Text(node.bindEuler) + "\n";
    return out;
}

std::string MeshPropText(const BafFile& file, size_t meshIndex, const BuiltMesh& built) {
    if (meshIndex >= file.meshes.size()) return std::string();
    const Mesh& mesh = file.meshes[meshIndex];
    std::string out;
    out += "bafMeshIndex = " + std::to_string(meshIndex) + "\n";
    out += "bafMeshName = " + mesh.name + "\n";
    out += "bafGeometryMode = " + std::to_string(mesh.geometryMode) + "\n";
    out += "bafLightingMode = " + std::to_string(mesh.lightingMode) + "\n";
    out += "bafTextureMode = " + std::to_string(mesh.textureMode) + "\n";
    // Die Huellkugel, die die Engine mitliefert. Beim Kopf ist das
    // die Kugel, die der Blickrichtungs-Leser der KI transformiert -
    // sie wird nicht nachgerechnet.
    out += "bafBoundsRadius = " + FormatFloat(mesh.radius) + "\n";
    out += "bafBoundsCenter = " + Vec3Text(mesh.center) + "\n";

    // Zuordnung Dreieck -> Quellface. Nach der Triangulierung ist
    // sie sonst verloren.
    out += "bafFaceMap = " + JoinList(built.tris.size(), [&](size_t i) {
        return std::to_string(built.tris[i].sourceFace);
    }) + "\n";
    out += "bafSkippedFaces = " + JoinList(built.skippedFaces.size(), [&](size_t i) {
        return std::to_string(built.skippedFaces[i]);
    }) + "\n";

    // Die Wortfelder je Face, in ROHFORM - also vor den beiden
    // Ladedurchgaengen. Ein Export muss sie unveraendert wieder
    // herausschreiben, nicht das Ergebnis der Durchgaenge.
    out += "bafFaceFlags = " + JoinList(mesh.faces.size(), [&](size_t i) {
        return std::to_string(mesh.faces[i].flags);
    }) + "\n";
    out += "bafFaceLightMode = " + JoinList(mesh.faces.size(), [&](size_t i) {
        return std::to_string(mesh.faces[i].lightMode);
    }) + "\n";
    out += "bafFaceGeoMode = " + JoinList(mesh.faces.size(), [&](size_t i) {
        return std::to_string(mesh.faces[i].geometryMode);
    }) + "\n";
    out += "bafFaceTexMode = " + JoinList(mesh.faces.size(), [&](size_t i) {
        return std::to_string(mesh.faces[i].textureMode);
    }) + "\n";
    out += "bafFaceExtraLight = " + JoinList(mesh.faces.size(), [&](size_t i) {
        return FormatFloat(mesh.faces[i].extraLight);
    }) + "\n";
    out += "bafFaceMaterial = " + JoinList(mesh.faces.size(), [&](size_t i) {
        return std::to_string(mesh.faces[i].material);
    }) + "\n";
    // Die Intensitaet je Vertex ist die Alphaquelle des zweiten
    // Ladedurchgangs.
    out += "bafIntensity = " + JoinList(mesh.intensities.size(), [&](size_t i) {
        return FormatFloat(mesh.intensities[i]);
    }) + "\n";
    return out;
}

std::string ClipPropText(const BafFile& file, size_t clipIndex) {
    if (clipIndex >= file.clips.size()) return std::string();
    const Clip& clip = file.clips[clipIndex];
    const std::string prefix = "bafClip" + std::to_string(clipIndex);
    std::string out;
    out += prefix + "Name = " + clip.name + "\n";
    out += prefix + "Fps = " + FormatFloat(clip.fps) + "\n";
    out += prefix + "Frames = " + std::to_string(clip.numFrames) + "\n";
    out += prefix + "Fade = " + FormatFloat(clip.fadeSeconds) + "\n";
    out += prefix + "Loop = " + std::to_string(clip.var1Loop) + "\n";
    out += prefix + "Var2 = " + std::to_string(clip.var2Frame) + "\n";
    out += prefix + "TrackFlags = " + std::to_string(clip.trackFlags) + "\n";
    out += prefix + "Usage = " + std::to_string(clip.specialUsage) + "\n";
    // Die Maske sagt, welche Koerperteile der Clip anfasst -
    // geANDet gegen node.type.
    out += prefix + "TypeMask = " + std::to_string(clip.typeMask) + "\n";
    out += prefix + "Events = " + JoinList(clip.events.size(), [&](size_t i) {
        const SoundEvent& event = clip.events[i];
        return std::to_string(event.frame) + ":" + event.wav + ":" +
               std::to_string(event.typeBits) + ":" + FormatFloat(event.volume, 3) + ":" +
               std::to_string(event.priority) + ":" + std::to_string(event.soundId);
    }) + "\n";
    return out;
}

void CheckLimits(const BafFile& file, LimitReport& out) {
    out = LimitReport();
    auto Line = [&out](bool bad, const std::string& text) {
        out.text += (bad ? "UEBER  " : "ok     ") + text + "\n";
        if (bad) ++out.exceeded;
    };

    Line(file.nodes.size() > kMaxModelNodes,
         "Knoten " + std::to_string(file.nodes.size()) + " / " +
         std::to_string(kMaxModelNodes));

    // Die Grenzen gelten JE MESH, nicht fuer das Modell insgesamt.
    // Nur das groesste Mesh je Zahl zu melden reicht, aber der
    // Name muss dabeistehen - sonst sucht man es.
    size_t worstVerts = 0, worstFaces = 0, worstTexVerts = 0, worstFaceVerts = 0;
    std::string vertsName, facesName, texVertsName, faceVertsName;
    for (const Mesh& mesh : file.meshes) {
        if (mesh.positions.size() > worstVerts) { worstVerts = mesh.positions.size(); vertsName = mesh.name; }
        if (mesh.faces.size() > worstFaces) { worstFaces = mesh.faces.size(); facesName = mesh.name; }
        if (mesh.texVerts.size() > worstTexVerts) { worstTexVerts = mesh.texVerts.size(); texVertsName = mesh.name; }
        for (const Face& face : mesh.faces) {
            if (face.vertIdx.size() > worstFaceVerts) {
                worstFaceVerts = face.vertIdx.size();
                faceVertsName = mesh.name;
            }
        }
    }
    Line(worstVerts > kMaxMeshVerts, "Vertices je Mesh " + std::to_string(worstVerts) +
         " / " + std::to_string(kMaxMeshVerts) + "  (" + vertsName + ")");
    Line(worstFaces > kMaxMeshFaces, "Faces je Mesh " + std::to_string(worstFaces) +
         " / " + std::to_string(kMaxMeshFaces) + "  (" + facesName + ")");
    Line(worstTexVerts > kMaxMeshTexVerts, "Texturvertices je Mesh " +
         std::to_string(worstTexVerts) + " / " + std::to_string(kMaxMeshTexVerts) +
         "  (" + texVertsName + ")");
    Line(worstFaceVerts > kMaxFaceVerts, "Ecken je Face " +
         std::to_string(worstFaceVerts) + " / " + std::to_string(kMaxFaceVerts) +
         "  (" + faceVertsName + ")");
}

void ComputeBounds(const std::vector<Vec3>& positions, float& radius, Vec3& center) {
    radius = 0.0f;
    center = Vec3{};
    if (positions.empty()) return;

    double lo[3] = { positions[0].x, positions[0].y, positions[0].z };
    double hi[3] = { positions[0].x, positions[0].y, positions[0].z };
    for (const Vec3& point : positions) {
        const double value[3] = { point.x, point.y, point.z };
        for (int axis = 0; axis < 3; ++axis) {
            if (value[axis] < lo[axis]) lo[axis] = value[axis];
            if (value[axis] > hi[axis]) hi[axis] = value[axis];
        }
    }
    const double mid[3] = { (lo[0] + hi[0]) * 0.5, (lo[1] + hi[1]) * 0.5,
                            (lo[2] + hi[2]) * 0.5 };
    double farthest = 0.0;
    for (const Vec3& point : positions) {
        const double dx = point.x - mid[0];
        const double dy = point.y - mid[1];
        const double dz = point.z - mid[2];
        const double squared = dx * dx + dy * dy + dz * dz;
        if (squared > farthest) farthest = squared;
    }
    radius = static_cast<float>(std::sqrt(farthest));
    center = Vec3{ static_cast<float>(mid[0]), static_cast<float>(mid[1]),
                   static_cast<float>(mid[2]) };
}

int32_t HeadNodeIndex(const BafFile& file) {
    const uint8_t neck = file.nodeRefs[1];
    return (neck < file.nodes.size()) ? static_cast<int32_t>(neck) : -1;
}

uint32_t TexVertPool::IndexOf(float u, float v) {
    // Auf 6 Nachkommastellen vergleichen - dieselbe Genauigkeit,
    // mit der die Werte auch im Dump stehen. Ein exakter Vergleich
    // wuerde zwei Ecken trennen, die dieselbe Stelle meinen.
    for (size_t i = 0; i < texVerts_.size(); ++i) {
        if (std::fabs(static_cast<double>(texVerts_[i][0]) - u) <= 5e-7 &&
            std::fabs(static_cast<double>(texVerts_[i][1]) - v) <= 5e-7) {
            return static_cast<uint32_t>(i);
        }
    }
    texVerts_.push_back({ u, v });
    return static_cast<uint32_t>(texVerts_.size() - 1);
}

Vec3 NormalizeOrUp(const Vec3& normal) {
    const double length = std::sqrt(static_cast<double>(normal.x) * normal.x +
                                   static_cast<double>(normal.y) * normal.y +
                                   static_cast<double>(normal.z) * normal.z);
    if (length <= 1e-6) return Vec3{ 0.0f, 0.0f, 1.0f };
    return Vec3{ static_cast<float>(normal.x / length),
                 static_cast<float>(normal.y / length),
                 static_cast<float>(normal.z / length) };
}

void BuildMaterialSlots(const BafFile& file, std::vector<MaterialSlot>& slots) {
    slots.clear();
    // In Datei-Reihenfolge, damit die Nummern zwischen zwei
    // Laeufen dieselben bleiben. Ein std::map waere nach dem
    // Schluessel sortiert und damit weniger nachvollziehbar.
    for (const Mesh& mesh : file.meshes) {
        for (const Face& face : mesh.faces) {
            const RenderClass rc = FaceRenderClass(face);
            bool found = false;
            for (const MaterialSlot& slot : slots) {
                if (slot.material == face.material && slot.blend == rc.blend &&
                    slot.twoSided == rc.twoSided && slot.fullbright == rc.fullbright) {
                    found = true;
                    break;
                }
            }
            if (found) continue;
            MaterialSlot slot;
            slot.material   = face.material;
            slot.blend      = rc.blend;
            slot.twoSided   = rc.twoSided;
            slot.fullbright = rc.fullbright;
            slot.envMap     = rc.envMap;
            slot.alpha      = rc.alpha;   // vom ERSTEN Face dieses Platzes
            slots.push_back(slot);
        }
    }
}

uint32_t SlotOfFace(const std::vector<MaterialSlot>& slots, const Face& face) {
    const RenderClass rc = FaceRenderClass(face);
    for (size_t i = 0; i < slots.size(); ++i) {
        if (slots[i].material == face.material && slots[i].blend == rc.blend &&
            slots[i].twoSided == rc.twoSided && slots[i].fullbright == rc.fullbright) {
            return static_cast<uint32_t>(i);
        }
    }
    return 0;
}

bool DecodeCel(const BafFile& file, size_t materialIndex, size_t celIndex,
               DecodedCel& out) {
    out = DecodedCel();
    if (materialIndex >= file.materials.size()) return false;
    const Material& material = file.materials[materialIndex];
    if (celIndex >= material.cels.size()) return false;
    const Cel& cel = material.cels[celIndex];
    if (cel.pixels.empty() || cel.width == 0 || cel.height == 0) return false;

    // Nur 8 Bit. Alles andere kommt in den Retail-Daten nicht vor,
    // und stillschweigend etwas anderes zu liefern waere schlimmer
    // als abzulehnen.
    if (material.bitDepth != 8) return false;

    const std::vector<uint8_t>* palette = file.MaterialPalette(materialIndex);
    out.width  = cel.width;
    out.height = cel.height;
    out.keyed  = cel.Keyed();
    out.rgba.assign(static_cast<size_t>(cel.width) * cel.height * 4, 0);

    for (uint32_t row = 0; row < cel.height; ++row) {
        for (uint32_t column = 0; column < cel.width; ++column) {
            const size_t source = static_cast<size_t>(row) * cel.width + column;
            const uint8_t index = (source < cel.pixels.size()) ? cel.pixels[source] : 0;
            uint8_t red, green, blue;
            if (palette != nullptr && static_cast<size_t>(index) * 3 + 2 < palette->size()) {
                red   = (*palette)[static_cast<size_t>(index) * 3 + 0];
                green = (*palette)[static_cast<size_t>(index) * 3 + 1];
                blue  = (*palette)[static_cast<size_t>(index) * 3 + 2];
            } else {
                red = green = blue = index;   // ohne Palette: Graustufe
            }
            const size_t target = (static_cast<size_t>(row) * cel.width + column) * 4;
            out.rgba[target + 0] = red;
            out.rgba[target + 1] = green;
            out.rgba[target + 2] = blue;
            // Index 0 ist nur durchsichtig, wenn das Key-Bit es sagt.
            out.rgba[target + 3] = (out.keyed && index == 0) ? 0 : 255;
        }
    }
    return true;
}

bool WriteTga(const DecodedCel& cel, const std::string& path, std::string& error) {
    if (cel.width == 0 || cel.height == 0 || cel.rgba.empty()) {
        error = "leeres Bild";
        return false;
    }
    if (cel.width > 0xFFFFu || cel.height > 0xFFFFu) {
        error = "Bild zu gross fuer das TGA-Format";
        return false;
    }

    std::vector<uint8_t> file;
    file.reserve(18 + cel.rgba.size());
    const uint8_t header[18] = {
        0,          // keine ID
        0,          // keine Farbtabelle
        2,          // unkomprimiert, Echtfarbe
        0, 0, 0, 0, 0,
        0, 0,       // X-Ursprung
        0, 0,       // Y-Ursprung
        static_cast<uint8_t>(cel.width  & 0xFF), static_cast<uint8_t>(cel.width  >> 8),
        static_cast<uint8_t>(cel.height & 0xFF), static_cast<uint8_t>(cel.height >> 8),
        32,         // Bits je Pixel
        8           // 8 Bit Alpha, Ursprung unten links
    };
    file.insert(file.end(), header, header + sizeof header);

    // TGA speichert von unten nach oben, das Cel liegt von oben
    // nach unten - also rueckwaerts durch die Zeilen. Und die
    // Kanalreihenfolge ist BGRA, nicht RGBA.
    for (uint32_t row = cel.height; row-- > 0;) {
        const size_t start = static_cast<size_t>(row) * cel.width * 4;
        for (uint32_t column = 0; column < cel.width; ++column) {
            const size_t pixel = start + static_cast<size_t>(column) * 4;
            file.push_back(cel.rgba[pixel + 2]);
            file.push_back(cel.rgba[pixel + 1]);
            file.push_back(cel.rgba[pixel + 0]);
            file.push_back(cel.rgba[pixel + 3]);
        }
    }

    std::FILE* handle = nullptr;
#if defined(_WIN32)
    // Schreiben braucht denselben Umweg wie Lesen, sonst scheitert
    // ein Pfad mit Umlaut. OpenBinaryRead oeffnet nur lesend,
    // deshalb hier von Hand - ueber dieselbe Umwandlung.
    handle = OpenBinaryWrite(path);
#else
    handle = std::fopen(path.c_str(), "wb");
#endif
    if (handle == nullptr) { error = path + ": laesst sich nicht schreiben"; return false; }
    const size_t written = std::fwrite(file.data(), 1, file.size(), handle);
    std::fclose(handle);
    if (written != file.size()) { error = path + ": unvollstaendig geschrieben"; return false; }
    return true;
}

std::string SafeFileName(const std::string& name, const std::string& fallback) {
    // Erst auf druckbares ASCII eindampfen, dann auf den
    // Basisnamen kuerzen, dann die unter Windows verbotenen
    // Zeichen ersetzen. Die Reihenfolge ist wichtig: erst nach dem
    // Kuerzen ist sicher, dass kein Pfadanteil mehr drin steckt.
    std::string clean;
    for (unsigned char character : name) {
        if (character >= 32 && character < 127) clean.push_back(static_cast<char>(character));
    }
    for (char& character : clean) {
        if (character == '\\') character = '/';
    }
    // Punkte und Leerzeichen am Ende weg - "a.." wuerde sonst zu
    // einem Verweis auf den Elternordner.
    while (!clean.empty() && (clean.back() == '/' || clean.back() == ' ' || clean.back() == '.')) {
        clean.pop_back();
    }
    const size_t cut = clean.find_last_of('/');
    if (cut != std::string::npos) clean = clean.substr(cut + 1);

    std::string result;
    for (char character : clean) {
        const bool illegal = (character == '<' || character == '>' || character == ':' ||
                              character == '"' || character == '|' || character == '?' ||
                              character == '*' || character == '/');
        result.push_back(illegal ? '_' : character);
    }
    // Fuehrende und schliessende Leerzeichen weg
    while (!result.empty() && result.front() == ' ') result.erase(result.begin());
    while (!result.empty() && result.back() == ' ') result.pop_back();
    return result.empty() ? fallback : result;
}

void BuildMeshGeometry(const BafFile& file, size_t meshIndex, double scale,
                       const std::vector<MaterialSlot>& slots, BuiltMesh& out) {
    out = BuiltMesh();
    if (meshIndex >= file.meshes.size()) return;
    const Mesh& mesh = file.meshes[meshIndex];

    out.name = mesh.name;
    out.meshIndex = static_cast<int32_t>(meshIndex);
    out.drawn = mesh.Drawn();

    const Node* owner = file.NodeOfMesh(static_cast<int32_t>(meshIndex));
    Vec3 pivot{};
    if (owner != nullptr) {
        out.nodeIndex = owner->num;
        pivot = owner->pivot;
    }

    out.positions.resize(mesh.positions.size());
    for (size_t i = 0; i < mesh.positions.size(); ++i) {
        out.positions[i] = Vec3{
            static_cast<float>((static_cast<double>(mesh.positions[i].x) + pivot.x) * scale),
            static_cast<float>((static_cast<double>(mesh.positions[i].y) + pivot.y) * scale),
            static_cast<float>((static_cast<double>(mesh.positions[i].z) + pivot.z) * scale)};
    }
    out.normals = mesh.normals;

    for (size_t faceIndex = 0; faceIndex < mesh.faces.size(); ++faceIndex) {
        const Face& face = mesh.faces[faceIndex];
        const std::vector<uint32_t>& corners = face.vertIdx;

        // Dieselben drei Ausschlussgruende wie im Blender-Addon:
        // zu wenig Ecken, ein Vertex doppelt, Index ausserhalb.
        bool bad = (corners.size() < 3);
        for (size_t a = 0; !bad && a < corners.size(); ++a) {
            if (corners[a] >= out.positions.size()) { bad = true; break; }
            for (size_t b = a + 1; b < corners.size(); ++b) {
                if (corners[a] == corners[b]) { bad = true; break; }
            }
        }
        if (bad) {
            out.skippedFaces.push_back(static_cast<uint32_t>(faceIndex));
            continue;
        }

        // Die Cel-Groesse haengt am Material DIESES Face, nicht am
        // Mesh - innerhalb eines Mesh kann sie wechseln.
        uint32_t celWidth = 256, celHeight = 256;
        file.MaterialCelSize(face.material, celWidth, celHeight);
        const double invWidth  = (celWidth  != 0) ? 1.0 / celWidth  : 0.0;
        const double invHeight = (celHeight != 0) ? 1.0 / celHeight : 0.0;

        // UV einer Ecke. Ohne texIdx (unbetextetes Material) oder
        // bei einem Index ausserhalb bleibt sie auf 0/0 - genauso
        // macht es das Addon.
        auto CornerUv = [&](size_t corner, float& u, float& v) {
            u = 0.0f; v = 0.0f;
            if (corner >= face.texIdx.size()) return;
            const uint32_t texel = face.texIdx[corner];
            if (texel >= mesh.texVerts.size()) return;
            u = static_cast<float>(mesh.texVerts[texel][0] * invWidth);
            v = static_cast<float>(-mesh.texVerts[texel][1] * invHeight);
        };

        // Faechertriangulierung ab Ecke 0.
        for (size_t corner = 1; corner + 1 < corners.size(); ++corner) {
            BuiltTri tri;
            tri.material   = face.material;
            tri.slot       = slots.empty() ? 0u : SlotOfFace(slots, face);
            tri.sourceFace = static_cast<uint32_t>(faceIndex);
            const size_t pick[3] = {0, corner, corner + 1};
            for (int k = 0; k < 3; ++k) {
                tri.vertex[k] = corners[pick[k]];
                CornerUv(pick[k], tri.uv[k][0], tri.uv[k][1]);
            }
            out.tris.push_back(tri);
        }
    }
}

void EulerBasisD(double pitchDeg, double yawDeg, double rollDeg,
                 double right[3], double forward[3], double up[3]) {
    // Reihenfolge und Klammerung sind wortwoertlich die von
    // euler_basis() im Blender-Addon. Nicht "vereinfachen": ein
    // umgestellter Term ergibt in den letzten Bits ein anderes
    // Ergebnis, und der Vergleich gegen die Python-Referenz haette
    // dann Abweichungen, die keine sind.
    constexpr double kDegToRad = 3.14159265358979323846 / 180.0;
    const double pitch = pitchDeg * kDegToRad;
    const double yaw   = yawDeg   * kDegToRad;
    const double roll  = rollDeg  * kDegToRad;
    const double sp = std::sin(pitch), cp = std::cos(pitch);
    const double sy = std::sin(yaw),   cy = std::cos(yaw);
    const double sr = std::sin(roll),  cr = std::cos(roll);

    right[0]   = -(sr * sy) * sp + cr * cy;
    right[1]   = (sr * cy) * sp + cr * sy;
    right[2]   = -sr * cp;
    forward[0] = -sy * cp;
    forward[1] = cy * cp;
    forward[2] = sp;
    up[0]      = (cr * sy) * sp + sr * cy;
    up[1]      = -sp * (cy * cr) + sy * sr;
    up[2]      = cr * cp;
}

void BasisToEuler(const double right[3], const double forward[3], const double up[3],
                  double& pitchDeg, double& yawDeg, double& rollDeg) {
    constexpr double kRadToDeg = 180.0 / 3.14159265358979323846;
    // Klemmen, bevor asin daraus NaN macht: ein float-Rundungsfehler
    // reicht, um forward[2] knapp ueber 1 zu schieben.
    double sinPitch = forward[2];
    if (sinPitch < -1.0) sinPitch = -1.0;
    if (sinPitch >  1.0) sinPitch =  1.0;
    const double pitch = std::asin(sinPitch);
    pitchDeg = pitch * kRadToDeg;

    if (std::fabs(std::cos(pitch)) < 1e-6) {
        // Kardanblockade: das Rollen in die Gierung falten.
        yawDeg  = std::atan2(right[1], right[0]) * kRadToDeg;
        rollDeg = 0.0;
        return;
    }
    yawDeg  = std::atan2(-forward[0], forward[1]) * kRadToDeg;
    rollDeg = std::atan2(-right[2], up[2]) * kRadToDeg;
}

void EulerBasis(float pitchDeg, float yawDeg, float rollDeg,
                Vec3& right, Vec3& forward, Vec3& up) {
    double r[3], f[3], u[3];
    EulerBasisD(pitchDeg, yawDeg, rollDeg, r, f, u);
    right   = Vec3{ static_cast<float>(r[0]), static_cast<float>(r[1]), static_cast<float>(r[2]) };
    forward = Vec3{ static_cast<float>(f[0]), static_cast<float>(f[1]), static_cast<float>(f[2]) };
    up      = Vec3{ static_cast<float>(u[0]), static_cast<float>(u[1]), static_cast<float>(u[2]) };
}

Matrix34 NodeLocalMatrix(const Node& node, double scale) {
    Matrix34 out;
    EulerBasisD(node.bindEuler.x, node.bindEuler.y, node.bindEuler.z,
                out.m[0], out.m[1], out.m[2]);
    out.m[3][0] = static_cast<double>(node.bindPos.x) * scale;
    out.m[3][1] = static_cast<double>(node.bindPos.y) * scale;
    out.m[3][2] = static_cast<double>(node.bindPos.z) * scale;
    return out;
}

Matrix34 Concat(const Matrix34& parent, const Matrix34& local) {
    // In der Konvention der Engine sind right/forward/up die
    // Spalten, also gilt v' = right*v.x + forward*v.y + up*v.z.
    // Genau das macht Apply() - und damit ist Concat dasselbe wie
    // das "parent @ local" des Blender-Addons.
    auto Apply = [&parent](const double v[3], double out[3]) {
        for (int axis = 0; axis < 3; ++axis) {
            out[axis] = parent.m[0][axis] * v[0]
                      + parent.m[1][axis] * v[1]
                      + parent.m[2][axis] * v[2];
        }
    };
    Matrix34 result;
    Apply(local.m[0], result.m[0]);
    Apply(local.m[1], result.m[1]);
    Apply(local.m[2], result.m[2]);
    Apply(local.m[3], result.m[3]);
    for (int axis = 0; axis < 3; ++axis) result.m[3][axis] += parent.m[3][axis];
    return result;
}

void ComputeRestWorld(const BafFile& file, double scale, std::vector<Matrix34>& out) {
    out.clear();
    out.reserve(file.nodes.size());
    for (size_t i = 0; i < file.nodes.size(); ++i) {
        const Node& node = file.nodes[i];
        const Matrix34 local = NodeLocalMatrix(node, scale);
        // Ein Elternteil steht in der Datei immer vor seinem Kind.
        // Ein Verweis nach vorne waere ein Formatfehler; hier wird
        // der Knoten dann als Wurzel behandelt, wie im Addon auch.
        if (node.parent >= 0 && static_cast<size_t>(node.parent) < i) {
            out.push_back(Concat(out[static_cast<size_t>(node.parent)], local));
        } else {
            out.push_back(local);
        }
    }
}

size_t CursorAt(const Track& track, float frame) {
    size_t i = 0;
    while (i + 1 < track.keys.size() && track.keys[i + 1].frame <= frame) ++i;
    return i;
}

Matrix34 SampleLocalMatrix(const Track& track, double frame, double scale) {
    Matrix34 out{};
    // Ohne Keys: Einheitsbasis, keine Verschiebung. Ein Track ohne
    // Keys kommt vor und ist kein Fehler.
    out.m[0][0] = out.m[1][1] = out.m[2][2] = 1.0;
    if (track.keys.empty()) return out;

    const Key& key = track.keys[CursorAt(track, static_cast<float>(frame))];
    const double elapsed = frame - static_cast<double>(key.frame);

    // Ohne sein Interpolationsbit HAELT ein Key seinen Wert bis
    // zum naechsten - er wird nicht interpoliert. Das ist der
    // Grund, warum CursorAt den LETZTEN Key mit frame <= t nimmt
    // und nicht den naechstgelegenen.
    double position[3], orientation[3];
    for (int axis = 0; axis < 3; ++axis) {
        const double pos = (axis == 0) ? key.pos.x : (axis == 1) ? key.pos.y : key.pos.z;
        const double dpos = (axis == 0) ? key.dpos.x : (axis == 1) ? key.dpos.y : key.dpos.z;
        const double orient = (axis == 0) ? key.orient.x
                            : (axis == 1) ? key.orient.y : key.orient.z;
        const double dorient = (axis == 0) ? key.dorient.x
                             : (axis == 1) ? key.dorient.y : key.dorient.z;
        position[axis]    = (key.flags & kKeyLerpPos)    ? (pos + dpos * elapsed) : pos;
        orientation[axis] = (key.flags & kKeyLerpOrient) ? (orient + dorient * elapsed)
                                                         : orient;
    }

    EulerBasisD(orientation[0], orientation[1], orientation[2],
                out.m[0], out.m[1], out.m[2]);
    for (int axis = 0; axis < 3; ++axis) out.m[3][axis] = position[axis] * scale;
    return out;
}

void SampleTrack(const Track& track, float frame, Vec3& pos, Vec3& orient) {
    pos = Vec3{}; orient = Vec3{};
    if (track.keys.empty()) return;
    const Key& key = track.keys[CursorAt(track, frame)];
    const float elapsed = frame - key.frame;

    if (key.flags & kKeyLerpPos) {
        pos.x = key.pos.x + key.dpos.x * elapsed;
        pos.y = key.pos.y + key.dpos.y * elapsed;
        pos.z = key.pos.z + key.dpos.z * elapsed;
    } else {
        pos = key.pos;
    }
    if (key.flags & kKeyLerpOrient) {
        orient.x = key.orient.x + key.dorient.x * elapsed;
        orient.y = key.orient.y + key.dorient.y * elapsed;
        orient.z = key.orient.z + key.dorient.z * elapsed;
    } else {
        orient = key.orient;
    }
}

// ============================================================
//  Der eigentliche Parser
// ============================================================

namespace {

void ParseInto(Cursor& cur, const std::string& source, BafFile& out, std::string& warnings) {
    const size_t fileBytes = cur.Size();
    if (fileBytes < kHeaderBytes) throw ParseError("Datei ist kuerzer als der 0x120-Byte-Header");
    if (cur.U32At(0x04) != kHeaderBytes) throw ParseError("hdr+0x04 != 0x120, also keine .baf");

    out.source    = source;
    out.rawHeader = cur.RawAt(0, kHeaderBytes);
    out.name    = cur.CStrAt(0x08, 0x20);
    out.scale   = cur.F32At(0xac);
    out.radiusU = cur.F32At(0xec);
    out.heightU = cur.F32At(0xf4);
    for (size_t i = 0; i < 4; ++i) out.nodeRefs[i] = cur.U8At(0xf0 + i);

    const uint64_t geoBytes      = cur.U32At(0xc0);
    const uint32_t materialCount = cur.U32At(0xc4);
    const uint32_t clipCount     = cur.U32At(0xc8);
    const uint64_t materialBytes = cur.U32At(0xd0);
    const uint64_t animBytes     = cur.U32At(0xd4);
    const uint64_t paletteBytes  = cur.U32At(0xdc);
    const uint64_t geoOffset     = cur.U32At(0xe0);
    const uint64_t animOffset    = cur.U32At(0xe4);

    // Die Bloecke muessen die Datei lueckenlos kacheln. In 64 Bit
    // gerechnet, damit eine kaputte Zahl aus der Datei nicht
    // ueberlaeuft und dabei zufaellig aufgeht.
    const uint64_t total = static_cast<uint64_t>(fileBytes);
    if (kHeaderBytes + materialBytes + paletteBytes != total - animBytes - geoBytes ||
        total < animBytes + geoBytes) {
        throw ParseError("die Blockgroessen kacheln die Datei nicht");
    }
    if (kHeaderBytes + animOffset + animBytes != total) {
        throw ParseError("der Animationsblock endet nicht am Dateiende");
    }
    if (geoOffset != materialBytes + paletteBytes || animOffset != geoOffset + geoBytes) {
        throw ParseError("die gespiegelten Offsets widersprechen den Blockgroessen");
    }

    // ---- Palette ----
    // Der Palettenblock hat 0x30 Byte Vorspann, dann 768 Byte RGB.
    out.paletteBlock = cur.RawAt(kHeaderBytes, static_cast<size_t>(paletteBytes));
    if (paletteBytes >= 0x30 + 768) {
        cur.Seek(kHeaderBytes + 0x30);
        cur.Bytes(out.palette, 768);
    }

    // ---- Materialien ----
    cur.Seek(kHeaderBytes + static_cast<size_t>(paletteBytes));
    const size_t materialEnd = cur.Tell() + static_cast<size_t>(materialBytes);
    out.materials.reserve(materialCount);
    for (uint32_t materialIndex = 0; materialIndex < materialCount; ++materialIndex) {
        Material material;
        material.rawName = cur.RawAt(cur.Tell(), 0x20);
        material.name = cur.CStr(0x20);

        // Das 'MAT '-Tag als u32 vergleichen, nicht als String:
        // CStrAt wuerde an einer NUL abschneiden, und hier zaehlen
        // alle vier Bytes.
        const size_t tagAt = cur.Tell();
        const uint32_t tag = cur.U32At(tagAt);
        uint32_t wantTag = 0;
        std::memcpy(&wantTag, "MAT ", 4);
        if (tag != wantTag) {
            throw ParseError("Material '" + material.name + "': kein 'MAT '-Tag");
        }
        const uint32_t version = cur.U32At(tagAt + 0x04);
        if (version != 50) {
            throw ParseError("Material '" + material.name + "': Version " +
                             std::to_string(version) + ", der Loader verlangt 50");
        }
        material.matType          = cur.U32At(tagAt + 0x08);
        const uint32_t recordCount = cur.U32At(tagAt + 0x0c);
        const uint32_t celCount    = cur.U32At(tagAt + 0x10);
        material.bitDepth         = cur.U32At(tagAt + 0x18);
        if (material.bitDepth == 0 || material.bitDepth % 8 != 0) {
            throw ParseError("Material '" + material.name + "': Bittiefe " +
                             std::to_string(material.bitDepth));
        }
        material.rawHeader = cur.RawAt(tagAt, kMatHeaderBytes);
        cur.Skip(kMatHeaderBytes);

        // Die Saetze: Bit 3 verlaengert einen Satz von 0x18 auf 0x28.
        const size_t recordStart = cur.Tell();
        for (uint32_t record = 0; record < recordCount; ++record) {
            const uint32_t recordFlags = cur.U32At(cur.Tell());
            cur.Skip((recordFlags & 8u) ? 0x28 : 0x18);
        }
        // Unangetastet durchreichen - was da steht, weiss niemand.
        material.rawRecords = cur.RawAt(recordStart, cur.Tell() - recordStart);

        // Die Cel-Kette wechselt sich ab: Kopf, Pixel, Kopf, Pixel -
        // nicht erst alle Koepfe.
        material.cels.reserve(celCount);
        for (uint32_t celIndex = 0; celIndex < celCount; ++celIndex) {
            const size_t celAt = cur.Tell();
            Cel cel;
            cel.width          = cur.U32At(celAt + 0x00);
            cel.height         = cur.U32At(celAt + 0x04);
            cel.colorKeyEnable = cur.U32At(celAt + 0x08);
            cel.numMips        = cur.U32At(celAt + 0x14);
            cel.raw            = cur.RawAt(celAt, kMatCelBytes);
            cur.Skip(kMatCelBytes);
            const uint64_t pixelBytes =
                CelPixelBytes(cel.width, cel.height, cel.numMips, material.bitDepth);
            if (pixelBytes > total) {
                throw ParseError("Material '" + material.name + "': Cel " +
                                 std::to_string(celIndex) + " behauptet " +
                                 std::to_string(pixelBytes) + " Byte Pixel");
            }
            cur.Bytes(cel.pixels, static_cast<size_t>(pixelBytes));
            material.cels.push_back(std::move(cel));
        }
        if (material.matType & kMatOwnPalette) cur.Bytes(material.ownPalette, 768);
        out.materials.push_back(std::move(material));
    }
    if (cur.Tell() != materialEnd) {
        throw ParseError("Materialblock endet an " + std::to_string(cur.Tell()) +
                         ", erwartet war " + std::to_string(materialEnd));
    }

    // ---- Geometrie (das Skelett ist ihr Schwanz) ----
    const size_t geoBase = kHeaderBytes + static_cast<size_t>(geoOffset);
    const size_t geoEnd  = geoBase + static_cast<size_t>(geoBytes);
    const uint32_t geosetCount = cur.U32At(geoBase + 0x44);
    // Alle 303 Retail-Modelle tragen genau ein Geoset. Der Parser
    // flacht ohnehin alles in eine Liste, also waere ein zweites
    // still verformt - deshalb hier abbrechen statt raten.
    if (geosetCount > 1) {
        throw ParseError("geo+0x44 nennt " + std::to_string(geosetCount) +
                         " Geosets; unterstuetzt ist eins");
    }
    if (cur.U32At(geoBase + 0x4c) != materialCount) {
        throw ParseError("geo+0x4c widerspricht hdr+0xc4 (die Zahl steht zweimal da)");
    }
    const uint32_t nodeCount = cur.U32At(geoBase + 0x54);
    out.rawGeoHeader = cur.RawAt(geoBase, kGeoHeaderBytes);
    cur.Seek(geoBase + kGeoHeaderBytes);

    for (uint32_t geoset = 0; geoset < geosetCount; ++geoset) {
        const uint32_t meshCount = cur.U32At(geoBase + 0x24 + geoset * 8);
        if (meshCount == 0) continue;

        // Erst ALLE Mesh-Saetze, dann alle ihre Daten.
        const size_t recordBase = cur.Tell();
        cur.Skip(static_cast<size_t>(meshCount) * kMeshBytes);

        out.meshes.reserve(out.meshes.size() + meshCount);
        for (uint32_t meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
            const size_t rec = recordBase + static_cast<size_t>(meshIndex) * kMeshBytes;
            if (cur.I32At(rec + 0x20) != static_cast<int32_t>(meshIndex)) {
                throw ParseError("Mesh " + std::to_string(meshIndex) +
                                 " traegt den Eigenindex " +
                                 std::to_string(cur.I32At(rec + 0x20)));
            }
            Mesh mesh;
            mesh.name         = cur.CStrAt(rec + 0x00, 0x20);
            mesh.geometryMode = cur.U32At(rec + 0x24);
            mesh.lightingMode = cur.U32At(rec + 0x28);
            mesh.textureMode  = cur.U32At(rec + 0x2c);
            mesh.radius       = cur.F32At(rec + 0x54);
            mesh.center       = cur.V3At(rec + 0x58);
            mesh.raw          = cur.RawAt(rec, kMeshBytes);

            const uint32_t vertCount    = cur.U32At(rec + 0x48);
            const uint32_t texVertCount = cur.U32At(rec + 0x4c);
            const uint32_t faceCount    = cur.U32At(rec + 0x50);

            mesh.positions.resize(vertCount);
            for (uint32_t i = 0; i < vertCount; ++i) mesh.positions[i] = cur.V3();
            mesh.normals.resize(vertCount);
            for (uint32_t i = 0; i < vertCount; ++i) mesh.normals[i] = cur.V3();
            mesh.intensities.resize(vertCount);
            for (uint32_t i = 0; i < vertCount; ++i) mesh.intensities[i] = cur.F32();
            // Totes Feld, nie beschrieben - gehoert aber zur Datei.
            mesh.dead = cur.RawAt(cur.Tell(), static_cast<size_t>(vertCount) * 4);
            cur.Skip(static_cast<size_t>(vertCount) * 4);
            mesh.texVerts.resize(texVertCount);
            for (uint32_t i = 0; i < texVertCount; ++i) {
                mesh.texVerts[i][0] = cur.F32();
                mesh.texVerts[i][1] = cur.F32();
            }

            // Erst alle Face-Saetze, dann die Indexbloecke.
            const size_t faceBase = cur.Tell();
            cur.Skip(static_cast<size_t>(faceCount) * kFaceBytes);
            mesh.faces.reserve(faceCount);
            for (uint32_t faceIndex = 0; faceIndex < faceCount; ++faceIndex) {
                const size_t fr = faceBase + static_cast<size_t>(faceIndex) * kFaceBytes;
                Face face;
                face.material     = cur.U32At(fr + 0x00);
                face.flags        = cur.U32At(fr + 0x04);
                face.geometryMode = cur.U32At(fr + 0x08);
                face.lightMode    = cur.U32At(fr + 0x0c);
                face.textureMode  = cur.U32At(fr + 0x10);
                face.extraLight   = cur.F32At(fr + 0x30);
                face.v0           = cur.F32At(fr + 0x34);
                face.normal       = cur.V3At(fr + 0x38);
                face.raw          = cur.RawAt(fr, kFaceBytes);

                const uint32_t faceVerts = cur.U32At(fr + 0x14);
                if (faceVerts > 0) {
                    face.vertIdx.resize(faceVerts);
                    for (uint32_t k = 0; k < faceVerts; ++k) face.vertIdx[k] = cur.U32();
                    // Die texIdx-Regel, pro Face, wortwoertlich aus
                    // dem Fixup des Loaders.
                    if (face.material != 0xffffu) {
                        if (face.material >= out.materials.size()) {
                            throw ParseError("Mesh '" + mesh.name + "' Face " +
                                             std::to_string(faceIndex) + " nennt Material " +
                                             std::to_string(face.material) + ", die Datei hat " +
                                             std::to_string(out.materials.size()));
                        }
                        if (out.materials[face.material].matType & kMatTextured) {
                            face.texIdx.resize(faceVerts);
                            for (uint32_t k = 0; k < faceVerts; ++k) face.texIdx[k] = cur.U32();
                        }
                    }
                }
                mesh.faces.push_back(std::move(face));
            }
            ApplyLoadPasses(mesh, out.materials);
            out.meshes.push_back(std::move(mesh));
        }
    }

    const size_t nodeBase = cur.Tell();
    cur.Skip(static_cast<size_t>(nodeCount) * kNodeBytes);
    if (cur.Tell() != geoEnd) {
        throw ParseError("Geometrieblock endet an " + std::to_string(cur.Tell()) +
                         ", erwartet war " + std::to_string(geoEnd));
    }
    out.nodes.reserve(nodeCount);
    for (uint32_t i = 0; i < nodeCount; ++i) {
        const size_t nr = nodeBase + static_cast<size_t>(i) * kNodeBytes;
        if (cur.I32At(nr + 0x44) != static_cast<int32_t>(i)) {
            throw ParseError("Knoten " + std::to_string(i) + " traegt den Eigenindex " +
                             std::to_string(cur.I32At(nr + 0x44)));
        }
        Node node;
        node.name        = cur.CStrAt(nr + 0x00, 0x40);
        node.flags       = cur.U32At(nr + 0x40);
        node.num         = cur.I32At(nr + 0x44);
        node.type        = cur.U32At(nr + 0x48);
        node.meshIndex   = cur.I32At(nr + 0x4c);
        node.parent      = cur.I32At(nr + 0x50);
        node.numChildren = cur.I32At(nr + 0x54);
        node.child       = cur.I32At(nr + 0x58);
        node.sibling     = cur.I32At(nr + 0x5c);
        node.pivot       = cur.V3At(nr + 0x60);
        node.bindPos     = cur.V3At(nr + 0x6c);
        node.bindEuler   = cur.V3At(nr + 0x78);
        node.raw         = cur.RawAt(nr, kNodeBytes);
        out.nodes.push_back(std::move(node));
    }

    // ---- Animation ----
    cur.Seek(kHeaderBytes + static_cast<size_t>(animOffset));
    uint64_t totalEvents = 0;
    std::vector<std::pair<uint32_t, uint32_t>> eventRanges;  // (first, count) je Clip
    out.clips.reserve(clipCount);
    for (uint32_t clipIndex = 0; clipIndex < clipCount; ++clipIndex) {
        const size_t clipStart = cur.Tell();
        const uint32_t blockBytes   = cur.U32At(clipStart + 0x00);
        const uint32_t eventCount   = cur.U32At(clipStart + 0x1c);
        const uint32_t eventFirst   = cur.U32At(clipStart + 0x20);
        if (eventFirst != totalEvents) {
            throw ParseError("Clip " + std::to_string(clipIndex) +
                             ": sound_event_first ist nicht die laufende Summe");
        }
        Clip clip;
        clip.fadeSeconds  = cur.F32At(clipStart + 0x08);
        clip.var1Loop     = cur.U32At(clipStart + 0x0c);
        clip.var2Frame    = cur.U32At(clipStart + 0x10);
        clip.trackFlags   = cur.U32At(clipStart + 0x14);
        clip.specialUsage = cur.U32At(clipStart + 0x18);
        clip.rawDesc      = cur.RawAt(clipStart, kDescBytes);
        totalEvents += eventCount;
        eventRanges.emplace_back(eventFirst, eventCount);
        cur.Skip(kDescBytes);

        const size_t keyHeader = cur.Tell();
        clip.name      = cur.CStrAt(keyHeader + 0x00, 0x24);
        clip.typeMask  = cur.U32At(keyHeader + 0x2c);
        clip.fps       = cur.F32At(keyHeader + 0x30);
        clip.numFrames = cur.I32At(keyHeader + 0x34);
        clip.rawKeyHdr = cur.RawAt(keyHeader, kKeyHdrBytes);
        const int32_t trackCount = cur.I32At(keyHeader + 0x38);  // +0x38, NICHT +0x28
        if (trackCount < 0) {
            throw ParseError("Clip '" + clip.name + "': negative Trackanzahl");
        }
        cur.Skip(kKeyHdrBytes);

        // Erst alle Track-Koepfe, dann alle ihre Keys.
        struct Head { std::string name; int32_t joint; uint32_t keyCount;
                      std::vector<uint8_t> raw; };
        std::vector<Head> heads;
        heads.reserve(static_cast<size_t>(trackCount));
        for (int32_t t = 0; t < trackCount; ++t) {
            const size_t tr = cur.Tell();
            Head head;
            head.name     = cur.CStrAt(tr + 0x00, 0x20);
            head.joint    = cur.I32At(tr + 0x20);
            const int32_t keyCount = cur.I32At(tr + 0x24);
            if (keyCount < 0) throw ParseError("Clip '" + clip.name + "': negative Keyanzahl");
            head.keyCount = static_cast<uint32_t>(keyCount);
            head.raw      = cur.RawAt(tr, kKeyNodeBytes);
            heads.push_back(std::move(head));
            cur.Skip(kKeyNodeBytes);
        }
        clip.tracks.reserve(heads.size());
        for (const Head& head : heads) {
            Track track;
            track.name = head.name;
            track.jointIndex = head.joint;
            track.raw = head.raw;
            track.keys.resize(head.keyCount);
            for (uint32_t k = 0; k < head.keyCount; ++k) {
                const size_t kr = cur.Tell();
                Key& key = track.keys[k];
                key.frame   = cur.F32At(kr + 0x00);
                key.flags   = cur.U32At(kr + 0x04);
                key.pos     = cur.V3At(kr + 0x08);
                key.orient  = cur.V3At(kr + 0x14);
                key.dpos    = cur.V3At(kr + 0x20);
                key.dorient = cur.V3At(kr + 0x2c);
                key.raw     = cur.RawAt(kr, kKeyBytes);
                cur.Skip(kKeyBytes);
            }
            clip.tracks.push_back(std::move(track));
        }
        if (cur.Tell() - clipStart != blockBytes) {
            throw ParseError("Clip '" + clip.name + "': " +
                             std::to_string(cur.Tell() - clipStart) +
                             " Byte gelesen, der Descriptor sagt " + std::to_string(blockBytes));
        }
        out.clips.push_back(std::move(clip));
    }

    // ---- Sound-Events ----
    const size_t eventBase = cur.Tell();
    for (uint64_t i = 0; i < totalEvents; ++i) {
        const size_t er = eventBase + static_cast<size_t>(i) * kEventBytes;
        SoundEvent event;
        event.frame     = cur.U32At(er + 0x00);
        event.typeBits  = cur.U32At(er + 0x04);
        event.wav       = cur.CStrAt(er + 0x08, 0x10);
        event.callFlags = cur.U32At(er + 0x20);
        event.volume    = cur.F32At(er + 0x24);
        event.rangeU    = cur.F32At(er + 0x2c);
        event.minDistU  = cur.F32At(er + 0x30);
        event.maxDistU  = cur.F32At(er + 0x34);
        event.priority  = cur.U32At(er + 0x38);
        event.soundId   = cur.I32At(er + 0x3c);
        event.raw       = cur.RawAt(er, kEventBytes);

        bool placed = false;
        for (size_t c = 0; c < eventRanges.size(); ++c) {
            const uint32_t first = eventRanges[c].first;
            const uint32_t count = eventRanges[c].second;
            if (i >= first && i < static_cast<uint64_t>(first) + count) {
                out.clips[c].events.push_back(std::move(event));
                placed = true;
                break;
            }
        }
        if (!placed) {
            throw ParseError("Sound-Event " + std::to_string(i) + " gehoert zu keinem Clip");
        }
    }
    if (eventBase + static_cast<size_t>(totalEvents) * kEventBytes != fileBytes) {
        throw ParseError("die Sound-Event-Tabelle endet nicht genau am Dateiende");
    }

    // Nicht fatale Hinweise: Dinge, die den Import nicht
    // aufhalten, aber beim Nachsehen helfen.
    if (out.nodes.empty()) warnings += "Modell hat keine Knoten\n";
    if (out.meshes.empty()) warnings += "Modell hat keine Meshes\n";
}

} // namespace

ReadResult ReadBafBuffer(const uint8_t* data, size_t size, const std::string& source,
                         BafFile& out) {
    ReadResult result;
    out = BafFile();
    if (!data && size) { result.error = "Nullzeiger mit Groesse != 0"; return result; }

    Cursor cursor(data, size);
    try {
        ParseInto(cursor, source, out, result.warnings);
    } catch (const ParseError& error) {
        out = BafFile();
        result.error = source.empty() ? error.what() : (source + ": " + error.what());
        return result;
    } catch (const std::bad_alloc&) {
        out = BafFile();
        result.error = source + ": Speicher reicht nicht - Datei vermutlich beschaedigt";
        return result;
    }
    result.ok = true;
    return result;
}

ReadResult ReadBafFile(const std::string& path, BafFile& out) {
    ReadResult result;
    std::FILE* file = OpenBinaryRead(path);
    if (!file) { result.error = path + ": laesst sich nicht oeffnen"; return result; }

    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);
    std::fseek(file, 0, SEEK_SET);
    if (length <= 0) {
        std::fclose(file);
        result.error = path + ": leere Datei";
        return result;
    }
    std::vector<uint8_t> buffer(static_cast<size_t>(length));
    const size_t got = std::fread(buffer.data(), 1, buffer.size(), file);
    std::fclose(file);
    if (got != buffer.size()) { result.error = path + ": nicht vollstaendig lesbar"; return result; }

    return ReadBafBuffer(buffer.data(), buffer.size(), path, out);
}

// ============================================================
//  Ausgabe
// ============================================================

std::string FormatFloat(double value, int decimals) {
    // ------------------------------------------------------------
    //  Warum das hier von Hand gerechnet wird und nicht mit
    //  to_chars oder printf:
    //
    //  Beides waere locale-fest genug. Aber KEINES von beiden legt
    //  fest, wohin ein exakter Rundungs-Gleichstand faellt. Der
    //  Standard sagt zu to_chars(fixed) nur "wie printf im
    //  C-Locale", und die Tie-Regel von printf ist Sache der
    //  Implementierung - die UCRT von Microsoft darf sie anders
    //  ziehen als die glibc.
    //
    //  In den Retail-Daten gibt es 473 Werte, deren exakte
    //  Dezimaldarstellung genau auf der Grenze der 6. Nachkomma-
    //  stelle liegt. Bei denen koennten C++-Dump und Python-Dump
    //  auseinanderlaufen, ohne dass am Parser irgendetwas falsch
    //  waere - man wuerde dann tagelang einen Fehler suchen, den
    //  es nicht gibt.
    //
    //  Also: skalieren, Betrag bilden, +0.5, abrunden. Die
    //  Multiplikation ist eine gewoehnliche IEEE-754-Operation und
    //  damit auf jeder Plattform bitgleich; die Tie-Regel ist
    //  danach unsere eigene (kaufmaennisch, von der Null weg) und
    //  steht in beiden Sprachen im Quelltext. pydump.py rechnet
    //  Zeile fuer Zeile dasselbe.
    //
    //  Nebeneffekt: kein <charconv> noetig. Damit faellt die
    //  Weiche fuer die Toolsets vor VS 2019 16.4 weg, und alle
    //  Max-Jahrgaenge 2016-2027 gehen durch denselben Code.
    //
    //  Der groesste Betrag im Datensatz ist 1,9e6, skaliert also
    //  1,9e12 - weit unter 2^53, wo ein double noch jede ganze
    //  Zahl exakt haelt. Das +0.5 ist dort verlustfrei.
    // ------------------------------------------------------------
    if (std::isnan(value)) return "nan";
    if (std::isinf(value)) return (value < 0.0) ? "-inf" : "inf";
    if (decimals < 0) decimals = 0;
    if (decimals > 9) decimals = 9;

    static const double kPow10[10] = {
        1.0, 10.0, 100.0, 1000.0, 10000.0,
        100000.0, 1000000.0, 10000000.0, 100000000.0, 1000000000.0
    };
    static const uint64_t kPow10i[10] = {
        1ull, 10ull, 100ull, 1000ull, 10000ull,
        100000ull, 1000000ull, 10000000ull, 100000000ull, 1000000000ull
    };

    const double scaled = value * kPow10[decimals];
    const double magnitude = std::floor(std::fabs(scaled) + 0.5);

    // Ueber 2^63 gibt es keine sinnvolle Festkommaausgabe mehr.
    // Kommt in .baf-Daten nicht vor, aber eine kaputte Datei darf
    // hier keinen undefinierten Cast ausloesen.
    if (magnitude >= 9223372036854775808.0) return (value < 0.0) ? "-huge" : "huge";

    const uint64_t units = static_cast<uint64_t>(magnitude);
    const uint64_t whole = units / kPow10i[decimals];
    const uint64_t fraction = units % kPow10i[decimals];

    std::string text;
    // Das Vorzeichen haengt am Eingabewert, nicht am Ergebnis:
    // -0.0000001 soll "-0.000000" ergeben, so wie "%.6f" es auch tut.
    if (std::signbit(value)) text += '-';
    text += std::to_string(whole);
    if (decimals > 0) {
        text += '.';
        std::string digits = std::to_string(fraction);
        text.append(static_cast<size_t>(decimals) - digits.size(), '0');
        text += digits;
    }
    return text;
}

namespace {

std::string V3Text(const Vec3& v) {
    return FormatFloat(v.x) + " " + FormatFloat(v.y) + " " + FormatFloat(v.z);
}

// FNV-1a, 64 Bit. Damit laesst sich ein Pixelblock in einer Zeile
// vergleichen, ohne 512 KB Hexdump - und die Python-Seite rechnet
// dieselbe Zahl aus.
uint64_t Fnv1a(const std::vector<uint8_t>& bytes) {
    uint64_t hash = 1469598103934665603ull;
    for (uint8_t byte : bytes) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string Hex16(uint64_t value) {
    char buffer[17];
    std::snprintf(buffer, sizeof buffer, "%016llx", static_cast<unsigned long long>(value));
    return std::string(buffer);
}

} // namespace

void WriteRestDump(const std::vector<Matrix34>& world,
                   const std::vector<std::string>& names,
                   std::ostream& out) {
    for (size_t i = 0; i < world.size(); ++i) {
        const Matrix34& tm = world[i];
        out << "rest " << i << " " << (i < names.size() ? names[i] : std::string())
            << " r=" << FormatFloat(tm.m[0][0]) << " " << FormatFloat(tm.m[0][1])
            << " " << FormatFloat(tm.m[0][2])
            << " f=" << FormatFloat(tm.m[1][0]) << " " << FormatFloat(tm.m[1][1])
            << " " << FormatFloat(tm.m[1][2])
            << " u=" << FormatFloat(tm.m[2][0]) << " " << FormatFloat(tm.m[2][1])
            << " " << FormatFloat(tm.m[2][2])
            << " t=" << FormatFloat(tm.m[3][0]) << " " << FormatFloat(tm.m[3][1])
            << " " << FormatFloat(tm.m[3][2]) << "\n";
    }
}

void WriteAnimDump(const BafFile& file, size_t clipIndex, double scale,
                   std::ostream& out) {
    if (clipIndex >= file.clips.size()) return;
    const Clip& clip = file.clips[clipIndex];
    out << "anim " << clipIndex << " name=" << clip.name << " fps="
        << FormatFloat(clip.fps) << " frames=" << clip.numFrames
        << " tracks=" << clip.tracks.size() << "\n";
    for (size_t trackIndex = 0; trackIndex < clip.tracks.size(); ++trackIndex) {
        const Track& track = clip.tracks[trackIndex];
        // Bindung ueber den INDEX, nicht ueber den Namen: in
        // einigen Modellen widersprechen sich die beiden.
        const int32_t joint =
            (track.jointIndex >= 0 && static_cast<size_t>(track.jointIndex) < file.nodes.size())
                ? track.jointIndex : static_cast<int32_t>(trackIndex);
        out << "  at " << trackIndex << " joint=" << joint << " name=" << track.name
            << " keys=" << track.keys.size() << "\n";
        for (size_t keyIndex = 0; keyIndex < track.keys.size(); ++keyIndex) {
            const Matrix34 tm = SampleLocalMatrix(
                track, static_cast<double>(track.keys[keyIndex].frame), scale);
            out << "    ak " << keyIndex << " f="
                << FormatFloat(track.keys[keyIndex].frame)
                << " flags=" << track.keys[keyIndex].flags;
            for (int row = 0; row < 4; ++row) {
                for (int column = 0; column < 3; ++column) {
                    out << " " << FormatFloat(tm.m[row][column]);
                }
            }
            out << "\n";
        }
    }
}

void WriteBuildDump(const BuiltMesh& built, std::ostream& out) {
    out << "build " << built.meshIndex << " name=" << built.name
        << " node=" << built.nodeIndex << " drawn=" << (built.drawn ? 1 : 0)
        << " verts=" << built.positions.size() << " tris=" << built.tris.size()
        << " skipped=" << built.skippedFaces.size() << "\n";
    for (size_t i = 0; i < built.positions.size(); ++i) {
        const Vec3& position = built.positions[i];
        out << "  bv " << i << " " << FormatFloat(position.x) << " "
            << FormatFloat(position.y) << " " << FormatFloat(position.z) << "\n";
    }
    for (size_t i = 0; i < built.tris.size(); ++i) {
        const BuiltTri& tri = built.tris[i];
        out << "  bt " << i << " " << tri.vertex[0] << " " << tri.vertex[1] << " "
            << tri.vertex[2] << " mat=" << tri.material << " slot=" << tri.slot
            << " src=" << tri.sourceFace << " uv=";
        for (int k = 0; k < 3; ++k) {
            out << (k ? " " : "") << FormatFloat(tri.uv[k][0]) << " "
                << FormatFloat(tri.uv[k][1]);
        }
        out << "\n";
    }
    for (size_t i = 0; i < built.skippedFaces.size(); ++i) {
        out << "  bs " << built.skippedFaces[i] << "\n";
    }
}

void WriteDump(const BafFile& file, const std::string& label, std::ostream& out,
               uint32_t detail, double scale) {
    out << "BAF " << label << "\n";
    out << "name " << file.name << "\n";
    out << "scale " << FormatFloat(file.scale) << "\n";
    out << "radius " << FormatFloat(file.radiusU) << "\n";
    out << "height " << FormatFloat(file.heightU) << "\n";
    out << "noderefs " << static_cast<int>(file.nodeRefs[0]) << " "
        << static_cast<int>(file.nodeRefs[1]) << " " << static_cast<int>(file.nodeRefs[2])
        << " " << static_cast<int>(file.nodeRefs[3]) << "\n";
    out << "palette " << file.palette.size() << " " << Hex16(Fnv1a(file.palette)) << "\n";
    out << "counts materials=" << file.materials.size() << " meshes=" << file.meshes.size()
        << " nodes=" << file.nodes.size() << " clips=" << file.clips.size() << "\n";

    for (size_t i = 0; i < file.materials.size(); ++i) {
        const Material& material = file.materials[i];
        out << "material " << i << " name=" << material.name << " type=" << material.matType
            << " bits=" << material.bitDepth << " cels=" << material.cels.size()
            << " ownpal=" << material.ownPalette.size() << "\n";
        for (size_t c = 0; c < material.cels.size(); ++c) {
            const Cel& cel = material.cels[c];
            out << "  cel " << c << " " << cel.width << "x" << cel.height
                << " key=" << cel.colorKeyEnable << " mips=" << cel.numMips
                << " bytes=" << cel.pixels.size() << " hash=" << Hex16(Fnv1a(cel.pixels)) << "\n";
        }
    }

    for (size_t i = 0; i < file.nodes.size(); ++i) {
        const Node& node = file.nodes[i];
        out << "node " << i << " name=" << node.name << " flags=" << node.flags
            << " type=" << node.type << " mesh=" << node.meshIndex
            << " parent=" << node.parent << " children=" << node.numChildren
            << " child=" << node.child << " sibling=" << node.sibling
            << " pivot=" << V3Text(node.pivot) << " pos=" << V3Text(node.bindPos)
            << " eul=" << V3Text(node.bindEuler) << "\n";
    }

    if (detail & kDumpRest) {
        std::vector<Matrix34> world;
        ComputeRestWorld(file, scale, world);
        std::vector<std::string> names;
        names.reserve(file.nodes.size());
        for (const Node& node : file.nodes) names.push_back(node.name);
        WriteRestDump(world, names, out);
    }

    for (size_t i = 0; i < file.meshes.size(); ++i) {
        const Mesh& mesh = file.meshes[i];
        out << "mesh " << i << " name=" << mesh.name << " geo=" << mesh.geometryMode
            << " light=" << mesh.lightingMode << " tex=" << mesh.textureMode
            << " verts=" << mesh.positions.size() << " texverts=" << mesh.texVerts.size()
            << " faces=" << mesh.faces.size() << " radius=" << FormatFloat(mesh.radius)
            << " center=" << V3Text(mesh.center) << "\n";
        if (!(detail & kDumpVertices)) continue;
        for (size_t v = 0; v < mesh.positions.size(); ++v) {
            out << "  v " << v << " " << V3Text(mesh.positions[v]) << " n "
                << V3Text(mesh.normals[v]) << " i " << FormatFloat(mesh.intensities[v]) << "\n";
        }
        for (size_t t = 0; t < mesh.texVerts.size(); ++t) {
            out << "  t " << t << " " << FormatFloat(mesh.texVerts[t][0]) << " "
                << FormatFloat(mesh.texVerts[t][1]) << "\n";
        }
        for (size_t f = 0; f < mesh.faces.size(); ++f) {
            const Face& face = mesh.faces[f];
            out << "  f " << f << " mat=" << face.material << " flags=" << face.flags
                << " geo=" << face.geometryMode << " light=" << face.lightMode
                << " tex=" << face.textureMode << " xl=" << FormatFloat(face.extraLight)
                << " n=" << V3Text(face.normal)
                << " loaded=" << face.flagsLoaded << "/" << face.lightModeLoaded << "/"
                << FormatFloat(face.v0Loaded) << " vi=";
            for (size_t k = 0; k < face.vertIdx.size(); ++k) {
                out << (k ? "," : "") << face.vertIdx[k];
            }
            out << " ti=";
            for (size_t k = 0; k < face.texIdx.size(); ++k) {
                out << (k ? "," : "") << face.texIdx[k];
            }
            out << "\n";
        }
    }

    if (detail & kDumpBuild) {
        std::vector<MaterialSlot> slots;
        BuildMaterialSlots(file, slots);
        for (size_t i = 0; i < slots.size(); ++i) {
            const MaterialSlot& slot = slots[i];
            out << "slot " << i << " mat=" << slot.material << " blend=" << slot.blend
                << " twosided=" << (slot.twoSided ? 1 : 0)
                << " fullbright=" << (slot.fullbright ? 1 : 0)
                << " envmap=" << (slot.envMap ? 1 : 0)
                << " alpha=" << FormatFloat(slot.alpha) << "\n";
        }
        for (size_t i = 0; i < file.materials.size(); ++i) {
            for (size_t c = 0; c < file.materials[i].cels.size(); ++c) {
                DecodedCel decoded;
                if (!DecodeCel(file, i, c, decoded)) continue;
                out << "cel " << i << " " << c << " " << decoded.width << "x"
                    << decoded.height << " keyed=" << (decoded.keyed ? 1 : 0)
                    << " rgba=" << Hex16(Fnv1a(decoded.rgba)) << "\n";
            }
        }
        for (size_t i = 0; i < file.meshes.size(); ++i) {
            BuiltMesh built;
            BuildMeshGeometry(file, i, scale, slots, built);
            WriteBuildDump(built, out);
        }
    }

    if (detail & kDumpProps) {
        std::vector<MaterialSlot> propSlots;
        BuildMaterialSlots(file, propSlots);
        out << "props model\n" << ModelPropText(file);
        for (size_t i = 0; i < file.nodes.size(); ++i) {
            out << "props node " << i << "\n" << NodePropText(file, i);
        }
        for (size_t i = 0; i < file.meshes.size(); ++i) {
            BuiltMesh built;
            BuildMeshGeometry(file, i, scale, propSlots, built);
            out << "props mesh " << i << "\n" << MeshPropText(file, i, built);
        }
        for (size_t i = 0; i < file.clips.size(); ++i) {
            out << "props clip " << i << "\n" << ClipPropText(file, i);
        }
        LimitReport limits;
        CheckLimits(file, limits);
        out << "limits exceeded=" << limits.exceeded << "\n" << limits.text;
    }

    if (detail & kDumpAnim) {
        for (size_t i = 0; i < file.clips.size(); ++i) WriteAnimDump(file, i, scale, out);
    }

    for (size_t i = 0; i < file.clips.size(); ++i) {
        const Clip& clip = file.clips[i];
        size_t keyTotal = 0;
        for (const Track& track : clip.tracks) keyTotal += track.keys.size();
        out << "clip " << i << " name=" << clip.name << " fps=" << FormatFloat(clip.fps)
            << " frames=" << clip.numFrames << " fade=" << FormatFloat(clip.fadeSeconds)
            << " loop=" << clip.var1Loop << " var2=" << clip.var2Frame
            << " trackflags=" << clip.trackFlags << " usage=" << clip.specialUsage
            << " mask=" << clip.typeMask << " tracks=" << clip.tracks.size()
            << " keys=" << keyTotal << " events=" << clip.events.size() << "\n";
        for (size_t e = 0; e < clip.events.size(); ++e) {
            const SoundEvent& event = clip.events[e];
            out << "  e " << e << " frame=" << event.frame << " type=" << event.typeBits
                << " wav=" << event.wav << " call=" << event.callFlags
                << " vol=" << FormatFloat(event.volume) << " range=" << FormatFloat(event.rangeU)
                << " min=" << FormatFloat(event.minDistU) << " max=" << FormatFloat(event.maxDistU)
                << " prio=" << event.priority << " id=" << event.soundId << "\n";
        }
        if (!(detail & kDumpKeys)) continue;
        for (size_t t = 0; t < clip.tracks.size(); ++t) {
            const Track& track = clip.tracks[t];
            out << "  track " << t << " name=" << track.name << " joint=" << track.jointIndex
                << " keys=" << track.keys.size() << "\n";
            for (size_t k = 0; k < track.keys.size(); ++k) {
                const Key& key = track.keys[k];
                out << "    k " << k << " f=" << FormatFloat(key.frame)
                    << " flags=" << key.flags << " p=" << V3Text(key.pos)
                    << " o=" << V3Text(key.orient) << " dp=" << V3Text(key.dpos)
                    << " do=" << V3Text(key.dorient) << "\n";
            }
        }
    }
}

std::string MakeSummary(const BafFile& file) {
    size_t vertices = 0, faces = 0, keys = 0;
    for (const Mesh& mesh : file.meshes) {
        vertices += mesh.positions.size();
        faces    += mesh.faces.size();
    }
    for (const Clip& clip : file.clips) {
        for (const Track& track : clip.tracks) keys += track.keys.size();
    }
    std::string text;
    text += file.name.empty() ? file.source : file.name;
    text += "\n  Knoten:      " + std::to_string(file.nodes.size());
    text += "\n  Meshes:      " + std::to_string(file.meshes.size());
    text += "\n  Vertices:    " + std::to_string(vertices);
    text += "\n  Faces:       " + std::to_string(faces);
    text += "\n  Materialien: " + std::to_string(file.materials.size());
    text += "\n  Clips:       " + std::to_string(file.clips.size());
    text += "\n  Keyframes:   " + std::to_string(keys);
    text += "\n  Skalierung:  " + FormatFloat(file.scale);
    return text;
}

} // namespace baf
