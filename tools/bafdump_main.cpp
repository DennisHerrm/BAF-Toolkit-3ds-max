// ============================================================
//  bafdump - eigenstaendiges Werkzeug, OHNE Max SDK
//
//  Genau das ist der Punkt: der Parser laesst sich damit gegen
//  die Python-Referenz gegenpruefen, ohne 3ds Max ueberhaupt zu
//  starten. Erst wenn der Dump zeichengleich ist, kommt die
//  Max-Seite dran.
//
//    bafdump big.lab                      Archiv auflisten
//    bafdump big.lab anakin.baf           Zusammenfassung
//    bafdump big.lab anakin.baf out.txt   Dump schreiben
//    bafdump anakin.baf out.txt           lose Datei dumpen
//    bafdump big.lab --alle out/          alle 303 Modelle dumpen
//
//  Der Detailgrad wird ueber --verts / --keys / --all gesteuert.
// ============================================================

#include "../src/baf_lab.h"
#include "../src/baf_reader.h"

#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int Usage() {
    std::cout <<
        "bafdump - Phantom-Menace-.baf lesen und dumpen\n\n"
        "  bafdump <big.lab>\n"
        "  bafdump <big.lab> <name.baf> [ausgabe.txt] [--verts] [--keys] [--rest] [--all]\n"
        "  bafdump <datei.baf> [ausgabe.txt] [--verts] [--keys] [--all]\n"
        "  bafdump <big.lab> --alle <ordner> [--all]\n"
        "  bafdump <big.lab> <name.baf> <ordner> --tga\n"
        "  bafdump <big.lab> --rundlauf        lesen, schreiben, Bytes vergleichen\n";
    return 2;
}

bool EndsWith(const std::string& text, const char* suffix) {
    const size_t length = std::strlen(suffix);
    if (text.size() < length) return false;
    for (size_t i = 0; i < length; ++i) {
        char a = text[text.size() - length + i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a + 32);
        if (a != suffix[i]) return false;
    }
    return true;
}

bool WriteDumpFile(const baf::BafFile& file, const std::string& label,
                   const std::string& path, uint32_t detail) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "Kann nicht schreiben: " << path << "\n";
        return false;
    }
    baf::WriteDump(file, label, out, detail);
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<std::string> positional;
    uint32_t detail = baf::kDumpCounts;
    bool dumpAll = false;
    bool exportTga = false;
    bool roundTrip = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--verts")      detail |= baf::kDumpVertices;
        else if (arg == "--keys")  detail |= baf::kDumpKeys;
        else if (arg == "--rest")  detail |= baf::kDumpRest;
        else if (arg == "--build") detail |= baf::kDumpBuild;
        else if (arg == "--anim")  detail |= baf::kDumpAnim;
        else if (arg == "--props") detail |= baf::kDumpProps;
        else if (arg == "--all")   detail |= baf::kDumpAll;
        else if (arg == "--alle")  dumpAll = true;
        else if (arg == "--tga")   exportTga = true;
        else if (arg == "--rundlauf") roundTrip = true;
        else                       positional.push_back(arg);
    }
    if (positional.empty()) return Usage();

    // ---- lose .baf ----
    if (EndsWith(positional[0], ".baf")) {
        baf::BafFile file;
        const baf::ReadResult result = baf::ReadBafFile(positional[0], file);
        if (!result.ok) { std::cerr << result.error << "\n"; return 1; }
        std::cout << baf::MakeSummary(file) << "\n";
        if (positional.size() >= 2) {
            if (!WriteDumpFile(file, positional[0], positional[1], detail)) return 1;
            std::cout << "Dump geschrieben: " << positional[1] << "\n";
        }
        return 0;
    }

    // ---- Archiv ----
    baf::LabArchive archive;
    const baf::LabResult opened = archive.Open(positional[0]);
    if (!opened.ok) { std::cerr << opened.error << "\n"; return 1; }

    if (positional.size() == 1 && !roundTrip) {
        const std::vector<std::string> names = archive.NamesWithExtension("baf");
        std::cout << archive.Count() << " Eintraege, davon " << names.size() << " Modelle\n";
        for (const std::string& name : names) std::cout << "  " << name << "\n";
        return 0;
    }

    if (roundTrip) {
        // Jedes Modell lesen, wieder schreiben und die Bytes
        // vergleichen. Das ist die schaerfste Pruefung, die es
        // fuer einen Schreiber gibt: hat sich nichts geaendert,
        // muss die Datei dieselbe sein.
        const std::vector<std::string> names = archive.NamesWithExtension("baf");
        size_t identical = 0, differing = 0, failed = 0;
        for (const std::string& name : names) {
            const baf::LabEntry* item = archive.Find(name);
            std::vector<uint8_t> raw;
            std::string error;
            if (!item || !archive.ReadBytes(*item, raw, error)) { ++failed; continue; }
            baf::BafFile file;
            if (!baf::ReadBafBuffer(raw.data(), raw.size(), name, file).ok) { ++failed; continue; }
            std::vector<uint8_t> rebuilt;
            if (!baf::BuildBaf(file, rebuilt, error)) {
                std::cerr << name << ": " << error << "\n";
                ++failed;
                continue;
            }
            const size_t at = baf::FirstDifference(raw, rebuilt);
            if (at == SIZE_MAX) {
                ++identical;
            } else {
                ++differing;
                std::cerr << name << ": erste Abweichung bei Offset " << at << "\n";
            }
        }
        std::cout << identical << " byte-gleich, " << differing << " abweichend, "
                  << failed << " fehlgeschlagen\n";
        return (differing || failed) ? 1 : 0;
    }

    if (dumpAll) {
        const std::string folder = positional[1];
        const std::vector<std::string> names = archive.NamesWithExtension("baf");
        size_t done = 0, failed = 0;
        const auto started = std::chrono::steady_clock::now();
        for (const std::string& name : names) {
            const baf::LabEntry* entry = archive.Find(name);
            std::vector<uint8_t> bytes;
            std::string error;
            if (!entry || !archive.ReadBytes(*entry, bytes, error)) {
                std::cerr << name << ": " << error << "\n"; ++failed; continue;
            }
            baf::BafFile file;
            const baf::ReadResult result = baf::ReadBafBuffer(bytes.data(), bytes.size(), name, file);
            if (!result.ok) { std::cerr << result.error << "\n"; ++failed; continue; }
            const std::string target = folder + "/" + name + ".txt";
            if (!WriteDumpFile(file, name, target, detail)) { ++failed; continue; }
            ++done;
        }
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        std::cout << done << " gedumpt, " << failed << " fehlgeschlagen, "
                  << elapsed << " ms\n";
        return failed ? 1 : 0;
    }

    if (exportTga) {
        // Texturen eines Modells als TGA herausschreiben. Damit
        // laesst sich die Palettenaufloesung ansehen, ohne 3ds Max
        // zu starten.
        const baf::LabEntry* found = archive.Find(positional[1]);
        if (!found) { std::cerr << positional[1] << " nicht im Archiv\n"; return 1; }
        std::vector<uint8_t> raw;
        std::string readError;
        if (!archive.ReadBytes(*found, raw, readError)) { std::cerr << readError << "\n"; return 1; }
        baf::BafFile file;
        const baf::ReadResult result = baf::ReadBafBuffer(raw.data(), raw.size(), found->name, file);
        if (!result.ok) { std::cerr << result.error << "\n"; return 1; }

        const std::string folder = (positional.size() >= 3) ? positional[2] : std::string(".");
        int written = 0, skipped = 0;
        for (size_t m = 0; m < file.materials.size(); ++m) {
            for (size_t c = 0; c < file.materials[m].cels.size(); ++c) {
                baf::DecodedCel decoded;
                if (!baf::DecodeCel(file, m, c, decoded)) { ++skipped; continue; }
                std::string name = baf::SafeFileName(file.materials[m].name,
                                                     "mat" + std::to_string(m));
                const size_t dot = name.find_last_of('.');
                if (dot != std::string::npos) name = name.substr(0, dot);
                if (c > 0) name += "_cel" + std::to_string(c);
                const std::string target = folder + "/" + name + ".tga";
                std::string writeError;
                if (baf::WriteTga(decoded, target, writeError)) ++written;
                else { std::cerr << writeError << "\n"; ++skipped; }
            }
        }
        std::cout << written << " TGA geschrieben, " << skipped << " uebersprungen\n";
        return 0;
    }

    const baf::LabEntry* entry = archive.Find(positional[1]);
    if (!entry) { std::cerr << positional[1] << " nicht im Archiv\n"; return 1; }
    std::vector<uint8_t> bytes;
    std::string error;
    if (!archive.ReadBytes(*entry, bytes, error)) { std::cerr << error << "\n"; return 1; }

    baf::BafFile file;
    const auto started = std::chrono::steady_clock::now();
    const baf::ReadResult result = baf::ReadBafBuffer(bytes.data(), bytes.size(), entry->name, file);
    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - started).count();
    if (!result.ok) { std::cerr << result.error << "\n"; return 1; }

    std::cout << baf::MakeSummary(file) << "\n  Lesezeit:    " << micros << " us\n";
    if (!result.warnings.empty()) std::cout << "Hinweise:\n" << result.warnings;
    if (positional.size() >= 3) {
        if (!WriteDumpFile(file, entry->name, positional[2], detail)) return 1;
        std::cout << "Dump geschrieben: " << positional[2] << "\n";
    }
    return 0;
}
