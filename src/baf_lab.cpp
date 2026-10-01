#include "baf_lab.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace baf {
namespace {

// Kleiner Helfer: u32 aus einem Puffer, ohne Zeigerarithmetik im
// Aufrufer und ohne reinterpret_cast auf uint32_t* (Alignment und
// Aliasing sind sonst formal undefiniert).
uint32_t ReadU32(const std::vector<uint8_t>& buf, size_t offset) {
    uint32_t value = 0;
    std::memcpy(&value, buf.data() + offset, sizeof value);
    return value;   // Datei ist Little-Endian, Max ist x64 - passt
}

std::string ToLower(std::string text) {
    for (char& character : text) {
        if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + 32);
    }
    return text;
}

std::string BaseName(const std::string& path) {
    const size_t cut = path.find_last_of("/\\");
    return (cut == std::string::npos) ? path : path.substr(cut + 1);
}

} // namespace

// ------------------------------------------------------------
//  UTF-8 -> UTF-16, von Hand.
//
//  Absichtlich ohne MultiByteToWideChar: dann braucht diese Datei
//  kein <windows.h> und baut weiter unter Linux, wo bafdump zum
//  Gegenpruefen laeuft.
//
//  Ungueltige Bytes werden zu U+FFFD statt zu einem Fehler - ein
//  leicht falscher Pfad scheitert gleich danach beim Oeffnen und
//  meldet sich dort, das ist die verstaendlichere Stelle dafuer.
// ------------------------------------------------------------
#if defined(_WIN32)
namespace {

// UTF-8 nach UTF-16, gemeinsam fuer Lesen und Schreiben.
std::wstring Utf8ToWide(const std::string& utf8Path) {
    std::wstring wide;
    wide.reserve(utf8Path.size());
    for (size_t i = 0; i < utf8Path.size();) {
        const unsigned char lead = static_cast<unsigned char>(utf8Path[i]);
        uint32_t code = 0xFFFD;
        size_t length = 1;

        if (lead < 0x80)                        { code = lead;          length = 1; }
        else if ((lead & 0xE0) == 0xC0)         { code = lead & 0x1Fu;  length = 2; }
        else if ((lead & 0xF0) == 0xE0)         { code = lead & 0x0Fu;  length = 3; }
        else if ((lead & 0xF8) == 0xF0)         { code = lead & 0x07u;  length = 4; }

        if (length > 1 && i + length <= utf8Path.size()) {
            for (size_t k = 1; k < length; ++k) {
                const unsigned char cont = static_cast<unsigned char>(utf8Path[i + k]);
                if ((cont & 0xC0) != 0x80) { code = 0xFFFD; length = 1; break; }
                code = (code << 6) | (cont & 0x3Fu);
            }
        } else if (length > 1) {
            code = 0xFFFD; length = 1;
        }

        if (code >= 0x10000u && code <= 0x10FFFFu) {
            const uint32_t rest = code - 0x10000u;
            wide.push_back(static_cast<wchar_t>(0xD800u + (rest >> 10)));
            wide.push_back(static_cast<wchar_t>(0xDC00u + (rest & 0x3FFu)));
        } else {
            wide.push_back(static_cast<wchar_t>(code <= 0xFFFFu ? code : 0xFFFDu));
        }
        i += length;
    }

    return wide;
}

} // namespace
#endif

std::FILE* OpenBinaryRead(const std::string& utf8Path) {
#if defined(_WIN32)
    std::FILE* file = nullptr;
    if (_wfopen_s(&file, Utf8ToWide(utf8Path).c_str(), L"rb") != 0) return nullptr;
    return file;
#else
    return std::fopen(utf8Path.c_str(), "rb");
#endif
}

std::FILE* OpenBinaryWrite(const std::string& utf8Path) {
#if defined(_WIN32)
    std::FILE* file = nullptr;
    if (_wfopen_s(&file, Utf8ToWide(utf8Path).c_str(), L"wb") != 0) return nullptr;
    return file;
#else
    return std::fopen(utf8Path.c_str(), "wb");
#endif
}

LabResult LabArchive::Open(const std::string& path) {
    Close();
    LabResult result;

    // "rb" - unter Windows sonst CRLF-Uebersetzung mitten in
    // Binaerdaten. Klassisches FILE* statt ifstream, weil das
    // Plugin sonst <iostream> mit in die DLL zieht.
    std::FILE* file = OpenBinaryRead(path);
    if (!file) {
        result.error = path + ": laesst sich nicht oeffnen";
        return result;
    }

    std::vector<uint8_t> header(kLabHeaderBytes);
    if (std::fread(header.data(), 1, kLabHeaderBytes, file) != kLabHeaderBytes ||
        std::memcmp(header.data(), "LABN", 4) != 0) {
        std::fclose(file);
        result.error = path + ": kein 'LABN' an Offset 0, also kein .lab-Archiv";
        return result;
    }

    fieldA_             = ReadU32(header, 0x04);
    const uint32_t count = ReadU32(header, 0x08);
    namePoolSize_        = ReadU32(header, 0x0c);

    // Plausibilitaet VOR dem Reservieren: eine kaputte Zahl aus
    // der Datei darf hier keinen 4-GB-Allokationsversuch ausloesen.
    // big.lab hat 1337 Eintraege und 16 KB Namenspool.
    if (count > 1000000u || namePoolSize_ > 64u * 1024u * 1024u) {
        std::fclose(file);
        result.error = path + ": Inhaltsverzeichnis behauptet unplausible Groessen";
        return result;
    }

    std::vector<uint8_t> records(static_cast<size_t>(count) * kLabRecordBytes);
    std::vector<uint8_t> pool(namePoolSize_);
    const bool readOk =
        (records.empty() || std::fread(records.data(), 1, records.size(), file) == records.size()) &&
        (pool.empty()    || std::fread(pool.data(),    1, pool.size(),    file) == pool.size());
    std::fclose(file);

    if (!readOk) {
        result.error = path + ": Datei ist kuerzer als ihr Inhaltsverzeichnis behauptet";
        return result;
    }

    entries_.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        const size_t base = static_cast<size_t>(i) * kLabRecordBytes;
        LabEntry entry;
        entry.nameOffset = ReadU32(records, base + 0x00);
        entry.dataOffset = ReadU32(records, base + 0x04);
        entry.size       = ReadU32(records, base + 0x08);
        entry.typeFourcc = ReadU32(records, base + 0x0c);

        // Namen aus dem Pool schneiden. Ein Offset hinter dem Pool
        // ist ein leerer Name, kein Lesefehler - so macht es die
        // Python-Seite auch, und ein Eintrag mit kaputtem Namen
        // soll nicht das ganze Archiv unbrauchbar machen.
        if (entry.nameOffset < pool.size()) {
            const char* start = reinterpret_cast<const char*>(pool.data()) + entry.nameOffset;
            const size_t maxLen = pool.size() - entry.nameOffset;
            const size_t length = ::strnlen(start, maxLen);
            entry.name.assign(start, length);
        }
        entries_.push_back(std::move(entry));
    }

    path_ = path;
    open_ = true;
    result.ok = true;
    return result;
}

void LabArchive::Close() {
    open_ = false;
    path_.clear();
    entries_.clear();
    fieldA_ = 0;
    namePoolSize_ = 0;
}

const LabEntry* LabArchive::Find(const std::string& filename) const {
    const std::string wanted = ToLower(BaseName(filename));
    for (const LabEntry& entry : entries_) {
        if (ToLower(entry.name) == wanted) return &entry;
    }
    return nullptr;
}

bool LabArchive::ReadBytes(const LabEntry& entry, std::vector<uint8_t>& out,
                           std::string& error) const {
    out.clear();
    if (!open_) { error = "Archiv ist nicht geoeffnet"; return false; }
    if (entry.size == 0) return true;
    if (entry.size > 256u * 1024u * 1024u) {
        error = entry.name + ": Eintrag behauptet " + std::to_string(entry.size) + " Bytes";
        return false;
    }

    std::FILE* file = OpenBinaryRead(path_);
    if (!file) { error = path_ + ": laesst sich nicht oeffnen"; return false; }

    out.resize(entry.size);
    const bool ok =
#if defined(_MSC_VER)
        _fseeki64(file, static_cast<int64_t>(entry.dataOffset), SEEK_SET) == 0 &&
#else
        std::fseek(file, static_cast<long>(entry.dataOffset), SEEK_SET) == 0 &&
#endif
        std::fread(out.data(), 1, out.size(), file) == out.size();
    std::fclose(file);

    if (!ok) {
        out.clear();
        error = entry.name + ": abgeschnitten, " + std::to_string(entry.size) +
                " Bytes ab Offset " + std::to_string(entry.dataOffset) + " nicht lesbar";
        return false;
    }
    return true;
}

namespace {

// Datei stueckweise kopieren. Nicht am Stueck: big.lab ist
// 115 MB, und die auf einmal in den Speicher zu holen, nur um
// sie gleich wieder wegzuschreiben, waere unnoetig.
bool CopyFileChunked(const std::string& from, const std::string& to, std::string& error) {
    std::FILE* source = OpenBinaryRead(from);
    if (source == nullptr) { error = from + ": laesst sich nicht lesen"; return false; }
    std::FILE* target = OpenBinaryWrite(to);
    if (target == nullptr) {
        std::fclose(source);
        error = to + ": laesst sich nicht schreiben";
        return false;
    }
    std::vector<uint8_t> buffer(1u << 20);
    bool ok = true;
    for (;;) {
        const size_t got = std::fread(buffer.data(), 1, buffer.size(), source);
        if (got == 0) break;
        if (std::fwrite(buffer.data(), 1, got, target) != got) { ok = false; break; }
    }
    if (std::ferror(source)) ok = false;
    std::fclose(source);
    if (std::fclose(target) != 0) ok = false;
    if (!ok) error = to + ": Kopieren abgebrochen - Platte voll?";
    return ok;
}

bool FileExists(const std::string& path) {
    std::FILE* handle = OpenBinaryRead(path);
    if (handle == nullptr) return false;
    std::fclose(handle);
    return true;
}

// Zwei Dateien Byte fuer Byte vergleichen. Die Gegenprobe nach
// dem Schreiben - ohne sie waere "geschrieben" nur eine
// Behauptung.
bool FilesEqual(const std::string& a, const std::string& b, std::string& error) {
    std::FILE* first = OpenBinaryRead(a);
    std::FILE* second = OpenBinaryRead(b);
    if (first == nullptr || second == nullptr) {
        if (first) std::fclose(first);
        if (second) std::fclose(second);
        error = "Gegenprobe: eine der Dateien laesst sich nicht oeffnen";
        return false;
    }
    std::vector<uint8_t> bufferA(1u << 20), bufferB(1u << 20);
    bool equal = true;
    size_t position = 0;
    for (;;) {
        const size_t gotA = std::fread(bufferA.data(), 1, bufferA.size(), first);
        const size_t gotB = std::fread(bufferB.data(), 1, bufferB.size(), second);
        if (gotA != gotB) { equal = false; break; }
        if (gotA == 0) break;
        if (std::memcmp(bufferA.data(), bufferB.data(), gotA) != 0) {
            for (size_t i = 0; i < gotA; ++i) {
                if (bufferA[i] != bufferB[i]) { position += i; break; }
            }
            error = "Gegenprobe: erste Abweichung bei Offset " + std::to_string(position);
            equal = false;
            break;
        }
        position += gotA;
    }
    std::fclose(first);
    std::fclose(second);
    if (!equal && error.empty()) error = "Gegenprobe: die Dateien sind verschieden lang";
    return equal;
}

std::string BackupPathOf(const std::string& archivePath) {
    return archivePath + ".bak";
}

} // namespace

LabWriteResult ReplaceEntry(const std::string& archivePath,
                            const std::string& entryName,
                            const std::vector<uint8_t>& data,
                            bool makeBackup) {
    LabWriteResult result;

    LabArchive archive;
    const LabResult opened = archive.Open(archivePath);
    if (!opened.ok) { result.error = opened.error; return result; }

    const LabEntry* entry = archive.Find(entryName);
    if (entry == nullptr) {
        result.error = entryName + ": nicht im Archiv - neue Eintraege anzulegen "
                       "kann diese Fassung noch nicht";
        return result;
    }
    if (data.size() > 0x7FFFFFFFu) {
        result.error = entryName + ": zu gross fuer einen 32-Bit-Groessenwert";
        return result;
    }

    // ---- 1. Sicherung ----
    const std::string backup = BackupPathOf(archivePath);
    if (makeBackup && !FileExists(backup)) {
        std::string error;
        if (!CopyFileChunked(archivePath, backup, error)) { result.error = error; return result; }
        result.report += "Sicherung angelegt: " + backup + "\n";
    } else if (makeBackup) {
        // Eine vorhandene Sicherung bleibt stehen. Sie zu
        // ueberschreiben wuerde genau den Stand vernichten, zu dem
        // man zurueckwill - naemlich den unveraenderten.
        result.report += "Sicherung vorhanden, bleibt unberuehrt: " + backup + "\n";
    }

    const uint32_t oldSize = entry->size;
    const uint32_t oldOffset = entry->dataOffset;

    // ---- 2a. Gleiche Groesse: an Ort und Stelle ----
    if (data.size() == oldSize) {
        std::FILE* handle = nullptr;
#if defined(_WIN32)
        if (_wfopen_s(&handle, Utf8ToWide(archivePath).c_str(), L"r+b") != 0) handle = nullptr;
#else
        handle = std::fopen(archivePath.c_str(), "r+b");
#endif
        if (handle == nullptr) {
            result.error = archivePath + ": laesst sich nicht zum Aendern oeffnen";
            return result;
        }
        bool ok =
#if defined(_MSC_VER)
            _fseeki64(handle, static_cast<int64_t>(oldOffset), SEEK_SET) == 0 &&
#else
            std::fseek(handle, static_cast<long>(oldOffset), SEEK_SET) == 0 &&
#endif
            (data.empty() || std::fwrite(data.data(), 1, data.size(), handle) == data.size());
        if (std::fclose(handle) != 0) ok = false;
        if (!ok) {
            result.error = archivePath + ": Schreiben abgebrochen. Die Sicherung "
                           "liegt unter " + backup;
            return result;
        }

        // ---- 3. Gegenprobe ----
        LabArchive check;
        const LabResult reopened = check.Open(archivePath);
        std::vector<uint8_t> back;
        std::string readError;
        const LabEntry* again = reopened.ok ? check.Find(entryName) : nullptr;
        if (!reopened.ok || again == nullptr ||
            !check.ReadBytes(*again, back, readError) || back != data) {
            result.error = "Gegenprobe fehlgeschlagen. Die Sicherung liegt unter " + backup;
            return result;
        }
        result.ok = true;
        result.report += "An Ort und Stelle ersetzt: " + std::to_string(data.size()) +
                         " Byte an Offset " + std::to_string(oldOffset) +
                         ", Gegenprobe bestanden\n";
        return result;
    }

    // ---- 2b. Andere Groesse: das Archiv neu aufbauen ----
    //
    // Erst in eine Nebendatei. Erst wenn die Gegenprobe stimmt,
    // tritt sie an die Stelle des Originals - ein Abbruch
    // mittendrin laesst dann eine unfertige Nebendatei zurueck,
    // aber ein heiles Archiv.
    const std::string temporary = archivePath + ".neu";
    const std::vector<LabEntry>& entries = archive.Entries();

    // Der Namenspool bleibt unveraendert: es kommen keine Namen
    // hinzu und es faellt keiner weg.
    std::vector<uint8_t> header;
    {
        std::FILE* source = OpenBinaryRead(archivePath);
        if (source == nullptr) { result.error = archivePath + ": nicht lesbar"; return result; }
        const size_t tocBytes = kLabHeaderBytes +
            entries.size() * kLabRecordBytes;
        // Kopf, Saetze und Namenspool am Stueck einlesen: der
        // erste Datenoffset markiert das Ende des Verzeichnisses.
        uint32_t firstData = 0xFFFFFFFFu;
        for (const LabEntry& item : entries) {
            if (item.dataOffset < firstData) firstData = item.dataOffset;
        }
        if (firstData == 0xFFFFFFFFu || firstData < tocBytes) firstData = static_cast<uint32_t>(tocBytes);
        header.resize(firstData);
        const bool ok = std::fread(header.data(), 1, header.size(), source) == header.size();
        std::fclose(source);
        if (!ok) { result.error = archivePath + ": Verzeichnis nicht vollstaendig lesbar"; return result; }
    }

    // Neue Offsets: lueckenlos in der Reihenfolge der alten
    // Offsets, so wie die Retail-Datei es auch haelt.
    std::vector<size_t> order(entries.size());
    for (size_t i = 0; i < order.size(); ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&entries](size_t a, size_t b) {
        return entries[a].dataOffset < entries[b].dataOffset;
    });

    std::vector<uint32_t> newOffset(entries.size(), 0);
    std::vector<uint32_t> newSize(entries.size(), 0);
    uint64_t cursor = header.size();
    for (size_t index : order) {
        const bool isTarget = (&entries[index] == entry);
        const uint32_t size = isTarget ? static_cast<uint32_t>(data.size()) : entries[index].size;
        newOffset[index] = static_cast<uint32_t>(cursor);
        newSize[index] = size;
        cursor += size;
        if (cursor > 0xFFFFFFFFull) {
            result.error = "Das Archiv wuerde ueber 4 GB wachsen";
            return result;
        }
    }

    for (size_t i = 0; i < entries.size(); ++i) {
        const size_t base = kLabHeaderBytes + i * kLabRecordBytes;
        std::memcpy(header.data() + base + 0x04, &newOffset[i], 4);
        std::memcpy(header.data() + base + 0x08, &newSize[i], 4);
    }

    {
        std::FILE* source = OpenBinaryRead(archivePath);
        std::FILE* target = OpenBinaryWrite(temporary);
        if (source == nullptr || target == nullptr) {
            if (source) std::fclose(source);
            if (target) std::fclose(target);
            result.error = temporary + ": laesst sich nicht anlegen";
            return result;
        }
        bool ok = std::fwrite(header.data(), 1, header.size(), target) == header.size();
        std::vector<uint8_t> buffer(1u << 20);
        for (size_t index : order) {
            if (!ok) break;
            if (&entries[index] == entry) {
                ok = data.empty() ||
                     std::fwrite(data.data(), 1, data.size(), target) == data.size();
                continue;
            }
            if (std::fseek(source, static_cast<long>(entries[index].dataOffset), SEEK_SET) != 0) {
                ok = false;
                break;
            }
            size_t left = entries[index].size;
            while (left > 0 && ok) {
                const size_t chunk = (left < buffer.size()) ? left : buffer.size();
                ok = std::fread(buffer.data(), 1, chunk, source) == chunk &&
                     std::fwrite(buffer.data(), 1, chunk, target) == chunk;
                left -= chunk;
            }
        }
        std::fclose(source);
        if (std::fclose(target) != 0) ok = false;
        if (!ok) {
            result.error = "Neuaufbau abgebrochen. Das Original ist unberuehrt, die "
                           "unfertige Datei liegt unter " + temporary;
            return result;
        }
    }

    // ---- 3. Gegenprobe an der NEBENDATEI, vor dem Tausch ----
    {
        LabArchive check;
        const LabResult reopened = check.Open(temporary);
        if (!reopened.ok) {
            result.error = "Gegenprobe: die neue Datei laesst sich nicht lesen. Das "
                           "Original ist unberuehrt.";
            return result;
        }
        if (check.Count() != archive.Count()) {
            result.error = "Gegenprobe: die neue Datei hat eine andere Eintragszahl. "
                           "Das Original ist unberuehrt.";
            return result;
        }
        // Jeden Eintrag lesen und gegen den alten halten - das ist
        // der Punkt, an dem ein verrutschter Offset auffaellt.
        std::vector<uint8_t> before, after;
        std::string readError;
        for (const LabEntry& item : archive.Entries()) {
            const LabEntry* other = check.Find(item.name);
            if (other == nullptr || !check.ReadBytes(*other, after, readError)) {
                result.error = "Gegenprobe: '" + item.name + "' fehlt in der neuen Datei. "
                               "Das Original ist unberuehrt.";
                return result;
            }
            if (&item == entry) {
                if (after != data) {
                    result.error = "Gegenprobe: der ersetzte Eintrag stimmt nicht. "
                                   "Das Original ist unberuehrt.";
                    return result;
                }
                continue;
            }
            if (!archive.ReadBytes(item, before, readError) || before != after) {
                result.error = "Gegenprobe: '" + item.name + "' hat sich veraendert, "
                               "obwohl er nicht angefasst wurde. Das Original ist "
                               "unberuehrt.";
                return result;
            }
        }
    }

    // ---- Tausch ----
    //
    // Die Zahl VOR dem Schliessen merken: entries ist eine
    // Referenz auf archive.Entries(), und Close() leert die Liste.
    // Die Meldung stand deshalb auf "0 Eintraege".
    const size_t entryCount = entries.size();
    archive.Close();
    if (std::remove(archivePath.c_str()) != 0) {
        result.error = "Der alte Archivstand laesst sich nicht entfernen - laeuft das "
                       "Spiel noch? Die neue Datei liegt unter " + temporary;
        return result;
    }
    if (std::rename(temporary.c_str(), archivePath.c_str()) != 0) {
        result.error = "Umbenennen fehlgeschlagen. Die neue Datei liegt unter " +
                       temporary + ", die Sicherung unter " + backup;
        return result;
    }

    result.ok = true;
    result.report += "Archiv neu aufgebaut: " + std::to_string(entryCount) +
                     " Eintraege, '" + entryName + "' von " + std::to_string(oldSize) +
                     " auf " + std::to_string(data.size()) +
                     " Byte, Gegenprobe bestanden\n";
    return result;
}

LabWriteResult RestoreBackup(const std::string& archivePath) {
    LabWriteResult result;
    const std::string backup = BackupPathOf(archivePath);
    if (!FileExists(backup)) {
        result.error = "Keine Sicherung gefunden: " + backup;
        return result;
    }
    std::string error;
    const std::string temporary = archivePath + ".zurueck";
    if (!CopyFileChunked(backup, temporary, error)) { result.error = error; return result; }
    if (!FilesEqual(backup, temporary, error)) {
        result.error = "Die Kopie der Sicherung stimmt nicht: " + error;
        return result;
    }
    std::remove(archivePath.c_str());
    if (std::rename(temporary.c_str(), archivePath.c_str()) != 0) {
        result.error = "Umbenennen fehlgeschlagen. Die Sicherung liegt weiter unter " + backup;
        return result;
    }
    result.ok = true;
    result.report = "Aus der Sicherung wiederhergestellt: " + backup + "\n";
    return result;
}

std::vector<std::string> LabArchive::NamesWithExtension(const std::string& ext) const {
    const std::string wanted = "." + ToLower(ext);
    std::vector<std::string> names;
    for (const LabEntry& entry : entries_) {
        const std::string lower = ToLower(entry.name);
        if (lower.size() > wanted.size() &&
            lower.compare(lower.size() - wanted.size(), wanted.size(), wanted) == 0) {
            names.push_back(entry.name);
        }
    }
    std::sort(names.begin(), names.end());
    return names;
}

} // namespace baf
