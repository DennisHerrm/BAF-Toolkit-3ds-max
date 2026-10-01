// ============================================================
//  .lab Archiv-Leser - Header
//
//  big.lab, localize.lab, voice.lab von Star Wars Episode I:
//  The Phantom Menace (Big Ape Productions / LucasArts, 1999).
//
//  Containerformat (aus dem Retail-Loader hergeleitet, siehe
//  lab.py des Blender-Addons):
//      0x00  char[4]  Magic "LABN"
//      0x04  u32      Konstante (unbekannt, wird nur durchgereicht)
//      0x08  u32      Anzahl Eintraege
//      0x0c  u32      Groesse des Namenspools
//      0x10  n * 16-Byte-Saetze:
//                 u32 nameOffset  (in den Namenspool, NUL-terminiert)
//                 u32 dataOffset  (absolut in dieser Datei)
//                 u32 size
//                 u32 typeFourcc
//      ...   Namenspool
//      ...   Nutzdaten
//
//  WICHTIG: Diese Uebersetzungseinheit haengt bewusst NICHT am
//  3ds Max SDK - genau wie xfbin_reader bei XFBIN Import. Nur
//  <cstdint>, <string>, <vector>. Dadurch laesst sie sich im
//  Plugin verwenden UND als eigenstaendiges bafdump.exe bauen,
//  das man ohne Max gegen die Python-Referenz diffen kann.
//  Bitte nicht mit Max-Typen "verbessern".
//
//  Nur lesend. Schreiben in ein Archiv ist bewusst nicht drin:
//  das Blender-Addon macht das mit Backup, Rebuild und
//  Byte-Gegenprobe, und ein halb geschriebenes 115-MB-Archiv aus
//  einem Importer heraus waere ein Datenverlust ohne Not.
// ============================================================

#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace baf {

// ------------------------------------------------------------
//  Datei zum Lesen oeffnen. Der Pfad ist UTF-8.
//
//  Warum nicht einfach fopen: unter Windows nimmt fopen den Pfad
//  in der ANSI-Codepage entgegen. Ein Pfad mit Umlaut oder mit
//  japanischen Zeichen - und 3ds Max steht auf vielen Rechnern in
//  solchen Ordnern - kommt dort falsch an, und die Datei wird
//  "nicht gefunden", obwohl sie da ist. Deshalb wird der Pfad
//  hier nach UTF-16 gewandelt und _wfopen benutzt.
//
//  Die Umwandlung ist von Hand geschrieben, damit diese
//  Uebersetzungseinheit weiterhin ohne <windows.h> auskommt und
//  bafdump auch unter Linux baut.
//
//  Rueckgabe nullptr, wenn die Datei nicht zu oeffnen ist. Der
//  Aufrufer schliesst sie mit fclose.
// ------------------------------------------------------------
std::FILE* OpenBinaryRead(const std::string& utf8Path);

// Dasselbe zum Schreiben. Ein Texturexport in einen Ordner mit
// Umlaut scheitert sonst genauso wie das Lesen.
std::FILE* OpenBinaryWrite(const std::string& utf8Path);

inline constexpr uint32_t kLabHeaderBytes = 0x10;
inline constexpr uint32_t kLabRecordBytes = 0x10;

struct LabEntry {
    std::string name;         // klein geschrieben wie in der Datei, latin1
    uint32_t    nameOffset = 0;
    uint32_t    dataOffset = 0;
    uint32_t    size       = 0;
    uint32_t    typeFourcc = 0;
};

// ------------------------------------------------------------
//  Ergebnis eines Leseversuchs. Kein Exception-Werfen nach
//  aussen: das Plugin laeuft im Max-Prozess, und eine
//  durchgereichte Exception ist dort ein Absturz, kein
//  Fehlerdialog.
// ------------------------------------------------------------
struct LabResult {
    bool        ok = false;
    std::string error;
};

// ------------------------------------------------------------
//  Schreiben ins Archiv.
//
//  Bewusst als freie Funktion und nicht als Methode von
//  LabArchive: die Klasse bleibt damit nur lesend, und man sieht
//  an der Aufrufstelle, dass hier eine 115-MB-Datei angefasst
//  wird.
//
//  Der Ablauf ist dreistufig und in dieser Reihenfolge wichtig:
//
//   1. SICHERUNG. Gibt es noch keine .bak, wird das Original
//      dorthin kopiert. Nur einmal - eine zweite Sicherung wuerde
//      die erste ueberschreiben und damit genau den Stand
//      vernichten, zu dem man zurueckwill.
//   2. SCHREIBEN. Ist der neue Eintrag genauso gross wie der
//      alte, werden nur seine Bytes an Ort und Stelle ersetzt -
//      das ist der Normalfall beim Bearbeiten und beruehrt die
//      restlichen 115 MB gar nicht. Sonst wird die ganze Datei
//      neu aufgebaut, erst in eine Nebendatei.
//   3. GEGENPROBE. Das Ergebnis wird wieder eingelesen und Byte
//      fuer Byte gegen das gehalten, was dastehen soll. Erst
//      wenn das stimmt, tritt die neue Datei an die Stelle der
//      alten.
//
//  Das Archiv der Retail-Daten ist lueckenlos gepackt: 1336 von
//  1336 Uebergaengen ohne ein einziges Fuellbyte, und der letzte
//  Eintrag endet genau am Dateiende. Der Neuaufbau haelt sich
//  daran.
// ------------------------------------------------------------
struct LabWriteResult {
    bool        ok = false;
    std::string error;
    std::string report;    // was getan wurde, fuer das Protokoll
};

LabWriteResult ReplaceEntry(const std::string& archivePath,
                            const std::string& entryName,
                            const std::vector<uint8_t>& data,
                            bool makeBackup);

// Die Sicherung zurueckspielen. Ohne Umweg, ohne Nachfrage -
// die Nachfrage gehoert in die Oberflaeche.
LabWriteResult RestoreBackup(const std::string& archivePath);

class LabArchive {
public:
    // Liest NUR das Inhaltsverzeichnis ein, nicht die Nutzdaten.
    // big.lab ist 115 MB gross - die will man nicht im Speicher
    // halten, nur um an ein 400-KB-Modell zu kommen.
    LabResult Open(const std::string& path);
    void      Close();

    bool IsOpen() const { return open_; }
    const std::string& Path() const { return path_; }

    const std::vector<LabEntry>& Entries() const { return entries_; }
    size_t Count() const { return entries_.size(); }

    // Sucht case-insensitiv ueber den Basisnamen. nullptr = nicht da.
    const LabEntry* Find(const std::string& filename) const;

    // Laedt die Nutzdaten eines Eintrags. false = Datei kuerzer
    // als das Inhaltsverzeichnis behauptet.
    bool ReadBytes(const LabEntry& entry, std::vector<uint8_t>& out,
                   std::string& error) const;

    // Namen aller Eintraege mit dieser Endung (ohne Punkt, klein),
    // alphabetisch. Fuer die Modellliste in der Oberflaeche.
    std::vector<std::string> NamesWithExtension(const std::string& ext) const;

private:
    bool                  open_ = false;
    std::string           path_;
    uint32_t              fieldA_ = 0;
    uint32_t              namePoolSize_ = 0;
    std::vector<LabEntry> entries_;
};

} // namespace baf
