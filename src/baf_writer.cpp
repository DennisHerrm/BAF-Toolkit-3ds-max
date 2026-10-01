// ============================================================
//  .baf Schreiber
//
//  Der Weg zurueck. Die entscheidende Entscheidung steht gleich
//  am Anfang und praegt alles Weitere:
//
//  ES WIRD GEPATCHT, NICHT NEU GEBAUT.
//
//  Jeder Satz der Datei wird als Rohbytes behalten, und der
//  Schreiber legt nur die dekodierten Felder wieder hinein. Der
//  Rest bleibt Byte fuer Byte stehen: alte Zeiger, die die Engine
//  beim Laden ohnehin ueberschreibt, die Fuellbytes des
//  Compilers von 1999, der Muell hinter der NUL in einem
//  Namensfeld.
//
//  Wer stattdessen aus den dekodierten Feldern eine neue Datei
//  zusammensetzt, bekommt eine Datei, die vielleicht laedt - aber
//  nicht mehr dieselbe ist. Und dann laesst sich nicht mehr
//  unterscheiden, ob eine Aenderung gewollt war oder ein Fehler
//  im Schreiber. Mit dem Patchen gilt: hat sich nichts geaendert,
//  ist die Datei byte-gleich. Alles andere ist ein Fund.
//
//  Neu gerechnet werden nur die Groessen und Offsets, die von
//  den Daten abhaengen - Blockgroessen, Zaehler, die laufende
//  Summe der Sound-Events.
//
//  WICHTIG: haengt bewusst NICHT am 3ds Max SDK. Siehe die
//  gleiche Notiz in baf_lab.h.
// ============================================================

#include "baf_reader.h"

#include <cstring>
#include <string>
#include <vector>

namespace baf {
namespace {

// ------------------------------------------------------------
//  Ein Satz zum Beschreiben.
//
//  Ist die Rohvorlage nicht genau so gross wie erwartet - etwa
//  weil der Satz in Max neu entstanden ist - wird mit Nullen
//  angefangen. Das ist der einzige Fall, in dem der Schreiber
//  Bytes erfindet, und er faellt beim Byte-Vergleich sofort auf.
// ------------------------------------------------------------
class Record {
public:
    Record(const std::vector<uint8_t>& raw, size_t size)
        : bytes_((raw.size() == size) ? raw : std::vector<uint8_t>(size, 0)) {}

    void U32(size_t offset, uint32_t value) { Put(offset, &value, sizeof value); }
    void I32(size_t offset, int32_t value)  { Put(offset, &value, sizeof value); }
    void U16(size_t offset, uint16_t value) { Put(offset, &value, sizeof value); }
    void F32(size_t offset, float value)    { Put(offset, &value, sizeof value); }

    void V3(size_t offset, const Vec3& value) {
        F32(offset + 0, value.x);
        F32(offset + 4, value.y);
        F32(offset + 8, value.z);
    }

    void Tag(size_t offset, const char* text) { Put(offset, text, 4); }

    // ------------------------------------------------------
    //  NUL-TERMINIERT, nicht NUL-GEFUELLT.
    //
    //  Hinter der NUL steht uninitialisierter Speicher von 1999,
    //  und der gehoert zur Byte-Gleichheit. Wer das Feld
    //  auffuellt, aendert Dateien, an denen niemand etwas
    //  geaendert hat.
    // ------------------------------------------------------
    void Str(size_t offset, size_t fieldWidth, const std::string& text) {
        if (fieldWidth == 0 || offset + fieldWidth > bytes_.size()) return;
        const size_t length = (text.size() < fieldWidth - 1) ? text.size() : fieldWidth - 1;
        if (length) std::memcpy(bytes_.data() + offset, text.data(), length);
        bytes_[offset + length] = 0;
    }

    const std::vector<uint8_t>& Bytes() const { return bytes_; }

private:
    void Put(size_t offset, const void* source, size_t count) {
        if (offset + count > bytes_.size()) return;
        std::memcpy(bytes_.data() + offset, source, count);
    }
    std::vector<uint8_t> bytes_;
};

void Append(std::vector<uint8_t>& out, const std::vector<uint8_t>& part) {
    out.insert(out.end(), part.begin(), part.end());
}

void AppendF32(std::vector<uint8_t>& out, float value) {
    uint8_t buffer[4];
    std::memcpy(buffer, &value, 4);
    out.insert(out.end(), buffer, buffer + 4);
}

void AppendV3(std::vector<uint8_t>& out, const Vec3& value) {
    AppendF32(out, value.x);
    AppendF32(out, value.y);
    AppendF32(out, value.z);
}

void AppendU32(std::vector<uint8_t>& out, uint32_t value) {
    uint8_t buffer[4];
    std::memcpy(buffer, &value, 4);
    out.insert(out.end(), buffer, buffer + 4);
}

} // namespace

size_t FirstDifference(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    const size_t shared = (a.size() < b.size()) ? a.size() : b.size();
    for (size_t i = 0; i < shared; ++i) {
        if (a[i] != b[i]) return i;
    }
    return (a.size() == b.size()) ? SIZE_MAX : shared;
}

bool BuildBaf(const BafFile& file, std::vector<uint8_t>& out, std::string& error) {
    out.clear();
    error.clear();

    // ---- Materialblock ----
    std::vector<uint8_t> materialBlock;
    for (const Material& material : file.materials) {
        Record name(material.rawName, 0x20);
        name.Str(0, 0x20, material.name);
        Append(materialBlock, name.Bytes());

        Record header(material.rawHeader, kMatHeaderBytes);
        header.Tag(0x00, "MAT ");
        header.U32(0x04, 50);
        header.U32(0x08, material.matType);
        // Die Zahl steht zweimal im Kopf. Beide Stellen setzen -
        // der Loader liest die zweite, der Parser prueft die erste.
        header.U32(0x0c, static_cast<uint32_t>(material.cels.size()));
        header.U32(0x10, static_cast<uint32_t>(material.cels.size()));
        header.U32(0x18, material.bitDepth);
        Append(materialBlock, header.Bytes());

        Append(materialBlock, material.rawRecords);

        for (const Cel& cel : material.cels) {
            Record celRecord(cel.raw, kMatCelBytes);
            celRecord.U32(0x00, cel.width);
            celRecord.U32(0x04, cel.height);
            celRecord.U32(0x08, cel.colorKeyEnable);
            celRecord.U32(0x14, cel.numMips);
            Append(materialBlock, celRecord.Bytes());
            Append(materialBlock, cel.pixels);
        }
        if (material.matType & kMatOwnPalette) {
            if (material.ownPalette.size() == 768) {
                Append(materialBlock, material.ownPalette);
            } else {
                materialBlock.insert(materialBlock.end(), 768, 0);
            }
        }
    }

    // ---- Geometrieblock ----
    std::vector<uint8_t> meshRecords, meshData;
    for (size_t meshIndex = 0; meshIndex < file.meshes.size(); ++meshIndex) {
        const Mesh& mesh = file.meshes[meshIndex];
        const uint32_t vertCount    = static_cast<uint32_t>(mesh.positions.size());
        const uint32_t texVertCount = static_cast<uint32_t>(mesh.texVerts.size());
        const uint32_t faceCount    = static_cast<uint32_t>(mesh.faces.size());

        if (mesh.normals.size() != mesh.positions.size() ||
            mesh.intensities.size() != mesh.positions.size()) {
            error = "Mesh '" + mesh.name +
                    "': Normalen und Intensitaeten muessen so viele Eintraege haben "
                    "wie es Vertices gibt";
            return false;
        }

        Record record(mesh.raw, kMeshBytes);
        record.Str(0x00, 0x20, mesh.name);
        record.I32(0x20, static_cast<int32_t>(meshIndex));
        record.U32(0x24, mesh.geometryMode);
        record.U32(0x28, mesh.lightingMode);
        record.U32(0x2c, mesh.textureMode);
        record.U32(0x48, vertCount);
        record.U32(0x4c, texVertCount);
        record.U32(0x50, faceCount);
        record.F32(0x54, mesh.radius);
        record.V3(0x58, mesh.center);
        Append(meshRecords, record.Bytes());

        // Die Reihenfolge ist die des Loaders und nicht verhandelbar:
        // Positionen, Normalen, Intensitaeten, totes Feld, UVs,
        // danach alle Face-Saetze und erst dann die Indexbloecke.
        std::vector<uint8_t> arrays;
        for (const Vec3& position : mesh.positions) AppendV3(arrays, position);
        for (const Vec3& normal : mesh.normals)     AppendV3(arrays, normal);
        for (float intensity : mesh.intensities)    AppendF32(arrays, intensity);
        if (mesh.dead.size() == static_cast<size_t>(vertCount) * 4) {
            Append(arrays, mesh.dead);
        } else {
            arrays.insert(arrays.end(), static_cast<size_t>(vertCount) * 4, 0);
        }
        for (const std::array<float, 2>& texel : mesh.texVerts) {
            AppendF32(arrays, texel[0]);
            AppendF32(arrays, texel[1]);
        }

        std::vector<uint8_t> indices;
        for (const Face& face : mesh.faces) {
            const uint32_t faceVerts = static_cast<uint32_t>(face.vertIdx.size());
            Record faceRecord(face.raw, kFaceBytes);
            faceRecord.U32(0x00, face.material);
            faceRecord.U32(0x04, face.flags);
            faceRecord.U32(0x08, face.geometryMode);
            faceRecord.U32(0x0c, face.lightMode);
            faceRecord.U32(0x10, face.textureMode);
            faceRecord.U32(0x14, faceVerts);
            faceRecord.F32(0x30, face.extraLight);
            // v0, NICHT v0Loaded: in der Datei steht der Rohwert,
            // nicht das Ergebnis des zweiten Ladedurchgangs.
            faceRecord.F32(0x34, face.v0);
            faceRecord.V3(0x38, face.normal);
            Append(arrays, faceRecord.Bytes());

            for (uint32_t index : face.vertIdx) AppendU32(indices, index);
            // Dieselbe Bedingung wie beim Lesen, wortwoertlich:
            // ohne sie wandert der Indexblock um texIdx-Laenge und
            // alles danach ist verschoben.
            if (face.material != 0xffffu) {
                if (face.material >= file.materials.size()) {
                    error = "Mesh '" + mesh.name + "': Face nennt Material " +
                            std::to_string(face.material) + ", die Datei hat " +
                            std::to_string(file.materials.size());
                    return false;
                }
                if (file.materials[face.material].matType & kMatTextured) {
                    if (face.texIdx.size() != face.vertIdx.size()) {
                        error = "Mesh '" + mesh.name +
                                "': texturiertes Face ohne passende Texturindizes";
                        return false;
                    }
                    for (uint32_t index : face.texIdx) AppendU32(indices, index);
                }
            }
        }
        Append(meshData, arrays);
        Append(meshData, indices);
    }

    std::vector<uint8_t> nodeBlock;
    for (size_t i = 0; i < file.nodes.size(); ++i) {
        const Node& node = file.nodes[i];
        Record record(node.raw, kNodeBytes);
        record.Str(0x00, 0x40, node.name);
        record.U32(0x40, node.flags);
        record.I32(0x44, static_cast<int32_t>(i));
        record.U32(0x48, node.type);
        record.I32(0x4c, node.meshIndex);
        record.I32(0x50, node.parent);
        record.I32(0x54, node.numChildren);
        record.I32(0x58, node.child);
        record.I32(0x5c, node.sibling);
        record.V3(0x60, node.pivot);
        record.V3(0x6c, node.bindPos);
        record.V3(0x78, node.bindEuler);
        Append(nodeBlock, record.Bytes());
    }

    Record geoHeader(file.rawGeoHeader, kGeoHeaderBytes);
    geoHeader.U32(0x24, static_cast<uint32_t>(file.meshes.size()));
    geoHeader.U32(0x44, 1);   // ein Geoset; der Leser lehnt alles andere ab
    geoHeader.U32(0x4c, static_cast<uint32_t>(file.materials.size()));
    geoHeader.U32(0x54, static_cast<uint32_t>(file.nodes.size()));

    std::vector<uint8_t> geoBlock = geoHeader.Bytes();
    Append(geoBlock, meshRecords);
    Append(geoBlock, meshData);
    Append(geoBlock, nodeBlock);

    // ---- Animationsblock ----
    std::vector<uint8_t> animBlock, eventBlock;
    uint32_t eventFirst = 0;
    for (const Clip& clip : file.clips) {
        std::vector<uint8_t> body;

        Record keyHeader(clip.rawKeyHdr, kKeyHdrBytes);
        keyHeader.Str(0x00, 0x24, clip.name);
        // Die Trackanzahl steht zweimal. +0x38 ist die, die der
        // Loader liest; +0x28 wird mitgeschrieben, damit die Datei
        // in sich stimmig bleibt.
        keyHeader.I32(0x28, static_cast<int32_t>(clip.tracks.size()));
        keyHeader.U32(0x2c, clip.typeMask);
        keyHeader.F32(0x30, clip.fps);
        keyHeader.I32(0x34, clip.numFrames);
        keyHeader.I32(0x38, static_cast<int32_t>(clip.tracks.size()));
        Append(body, keyHeader.Bytes());

        // Erst ALLE Track-Koepfe, dann alle Keys. Nicht abwechselnd.
        for (const Track& track : clip.tracks) {
            Record record(track.raw, kKeyNodeBytes);
            record.Str(0x00, 0x20, track.name);
            record.I32(0x20, track.jointIndex);
            record.I32(0x24, static_cast<int32_t>(track.keys.size()));
            Append(body, record.Bytes());
        }
        for (const Track& track : clip.tracks) {
            for (const Key& key : track.keys) {
                Record record(key.raw, kKeyBytes);
                record.F32(0x00, key.frame);
                record.U32(0x04, key.flags);
                record.V3(0x08, key.pos);
                record.V3(0x14, key.orient);
                record.V3(0x20, key.dpos);
                record.V3(0x2c, key.dorient);
                Append(body, record.Bytes());
            }
        }

        Record descriptor(clip.rawDesc, kDescBytes);
        descriptor.U32(0x00, static_cast<uint32_t>(kDescBytes + body.size()));
        descriptor.F32(0x08, clip.fadeSeconds);
        descriptor.U32(0x0c, clip.var1Loop);
        descriptor.U32(0x10, clip.var2Frame);
        descriptor.U32(0x14, clip.trackFlags);
        descriptor.U32(0x18, clip.specialUsage);
        descriptor.U32(0x1c, static_cast<uint32_t>(clip.events.size()));
        // Die laufende Summe, nicht die Nummer des Clips. Der
        // Leser prueft genau das.
        descriptor.U32(0x20, eventFirst);
        Append(animBlock, descriptor.Bytes());
        Append(animBlock, body);

        eventFirst += static_cast<uint32_t>(clip.events.size());

        for (const SoundEvent& event : clip.events) {
            Record record(event.raw, kEventBytes);
            record.U32(0x00, event.frame);
            record.U32(0x04, event.typeBits);
            record.Str(0x08, 0x10, event.wav);
            record.U32(0x20, event.callFlags);
            record.F32(0x24, event.volume);
            record.F32(0x2c, event.rangeU);
            record.F32(0x30, event.minDistU);
            record.F32(0x34, event.maxDistU);
            record.U32(0x38, event.priority);
            record.I32(0x3c, event.soundId);
            Append(eventBlock, record.Bytes());
        }
    }
    // Die Event-Tabelle steht als Ganzes hinter ALLEN Clips, nicht
    // je Clip dahinter.
    Append(animBlock, eventBlock);

    // ---- Kopf ----
    const size_t paletteBytes  = file.paletteBlock.size();
    const size_t materialBytes = materialBlock.size();
    const size_t geoBytes      = geoBlock.size();
    const size_t animBytes     = animBlock.size();

    Record header(file.rawHeader, kHeaderBytes);
    header.U32(0x04, static_cast<uint32_t>(kHeaderBytes));
    header.U32(0xc0, static_cast<uint32_t>(geoBytes));
    header.U32(0xc4, static_cast<uint32_t>(file.materials.size()));
    header.U32(0xc8, static_cast<uint32_t>(file.clips.size()));
    header.U16(0xcc, paletteBytes ? 1 : 0);
    header.U32(0xd0, static_cast<uint32_t>(materialBytes));
    header.U32(0xd4, static_cast<uint32_t>(animBytes));
    header.U32(0xdc, static_cast<uint32_t>(paletteBytes));
    // Die beiden Offsets sind gespiegelte Summen der Blockgroessen.
    // Der Leser prueft sie gegeneinander, also muessen sie hier
    // aus denselben Zahlen entstehen.
    header.U32(0xe0, static_cast<uint32_t>(paletteBytes + materialBytes));
    header.U32(0xe4, static_cast<uint32_t>(paletteBytes + materialBytes + geoBytes));

    out.reserve(kHeaderBytes + paletteBytes + materialBytes + geoBytes + animBytes);
    Append(out, header.Bytes());
    Append(out, file.paletteBlock);
    Append(out, materialBlock);
    Append(out, geoBlock);
    Append(out, animBlock);
    return true;
}

} // namespace baf
