# BAF Toolkit fuer 3ds Max - Entwicklungsnotizen

Importer fuer die Charaktere aus *Star Wars Episode I: The Phantom Menace*
(Big Ape Productions / LucasArts, 1999). Aufbau nach dem Vorbild von
**XFBIN Import** und **AnimMerge**: SDK-freier Parser in C++, duenne
Max-Schicht darueber, Oberflaeche in MAXScript.

---

## 1. Stand

**Stufe 0 ist fertig und gegengeprueft.**

| | |
|---|---|
| Parser | `src/baf_lab.*` (Archiv), `src/baf_reader.*` (Modell) |
| Werkzeug | `tools/bafdump_main.cpp` -> `bafdump.exe`, braucht kein Max SDK |
| Referenz | `tools/pydump.py`, benutzt den Codec des Blender-Addons |
| Gegenprobe | **303 Modelle aus `big.lab`, 655.923 Dumpzeilen, 0 Abweichungen** |
| Geschwindigkeit | 79,7 us pro Modell (reines Parsen, ohne Datei-I/O) |

Der Gegenprobe-Ablauf, der bei XFBIN funktioniert hat, laeuft hier genauso:

```
bafdump.exe big.lab --alle dump_cpp --all
python tools\pydump.py <addon-ordner> big.lab --alle dump_py --all
fc /L dump_cpp\anakin.baf.txt dump_py\anakin.baf.txt
```

### Warum die Zahlen von Hand formatiert werden

Zwei Fallen, nicht eine.

Die bekannte: ein deutsches 3ds Max schreibt mit `printf` Dezimalkommata, und
der Dump laesst sich nicht mehr vergleichen. Bei XFBIN schon passiert.

Die zweite ist tueckischer. Weder `printf` noch `std::to_chars` legen fest,
wohin ein **exakter Rundungs-Gleichstand** faellt - der Standard sagt zu
`to_chars(fixed)` nur "wie printf im C-Locale", und die Tie-Regel von printf
ist Sache der Implementierung. Die UCRT von Microsoft darf sie also anders
ziehen als die glibc.

Nachgezaehlt ueber den ganzen Retail-Datensatz: von 909.008 verschiedenen
Gleitkommawerten liegen **473 exakt auf der Grenze** der 6. Nachkommastelle.
`0.1015625` etwa ist als float32 exakt darstellbar; kaufmaennisch gerundet
wird daraus `0.101563`, nach der Regel "zur geraden Ziffer" `0.101562`. Beide
sind richtig. Nur eben nicht dasselbe - und der Vergleich haette einen Fehler
gemeldet, den es nicht gibt.

`FormatFloat()` rechnet deshalb selbst: skalieren, Betrag, `+0.5`, abrunden.
Die Multiplikation ist eine gewoehnliche IEEE-754-Operation und damit auf
jeder Plattform bitgleich, und die Tie-Regel steht danach in beiden Sprachen
im Quelltext statt in einer Laufzeitbibliothek. `pydump.py` macht Zeile fuer
Zeile dasselbe.

Nebeneffekt: `<charconv>` wird nicht mehr gebraucht. Damit faellt auch die
Weiche fuer die Toolsets vor VS 2019 16.4 weg (die `to_chars` fuer `float`
noch nicht hatten) - alle Jahrgaenge 2016-2027 gehen durch denselben Code.

Keine Sorge um Ueberlaeufe: der groesste Betrag im Datensatz ist 1,9e6,
skaliert 1,9e12, weit unter 2^53. Dort haelt ein `double` noch jede ganze
Zahl exakt, und das `+0.5` ist verlustfrei. NaN und Inf kommen in den Daten
nicht vor, werden aber beidseitig gleich behandelt.

---

## 2. Was das Format ist, und was es nicht ist

Eine `.baf` ist **ein kompletter Charakter in einer Datei**: Palette,
Texturen, Meshes, Skelett und alle Animationsclips. Das ist der grosse
Unterschied zu XFBIN, wo Modell und Animationen in getrennten Dateien lagen
und der Szenenzustand einen Dateiwechsel ueberleben musste. Hier reicht ein
Durchgang.

Der zweite grosse Unterschied: **es gibt kein Skinning.** Jedes Mesh gehoert
genau einem Knoten (`node.mesh_index`), starr. Kein `Skin`-Modifier, keine
Gewichte, keine `ISkinImportData`, keine Bind-Pose-Fallen. Die ganze
Fehlerklasse, die bei XFBIN die Zunge ueberdehnt hat, existiert hier nicht.

Groessenordnung des gesamten Retail-Datensatzes:

| | |
|---|---|
| Modelle in `big.lab` | 303 |
| Knoten gesamt | 3.483 (groesstes Modell: `obiwan.baf` mit 48) |
| Meshes / Vertices / Faces | 3.139 / 44.536 / 46.466 |
| Keyframes gesamt | 438.580 |

Das ist winzig. **Geschwindigkeit wird hier nicht das Problem.** C++ ist
trotzdem die richtige Wahl - aber wegen der Kontrolle ueber Matrizen,
Controller und Bitmaps, nicht wegen der 80 Mikrosekunden. Der Flaschenhals
wird das Anlegen der Szene in Max sein, genau wie bei XFBIN (dort:
Parsen 4 ms, Skelettbau 20 ms).

---

## 3. Abbildung auf 3ds Max

### 3.1 Skelett

```
lokal[i]  = Basis(bind_euler) mit Translation bind_pos * scale
welt[i]   = welt[parent] * lokal[i]        (parent < i ist garantiert)
```

`EulerBasis()` liefert `right / forward / up`. **Achtung auf die
Konvention:** das Blender-Addon setzt die drei als *Spalten* ein
(Spaltenvektor-Konvention). Max rechnet mit Zeilenvektoren, also:

```cpp
Matrix3 tm;
tm.SetRow(0, Point3(right.x,   right.y,   right.z));
tm.SetRow(1, Point3(forward.x, forward.y, forward.z));
tm.SetRow(2, Point3(up.x,      up.y,      up.z));
tm.SetRow(3, Point3(pos.x, pos.y, pos.z));
```

Das ist genau die Transponierte der Blender-Matrix und damit richtig. Bei
XFBIN wurde die Zeilenvektor-Konvention ueber den Bone-Dump bestaetigt -
derselbe Test gehoert hierher, bevor irgendetwas anderes gebaut wird.

Ein Punkt aus dem XFBIN-Verlauf, der hier vorbeugend gilt: **nicht
konjugieren.** Die 0.6.0/0.6.1-Runde hat gezeigt, dass die Konjugation aus
dem MDLX-Importer der MDX-Formatkonvention galt, nicht Max. Bei
`CTRL_ABSOLUTE` sagt das SDK "simply store the value".

### 3.2 Meshes

Vertexpositionen sind **knotenlokal, aber mit Pivot-Versatz**:

```
v_lokal = v_roh + node.pivot
```

Das Mesh wird an den Bone gehaengt, den `node.mesh_index` nennt. Kein Skin,
kein Modifier - schlichtes Parenting.

Zwei Dinge, die bei Max anders sind als bei Blender:

**N-Gons.** Blender nimmt Polygone beliebiger Eckenzahl, `Mesh` in Max nur
Dreiecke. Verteilung ueber den ganzen Datensatz:

| Ecken | 3 | 4 | 5 | 6 | 7 | 8 | 9-12 |
|---|---|---|---|---|---|---|---|
| Anzahl | 35.942 | 9.955 | 316 | 126 | 18 | 96 | 13 |

Also: Faechertriangulierung ab Ecke 0, und die `TVFace` in derselben
Reihenfolge mitbauen. Wer statt `Mesh` ein `MNMesh` nimmt, kann die N-Gons
behalten - dann aber konsequent, sonst gibt es zwei Wahrheiten ueber die
Topologie. Empfehlung: `Mesh` mit Faecher, weil die Kantensichtbarkeit
(`setEdgeVis`) sich sauber setzen laesst und der Export spaeter die
Originalindizes ohnehin aus einer eigenen Tabelle holen muss.

**Entartete Faces.** 118 Faces im Datensatz nennen denselben Vertex zweimal.
Blender lehnt die ab, Max nicht unbedingt. Sie werden uebersprungen und ihre
Indizes gemerkt (`skippedFaces`), damit ein spaeterer Export sie
unveraendert wieder herausschreiben kann.

**UVs sind Texel, keine 0..1-Koordinaten.** Geteilt wird durch die Groesse
von Cel 0 des jeweiligen Materials, und V wird negiert:

```
u = texel.u / cel.width
v = -texel.v / cel.height
```

Leser und Schreiber muessen sich auf dieselbe Cel-Groesse einigen, sonst
rutscht die Textur. `BafFile::MaterialCelSize()` ist die eine Stelle dafuer.

`geometry_mode == 0` heisst: die Engine zeichnet das Mesh nie (die
verstauten Umhaenge). Ausblenden statt weglassen.

### 3.3 Materialien

Ein Max-Material je **(Materialindex, Renderklasse)**, nicht je
Materialindex. Ein Engine-Material traegt genau einen Blendmodus und ein
Zweiseitig-Bit; nach Textur allein zu gruppieren kollidiert.

Die Renderklasse steht erst **nach** den beiden Ladedurchgaengen fest, die
`ApplyLoadPasses()` nachbaut. Der Fallstrick dort ist im Code vermerkt und
wiederholt sich leicht: **Face 0 entscheidet fuer das ganze Mesh**, weil die
beiden `extraLight`-Vergleiche im Loader vor dem Schleifenkopf stehen. Wer
sie pro Face auswertet, macht ein Face von 64 additiv statt alle 64.

Texturen sind 8 Bit palettisiert, 768-Byte-Palette (eigene je Material, wenn
`kMatOwnPalette` gesetzt ist). MAT-Zeilen laufen von oben nach unten. Index 0
ist nur dann transparent, wenn das Key-Bit des Cels es sagt - sonst entstehen
Loecher in hunderten Texturen.

### 3.4 Animation - der eine echte Stolperstein

Jeder Clip hat **seine eigene FPS**, und die schwanken heftig: gemessen von
2 bis 160, Haeufungen bei 30 (586 Clips), 24, 26, 20, 15. Eine Max-Szene hat
genau eine Framerate. Also muss die Zeit umgerechnet werden:

```
frame_max = frame_clip * sceneFps / clip.fps
```

Ohne das laeuft jede Animation mit der falschen Geschwindigkeit, und zwar
unauffaellig - es sieht nur "irgendwie zaeh" aus.

Keys sind kein TCB und keine Quaternionen, sondern **Euler-Tripel plus
Steigung**: `pos + dpos * t`, `orient + dorient * t`. Dazu zwei Flags:

* ohne `kKeyLerpPos` wird die Position **gehalten** bis zum naechsten Key
* ohne `kKeyLerpOrient` gilt dasselbe fuer die Drehung

Zwei gangbare Wege:

1. **Keys uebernehmen**, Tangenten linear, und wo das Flag fehlt, auf Step
   (`BEZKEY_STEP` ueber `IKeyControl`/`IBezFloatKey`, danach `SortKeys` -
   dasselbe Muster wie die Sichtbarkeits-Keys bei XFBIN 1.9.2). Wenig Keys,
   aber die Steigungen `dpos`/`dorient` sind damit nur naeherungsweise
   abgebildet, sobald sie nicht exakt zur Sekante zum naechsten Key passen.
2. **Backen**, ein Key je Frame ueber `SampleTrack()`. Exakt, mehr Keys. Bei
   438.580 Keys ueber den ganzen Datensatz und typisch 100 Frames je Clip ist
   das immer noch harmlos.

Empfehlung: Backen als Standard, Keys-uebernehmen als Haken. Erst
`SampleTrack()` gegen die Python-Referenz dumpen, dann in Max bauen.

Track-Bindung geht **ueber den Index, nicht ueber den Namen** - in einigen
Modellen widersprechen sich die beiden.

Fuer den Sequenzmodus gilt dasselbe Muster wie bei XFBIN 1.0.0 und AnimMerge:
Frame 0 Bind-Pose, dann alle Clips mit einstellbarem Abstand hintereinander,
Note Track `animations` auf **`rootNode`** - das ist die MAXScript-Konstante
fuer den Szenen-Wurzelknoten, nicht der Wurzel-Bone der Figur. Der Fehler
kostete bei XFBIN eine ganze Version (1.4.0). Und: Ruhelage-Keys an beiden
Sequenzenden fuer alles, was ein Clip gar nicht anfasst, sonst interpoliert
Max quer durch die Luecke.

### 3.5 Was der Importer nicht anfasst

Schreiben ins Archiv. Das Blender-Addon macht das mit Backup, Rebuild und
Byte-Gegenprobe. Ein halb geschriebenes 115-MB-`big.lab` aus einem Importer
heraus waere ein Datenverlust ohne Not. `LabArchive` ist nur lesend.

---

## 4. Stufenplan

| Stufe | Inhalt | Gegenprobe |
|---|---|---|
| **0** | Archiv + Parser + Dump | **fertig, 0 Abweichungen ueber 303 Modelle** |
| **1** | `BafImport.dlu`, `BafCpp.*`, Modellliste aus `big.lab`, Dump aus Max heraus | **gebaut**, Gegenprobe in Max steht noch aus |
| **2** | Skelett bauen, Ruhelage-Mathematik | **Mathematik geprueft: 3.483 Knoten, 0 Abweichungen.** Bone-Dump aus Max steht aus |
| **3** | Meshes bauen, Triangulierung, UVs | **Aufbereitung geprueft: 57.966 Dreiecke, 0 Abweichungen.** Mesh-Dump aus Max steht aus |
| **4** | Texturen und Materialien | **2.725 Cels und 3.052 Materialplaetze geprueft, 0 Abweichungen** |
| **5** | Animationen, FPS-Umrechnung, Sequenzmodus | **438.580 Keys abgetastet, 0 Abweichungen.** Anim-Dump aus Max steht aus |
| **6** | Rohdaten, Normalen, Huellkugeln, Grenzen, Schalter | **geprueft ueber 303 Modelle, 0 Abweichungen** |
| **7** | Export: Schreiber, Szene zurueckuebernehmen | **Rundlauf byte-gleich in 303 von 303 Modellen** |
| **8** | Archiv schreiben, Sicherung, Wiederherstellen | **beide Wege am echten `big.lab` geprueft** |
| **9** | Topologie umbauen | **303 Modelle umgebaut, geschrieben, wieder gelesen** |

Reihenfolge bewusst wie bei XFBIN. Jede Stufe endet mit einem Dump, der sich
gegen die Python-Referenz diffen laesst, **bevor** die naechste anfaengt.

---

## 5. Die Regeln, nach denen der C++-Teil geschrieben ist

Kurz begruendet, weil danach gefragt war. Das meiste ist an den
[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines)
orientiert, aber nur da, wo es im Max-Kontext wirklich etwas bringt.

**Ein Cursor, sonst nichts.** Jeder Lesezugriff auf den Dateipuffer geht
durch `Cursor::Need()` und damit durch eine Bereichspruefung. Ausserhalb
dieser Klasse gibt es in `baf_reader.cpp` keine Zeigerarithmetik. Das ist
Bounds.1 in Kurzform - <cite index="18-2">die Kernidee des Bounds-Profils ist, die Hauptquellen von Bereichsverletzungen zu beseitigen: Zeigerarithmetik und Array-Indizierung</cite>. Wer C++20 setzen darf, nimmt
`std::span` statt `(pointer, size)`; hier ist es der Cursor, weil das Ziel
C++17 gegen alte SDKs ist.

**Nie `reinterpret_cast` auf einen Zahlentyp.** Gelesen wird mit `memcpy`.
Ein `*(uint32_t*)(data + off)` ist bei ungeraden Offsets formal undefiniert
und bricht die strikte Aliasing-Regel - der Optimizer darf daraus machen, was
er will. `memcpy` ist dabei nicht langsamer, es verschwindet im Compiler.

**Groessen in 64 Bit rechnen, bevor sie zu Speicher werden.**
`width * height * mips` sind drei Zahlen aus einer Datei, die ein Angreifer
oder ein defektes Byte gesetzt haben kann. In 32 Bit laeuft das ueber und
ergibt eine kleine, plausibel aussehende Zahl. `CelPixelBytes()` rechnet
deshalb in `uint64_t`, und `LabArchive::Open()` prueft die Eintragszahl auf
Plausibilitaet, **bevor** reserviert wird.

**Die Kachelpruefung ganz nach vorne.** Die staerkste Aussage ueber eine
`.baf` ist, dass ihre Bloecke die Datei lueckenlos ausfuellen. Passt das
nicht, wird gar nicht erst weitergelesen. Das faengt mehr ab als jede
Einzelpruefung danach.

**Intern werfen, nach aussen nie.** `ParseError` fliegt durch den Parser,
`ReadBafBuffer` faengt alles - auch `std::bad_alloc` - und gibt ein
`ReadResult` zurueck. Eine Exception, die aus einem Plugin in den
Max-Prozess laeuft, ist ein Absturz, kein Fehlerdialog.

**Kein `swprintf_s` auf feste Puffer.** Der 1.9.3-Absturz bei XFBIN war kein
Nullzeiger, sondern 656 Zeichen in einem 420er-Puffer: `swprintf_s` ruft dann
den Invalid-Parameter-Handler, und der beendet den Prozess. Meldungen gehen
hier ueber `std::string`, das mitwaechst.

**Der Parser haengt nicht am Max SDK.** Nur `<cstdint>`, `<string>`,
`<vector>`. Genau das macht `bafdump.exe` moeglich - und damit die
Gegenprobe, ohne Max ueberhaupt zu starten.

**`/MD`, `/Zp8`, x64.** Max laeuft nur mit der dynamischen CRT; ein
versehentliches `/MT` ergibt getrennte Heaps und Abstuerze, die erst weit
spaeter auffallen. `/Zp8` ist beim Max SDK Pflicht, sonst wird die Struktur
falsch gelesen. Steht beides in der `CMakeLists.txt` und wird dort erzwungen.

**`/W4` immer, `/permissive-` nur wenn die SDK-Header es aushalten.**
Ermittelt ueber `check_cxx_source_compiles` gegen `<max.h>`, nicht ueber eine
geratene Jahresgrenze - die Erfahrung aus XFBIN 1.5.2 ist, dass 2016-2018
durchgingen und 2019 nicht.

**Zahlen selbst formatieren, nicht die Laufzeitbibliothek fragen.** Siehe
Abschnitt 1: es geht nicht nur um das Dezimalkomma, sondern um die Tie-Regel,
die keine der beiden Standardfunktionen festlegt.

---

## 6. Die Regeln fuer den MAXScript-Teil

**Was ueberhaupt in MAXScript gehoert:** die Oberflaeche, der Layerbau, die
Sequenzsteuerung. Alles, was ueber Vertices oder Keys laeuft, gehoert ins
Plugin. Bei XFBIN hat sich diese Grenze bewaehrt.

**`with redraw off ( ... )`** um jeden Szenenaufbau. Autodesk selbst nennt es
den bevorzugten Weg gegenueber `disableSceneRedraw()`/`enableSceneRedraw()` -
<cite index="25-1">bei vielen schnell hintereinander laufenden Aenderungen sollen Redraws abgeschaltet werden, bis alles fertig ist</cite>.

**`with undo off ( ... )`** beim Import. <cite index="21-1">Das Undo-System fuellt sich bei grossen Szenenaenderungen schnell und frisst viel Speicher</cite>. Ein Import ist eine
Aenderung, kein Dutzend. Aber Vorsicht bei der Umkehrung:
<cite index="28-1">Knoten nicht mit undo off loeschen, ausser sie wurden auch mit undo off angelegt und dazwischen nicht mit undo on angefasst</cite> - sonst
erwartet ein Eintrag im Undo-Stack eine Szene, die es nicht mehr gibt, und
das naechste Strg+Z stuerzt Max ab.

**Nicht im Modify-Panel arbeiten.** `max create mode` oder
`suspendEditing()`/`resumeEditing()`. <cite index="27-1">Manche Operationen werten den Modifier-Stack neu aus und erzwingen ein Update des Command Panel, selbst wenn Viewport-Redraws unterdrueckt sind</cite>.

**Arrays vorbelegen** statt `append` in der Schleife - `append` kopiert.

**Kein `continue`.** MAXScript kennt es nicht. Steht bei XFBIN als siebte
Pruefung in `PRUEFE_SCRIPTS.py`; dieselbe Datei kann hier unveraendert
weiterlaufen.

**Der Sicherheitsteil, der wirklich zaehlt:**

* **Dateinamen aus der Datei sind Eingaben, keine Pfade.** Ein Materialname
  aus einer `.baf` landet spaeter als Texturdatei auf der Platte. Er kann
  `..\` oder einen Doppelpunkt enthalten. `_safe_filename()` im Blender-Addon
  macht genau das richtige: auf Basisnamen kuerzen, verbotene Zeichen
  ersetzen, und wenn nichts uebrig bleibt, einen Ersatznamen nehmen. Das
  gehoert hier ins Plugin, nicht ins Skript.
* **Nie `execute` auf zusammengebauten Strings.** Wenn ein Name aus einer
  Datei in einer `execute`-Zeile landet, ist das Codeausfuehrung aus
  Spieldaten. Stattdessen die API-Funktionen des Plugins aufrufen.
* **Die Pluginversion beim Start pruefen**, gegen eine Konstante im Skript.
  Der Fehler `Unknown property ...` bei XFBIN 1.7.1 kam davon, dass die
  Skripte installiert, das Plugin aber nicht neu gebaut war. Die Pruefung
  **nicht** ueber `getPropNames` machen: Plugin-Funktionen sind Methoden
  (`FN_0`/`FN_3`), nicht Eigenschaften - die Liste kommt leer zurueck und
  blockiert ein funktionierendes Plugin. Das war der Fehlschlag in 1.7.2.
* **Alle drei Eintragungsstellen pruefen.** Eine neue Plugin-Funktion muss in
  `enum`, `FUNCTION_MAP` **und** im Interface-Deskriptor stehen. Fehlt die
  dritte, ist sie bei MaxScript gar nicht angemeldet, und kein Neubau
  behebt das. `tools/PRUEFE_API.py` aus XFBIN prueft genau das und laesst
  sich uebernehmen.

---

## 6a. Stufe 1 - was das Plugin macht und was nicht

`BafImport.dlu` traegt genau eine Klasse (`UtilityObj`) und darunter ein
`FPStaticInterface` namens `BafCpp`. Die Oberflaeche ist MAXScript, nicht C++ -
dieselbe Aufteilung wie bei XFBIN Import.

In Stufe 1 wird **nichts in der Szene angelegt**. Das Plugin oeffnet das
Archiv, liest ein Modell und kann einen Dump schreiben. Genau das ist der
Zweck: der Dump aus dem Max-Prozess heraus muss zeichengleich mit dem von
`bafdump.exe` sein. Erst wenn das steht, wird gebaut - sonst sucht man
spaeter einen Parserfehler in einer Szene, die aus zehn anderen Gruenden
falsch aussehen kann.

Die `SceneRedrawGuard`- und `HoldSuspendGuard`-Klassen stehen schon im Header,
obwohl sie noch niemand benutzt. Ab Stufe 2 werden sie gebraucht, und wenn sie
erst dann entstehen, paart irgendwer die Aufrufe von Hand.

Drei Kleinigkeiten, die beim Schreiben aufgefallen sind:

**Pfade.** `fopen` nimmt unter Windows die ANSI-Codepage. Ein Pfad mit Umlaut
oder mit japanischen Zeichen kommt dort falsch an, und die Datei wird "nicht
gefunden", obwohl sie daliegt. `OpenBinaryRead()` in `baf_lab.cpp` wandelt
deshalb nach UTF-16 und benutzt `_wfopen`. Die Umwandlung ist von Hand
geschrieben statt mit `MultiByteToWideChar`, damit der Parser weiter ohne
`<windows.h>` auskommt und `bafdump` auch unter Linux baut. Hin- und
Rueckweg sind gegen fuenf Pfade mit Umlauten, Kana, Kanji und Emoji geprueft,
mit 16-Bit-`wchar_t` wie unter Windows.

**latin1, nicht cp932.** Strings in einer `.baf` sind latin1. Anders als bei
XFBIN braucht es dafuer kein `MultiByteToWideChar`: die Codepunkte 0 bis 255
sind in Unicode dieselben Zahlen, ein Byte wird ein `wchar_t`.

**Rueckgabepuffer.** Eine an MaxScript zurueckgegebene `MCHAR*` muss den
Aufruf ueberleben. Ein lokaler `std::wstring` waere beim Lesen laengst weg,
also gibt es einen Member, durch den jede Textrueckgabe laeuft (`Give()`).

## 6b. Stufe 2 - Skelett

Die Ruhelage-Mathematik steckt jetzt im Parser (`ComputeRestWorld`), nicht in
der Max-Schicht. Damit laesst sie sich gegen die Python-Referenz halten,
**bevor** 3ds Max ueberhaupt startet: 303 Modelle, 3.483 Knoten, 0
Abweichungen.

`Matrix34` ist so angeordnet, dass `m[0..2]` die Basis und `m[3]` die
Translation sind - genau das, was `Matrix3::SetRow` haben will. Das ist schon
die Transponierte der Blender-Matrix, wo dieselben Vektoren als Spalten
stehen. Beim Bauen wird deshalb nur `double` nach `float` gewandelt, nichts
umsortiert.

### Der Fund: FMA-Kontraktion

Beim Gegenpruefen der Ruhelage ist etwas aufgefallen, das ohne den Vergleich
nie sichtbar geworden waere.

Ein Compiler darf `a*b + c` zu einem einzigen FMA-Befehl zusammenziehen. Das
ist schneller **und genauer** - es faellt ein Zwischenrundungsschritt weg.
Trotzdem ist es hier schaedlich: die Weltmatrizen entstehen aus genau solchen
Summen, und ob kontrahiert wird, haengt vom Compiler und den erlaubten
Befehlssaetzen ab.

Nachgemessen an den Retail-Daten: mit Kontraktion aendern sich **18 von 21.166
Ruhelage-Zeilen in 13 der 303 Modelle**. Der Vergleich gegen die Referenz
haette dort Abweichungen gemeldet, an denen nichts falsch ist - und die Suche
danach waere lang geworden, weil beide Seiten "richtig" gerechnet haben.

`CMakeLists.txt` schaltet die Kontraktion deshalb auf beiden Compilern aus
(`-ffp-contract=off` bzw. `/fp:precise`). Bei 80 Mikrosekunden je Modell ist
der Geschwindigkeitsverlust kein Thema.

Dasselbe gilt fuer die Reihenfolge der Terme in `EulerBasisD()`: sie ist
wortwoertlich die von `euler_basis()` im Blender-Addon. Ein umgestellter Term
ergaebe in den letzten Bits ein anderes Ergebnis.

`sin` und `cos` sind uebrigens kein Problem, obwohl sie nicht korrekt gerundet
sind und sich zwischen Bibliotheken unterscheiden: Python benutzt die libm der
Plattform, auf der es laeuft - also dieselbe wie der C++-Compiler daneben. Der
Vergleich findet immer innerhalb einer Plattform statt.

### Die Gegenprobe in Max

`restDump` schreibt die Ruhelage aus den **Dateidaten**, `sceneBoneDump` liest
sie aus der **Szene** zurueck. Beide schreiben dasselbe Format, ein `fc /L`
genuegt. Wichtig ist, dass `sceneBoneDump` wirklich `GetNodeTM` abfragt und
nicht neu rechnet - sonst prueft der Vergleich nur, ob dieselbe Funktion
zweimal dasselbe ausrechnet.

Ein geloeschter Bone gibt eine Nullzeile statt zu fehlen. Sonst verschieben
sich alle folgenden Zeilen, und der Vergleich meldet lauter Unterschiede, wo
nur einer ist.

### Handles statt Zeiger

`boneHandles_` haelt `AnimHandle`, keine `INode*`. Ein roher Zeiger auf einen
Szenenknoten ueberlebt es nicht, wenn der Benutzer den Knoten zwischen zwei
Aufrufen loescht - und das Plugin merkt davon nichts. Ueber das Handle liefert
`GetAnimByHandle` dann schlicht `NULL`.

Der Typ ist bewusst `AnimHandle` und nicht `ULONG`: `AnimHandle` ist im SDK
zeigergross, `ULONG` bleibt auch auf x64 bei 32 Bit. In der Praxis sind die
Werte kleine laufende Nummern, weshalb `ULONG` lange gutgeht - aber "geht
meistens gut" ist bei einem Handle kein Zustand. (Derselbe Punkt gilt fuer
`boneHandles_` in XFBIN Import, wo `ULONG` steht.)

## 6c. Stufe 3 - Meshes

Wieder liegt die Aufbereitung im Parser (`BuildMeshGeometry`) und nicht in der
Max-Schicht: Pivot-Versatz, Faktor, Faechertriangulierung, UVs je Ecke,
uebersprungene Faces. Geprueft gegen die Python-Referenz: **57.966 Dreiecke
ueber 303 Modelle, 0 Abweichungen.**

Eine Einschraenkung dabei, damit sie nicht spaeter fuer mehr gehalten wird als
sie ist: die drei Ausschlussgruende (zu wenig Ecken, Vertex doppelt, Index
ausserhalb) sind wortwoertlich die des Blender-Addons, aber was danach kommt
ist bewusst **nicht** dessen `from_pydata`. Das laesst ein Polygon fallen,
dessen Vertexmenge schon vorkam - und die Retail-Kunst baut zweiseitige
Geometrie als deckungsgleiche Zwillinge. Max nimmt die an, also behalten wir
sie. Der Vergleich prueft an dieser Stelle also unsere eigene Regel, nicht die
von Blender.

### UVs je Ecke, nicht je Vertex

Das ist der Punkt, an dem eine naheliegende Abkuerzung falsch waere. Die UVs
der Datei haengen am Face (`face.texIdx`), nicht am Vertex - und die
Cel-Groesse, durch die geteilt wird, haengt am Material **dieses Face** und
kann innerhalb eines Mesh wechseln. Ein gemeinsamer TVert-Vorrat, wie man ihn
sonst anlegt, waere hier schlicht falsch.

Deshalb: `setNumMapVerts(1, faceCount * 3)`, eine Koordinate je Ecke,
`tvFace = (3f, 3f+1, 3f+2)`. Kostet Speicher, den es bei 44.536 Vertices im
gesamten Datenbestand nicht zu sparen gibt.

### Wo die Vertices liegen

Knotenlokal, mit Pivot-Versatz. Das Objekt bekommt die Weltmatrix seines Bones
und wird dann mit `AttachChild(..., 1)` angehaengt - die Vertices bleiben, wie
sie sind. Kein Skin-Modifier: jedes Mesh gehoert starr genau einem Knoten.

`geometryMode == 0` heisst, die Engine zeichnet das Mesh nie - das sind die
verstauten Umhaenge. Sie werden ausgeblendet, nicht weggelassen, sonst fehlen
sie beim Export.

### PRUEFE_INCLUDES.py

Neu, und vorbeugend: die neueren SDKs ziehen fast alles ueber `max.h` mit
herein, die aelteren nicht. Wer nur gegen 2027 baut, merkt davon nichts, und
der 2016er Build faellt mit einer Lawine Folgefehler um. Bei XFBIN Import ist
das dreimal passiert. `TriObject` braucht `triobj.h`, `ObjectState` und
`triObjectClassID` brauchen `object.h` - beides steht jetzt drin, und der
Pruefer meldet es, wenn es wieder herausfaellt.

## 6d. Stufe 4 - Texturen und Materialien

Wie bei den Stufen davor liegt alles Rechenbare im Parser: Palettenaufloesung
(`DecodeCel`), TGA-Schreiber, Materialplaetze (`BuildMaterialSlots`). Geprueft
gegen die Python-Referenz: **2.725 Cels und 3.052 Materialplaetze ueber 303
Modelle, 0 Abweichungen** - die Cels ueber eine Pruefsumme des RGBA-Puffers,
und zusaetzlich ein Cel-fuer-Cel-Pixelvergleich der geschriebenen TGA gegen
die Regel des Addons.

### Warum die Plaetze nicht nach Materialindex gehen

Ein Engine-Material traegt genau einen Blendmodus und ein Zweiseitig-Bit - und
diese Angaben stehen am **Face**, nicht am Material. Nach dem Index allein
gruppiert kollidieren sie: dasselbe Material kommt an einem Face deckend und
am naechsten additiv vor. Der Schluessel ist deshalb
`(Material, Blend, zweiseitig, vollhell)`.

Das Alpha gehoert bewusst nicht in den Schluessel. Es kommt aus der
Vertex-Intensitaet und ist praktisch stufenlos; ein Schluessel darueber
ergaebe fast so viele Plaetze wie Faces. Uebernommen wird das Alpha des ersten
Faces eines Platzes - so macht es das Addon auch.

Die Tabelle wird in Datei-Reihenfolge aufgebaut, nicht in einer `std::map`.
Sie ist damit deterministisch, und `buildMeshes` kann die Platznummer schon
als Material-ID setzen, ohne dass `buildMaterials` gelaufen sein muss. Beide
rechnen dieselbe Tabelle aus.

### TGA statt PNG

PNG braucht Deflate, und dafuer entweder zlib als Abhaengigkeit oder eine
eigene Umsetzung - fuer ein Bild, das 3ds Max ohnehin nativ liest. TGA ist
18 Byte Kopf und dann rohe Pixel. Zu beachten: TGA speichert von unten nach
oben und in der Kanalreihenfolge BGRA.

Index 0 ist **nur** durchsichtig, wenn das Key-Bit des Cels es sagt. Ihn
bedingungslos auszustanzen reisst Loecher in hunderte Texturen.

Die Bitmap bekommt `FILTER_NADA` - 8 Bit palettisierte Kunst von 1999 sieht
gefiltert verwaschen aus, und die Kante am Farbschluessel franst aus.

### Namen aus Spieldateien sind keine Pfade

`SafeFileName()` dampft auf druckbares ASCII ein, kuerzt auf den Basisnamen
und ersetzt die unter Windows verbotenen Zeichen. Die Reihenfolge ist wichtig:
erst nach dem Kuerzen ist sicher, dass kein Pfadanteil mehr drinsteckt. Ein
Materialname darf `..\` enthalten, und ungeprueft an einen Ordner gehaengt
schriebe der Export dann irgendwohin.

### Das Protokoll

Es laeuft ab dieser Fassung **immer** mit, nicht nur bei `setDebug 1`. Der
Grund ist praktisch: ein Fehler, den man erst nach zwanzig Minuten bemerkt,
liesse sich sonst nicht mehr nachvollziehen - man muesste ihn mit
eingeschaltetem Protokoll neu erzeugen, und genau das gelingt oft nicht.

`setDebug 1` macht es nur ausfuehrlicher: jeder Bone, jedes Mesh, jeder
Materialplatz einzeln statt nur die Meilensteine. `ClearMessages()` leert
Fehler und Hinweise, aber nicht das Protokoll.

Die Groesse ist auf 256 KB gedeckelt. Laeuft es voll, faellt der **alte** Teil
weg und nicht der neue - beim Suchen zaehlt das Ende. Geschnitten wird an
einem Zeilenumbruch, sonst beginnt das Protokoll mitten in einem Wort.
`BafCpp.saveLog` schreibt es als UTF-8 heraus.

## 6e. Stufe 5 - Animationen

Geprueft gegen die Python-Referenz: **438.580 Keys ueber 303 Modelle, 0
Abweichungen.** Der Dump wird damit gross (147 MB fuer den ganzen Bestand mit
`--all`) - das ist der Preis dafuer, dass wirklich jeder Key nachgerechnet
wird.

### Kein Umweg ueber die Ruhelage

Das Blender-Addon rechnet jeden Key gegen die inverse Ruhelage des Bones, weil
Blenders Pose-Raum ein anderer ist. In Max entfaellt das: ein PRS-Controller
haelt die Lage **relativ zum Elternteil**, und genau das liefert ein Key
ohnehin. `SampleLocalMatrix` geht also direkt in `SetValue`.

Die Quaternionen werden **nicht** konjugiert. Die SDK-Referenz zu
`Control::SetValue` ist eindeutig: bei `CTRL_ABSOLUTE` soll der Controller den
Wert schlicht speichern.

Was dagegen sein muss: das Vorzeichen fortlaufend halten. Das Vorzeichen eines
Quaternions ist frei, aber ein Sprung darin nimmt bei der Interpolation den
langen Weg herum - der Arm dreht sich dann einmal komplett um sich selbst.
Deshalb der Test auf `DotProd(rotation, previous) < 0`.

### Die FPS-Umrechnung

Der eine Punkt, an dem sich ein Fehler nicht zeigt, sondern nur *anfuehlt*.
Jeder Clip hat seine eigene Framerate, gemessen von 2 bis 160; eine Max-Szene
hat genau eine. Ohne

    frame_max = frame_clip * sceneFps / clip.fps

laeuft jede Animation mit der falschen Geschwindigkeit, und niemand kann
sagen, woran es liegt - es sieht nur "irgendwie zaeh" aus. `clipSceneFrames`
gibt die umgerechnete Laenge zurueck, und die Oberflaeche zeigt sie schon in
der Clipliste an.

### Backen oder Keys uebernehmen

Standard ist backen, ein Key je Frame. Der Grund: ein Key ohne sein
Interpolationsbit **haelt** seinen Wert bis zum naechsten, und Max wuerde
dazwischen interpolieren.

Im Key-Modus wird das nachgebildet, indem kurz vor dem naechsten Key derselbe
Wert noch einmal gesetzt wird - dieselbe Stufe, ohne an den Tangenten zu
drehen. Das ist eine Naeherung und steht als solche im Code: liegen zwei Keys
weniger als einen Frame auseinander, passt kein Zwischenkey mehr hinein.
Deshalb ist Backen der Standard.

### Sequenzmodus

Alle Clips hintereinander, mit einstellbarem Abstand. Zwei Dinge, die dabei
leicht untergehen:

**Ruhelage-Keys an beiden Enden jedes Clips**, fuer alles, was dieser Clip
gar nicht anfasst. Ohne sie interpoliert Max quer durch die Luecke zwischen
zwei Clips, und ein Arm, der in Clip 3 bewegt wird und in Clip 4 nicht,
wandert waehrend Clip 4 langsam durch den Koerper.

**Der Note Track heisst `animations` und haengt am `rootNode`** - und
`rootNode` ist die MAXScript-Konstante fuer den Wurzelknoten der SZENE, nicht
der Wurzel-Bone der Figur. Dieselbe Verwechslung hat bei XFBIN Import eine
ganze Version gekostet.

## 6f. Stufe 6 - was das Blender-Addon sonst noch mitnimmt

Nach einem Abgleich mit dem Addon nachgezogen. Vier Dinge, in dieser
Reihenfolge nach Wichtigkeit:

### Die Rohdaten - der Punkt, der nicht warten kann

Wortfelder je Face (`flags`, `lightMode`, `geometryMode`, `textureMode`,
`extraLight`, `material`), Intensitaet je Vertex, Pivot, Huellkugel, die
uebersprungenen Faces, die Zuordnung Dreieck -> Quellface, Kopfangaben,
Clip-Metadaten und Sound-Events.

Der Grund, warum das jetzt eingebaut ist und nicht spaeter: **es laesst sich
nicht rekonstruieren.** Wer heute importiert und in einem halben Jahr
exportieren will, hat es sonst nicht mehr - die Szene enthaelt dann nur noch
das Ergebnis, nicht die Eingabe. Beim Import kostet es fast nichts.

Abgelegt als `SetUserPropBuffer`, also unter Objekteigenschaften ->
Benutzerdefiniert. Von Hand lesbar, mit `getUserPropBuffer` aus MAXScript
abfragbar, und der Szenendatei egal. Kopfangaben und Clips haengen am ersten
Bone - dort sucht man sie, und ein eigenes Hilfsobjekt waere ein Objekt mehr.

Die Wortfelder je Face werden in **Rohform** gespeichert, also vor den beiden
Ladedurchgaengen. Ein Export muss sie unveraendert wieder herausschreiben, nicht
das Ergebnis der Durchgaenge.

### Vertexnormalen

Die Engine beleuchtet je Vertex (Gouraud), und die Normalen stehen in der
Datei. Bisher wurden sie gelesen und weggeworfen; Max rechnete eigene aus den
Glaettungsgruppen. Jetzt gehen sie ueber `MeshNormalSpec` hinein.

Das ist der Punkt, an dem Import und Blender im Viewport bisher
unterschiedlich ausgesehen haetten - am deutlichsten an den Uebergaengen
zwischen Koerperteilen.

Nullnormalen kommen in den Daten vor. Normalisieren wuerde durch null teilen,
also ein Ersatzwert nach oben - genauso macht es das Addon.

### Die Grenzen der Engine

48 Knoten je Modell, 0x200 Vertices und Faces je Mesh, 0x300 Texturvertices,
24 Ecken je Face. `CheckLimits` prueft das beim Oeffnen und die Oberflaeche
zeigt es an.

Bei allen 303 Retail-Modellen liegt es im Rahmen, was zu erwarten war - sie
sind ja ausgeliefert worden. Auffaellig wird es erst bei einem selbst gebauten
Modell, und dann soll es auffallen, bevor jemand eine Stunde in die Szene
steckt. `obiwan.baf` sitzt uebrigens mit 48 Knoten exakt auf der Grenze.

### Huellkugeln und Schalter

Die mitgelieferte Huellkugel je Mesh, auf Wunsch als Helfer angezeigt. Beim
Kopf ist das die Kugel, die der Blickrichtungs-Leser der KI transformiert - er
rechnet sie nicht nach.

Dazu die Schalter, die das Addon hat: Normalen uebernehmen, unsichtbare Meshes
ausblenden, Rohdaten mitspeichern, Huellkugeln anzeigen. Und ein Suchfeld ueber
die Modellliste - bei 303 Eintraegen scrollt man sonst.

### Was weiterhin fehlt

Der **Export** (im Addon 816 Zeilen, plus Archiv-Neubau mit Sicherung,
Byte-Gegenprobe und Wiederherstellen-Befehl). Ein eigenes Projekt, kein
Nachtrag - und sinnvoll erst, wenn der Import durchgetestet ist.

Der **`sith/`-Teil** des Addons (1.100 Zeilen) liest und schreibt `.3do` und
`.mat` der Jedi-Knight-Engine. Verwandtes Format, aber nicht Phantom Menace.

## 6g. Stufe 7 - der Export

### Die eine Entscheidung, an der alles haengt

**Es wird gepatcht, nicht neu gebaut.**

Jeder Satz der Datei wird beim Lesen als Rohbytes behalten, und der Schreiber
legt nur die dekodierten Felder wieder hinein. Der Rest bleibt Byte fuer Byte
stehen: alte Zeiger, die die Engine beim Laden ohnehin ueberschreibt, die
Fuellbytes des Compilers von 1999, der Muell hinter der NUL in einem
Namensfeld.

Der Grund ist nicht Ehrfurcht vor alten Bytes, sondern Pruefbarkeit. Wer aus
den dekodierten Feldern eine neue Datei zusammensetzt, bekommt eine Datei, die
vielleicht laedt - aber nicht mehr dieselbe ist. Und dann laesst sich nie mehr
unterscheiden, ob eine Abweichung gewollt war oder ein Fehler im Schreiber.

Mit dem Patchen gilt dagegen: **hat sich nichts geaendert, ist die Datei
byte-gleich.** Alles andere ist ein Fund. Genau das ist die Zusicherung, die
`--rundlauf` prueft:

    bafdump.exe big.lab --rundlauf
    -> 303 byte-gleich, 0 abweichend, 0 fehlgeschlagen

Neu gerechnet werden nur die abhaengigen Groessen: Blockgroessen, Zaehler, die
laufende Summe der Sound-Events, die beiden gespiegelten Offsets im Kopf.

Ein Detail, das leicht falsch geht: `_put_str` schreibt **NUL-terminiert, nicht
NUL-gefuellt**. Wer das Namensfeld auffuellt, aendert Dateien, an denen niemand
etwas geaendert hat. `Record::Str` macht es genauso.

Und: in den Face-Satz geht `v0`, nicht `v0Loaded`. In der Datei steht der
Rohwert, nicht das Ergebnis des zweiten Ladedurchgangs.

### BasisToEuler

Der Rueckweg der Rotation. Geprueft ueber alle 3.483 Knoten: Euler ->
Basis -> Euler -> Basis weicht um hoechstens 6,4e-16 ab.

Verglichen wird dabei die **Basis**, nicht das Winkeltripel - dieselbe Drehung
hat mehr als eine Euler-Schreibweise, und bei Kardanblockade faellt das Rollen
in die Gierung. Ein Vergleich der Winkel haette dort Abweichungen gemeldet, die
keine sind.

### applyScene arbeitet an Ort und Stelle

Positionen, Normalen und UVs werden aus der Szene uebernommen, die **Topologie
nicht**. Wer in Max Faces hinzufuegt oder loescht, bekommt eine Meldung statt
einer stillschweigend falschen Datei - und zwar je Mesh, ohne dass die anderen
darunter leiden.

Das ist Absicht. Ein Mesh mit geaenderter Topologie braucht neue Face-Saetze,
neue Indexbloecke und eine neue Texturvertex-Tabelle, und dabei geht jedes
Wortfeld verloren, das nicht aus der Szene rekonstruierbar ist. Solange der
Rundlauf nicht einmal in Max durchgelaufen ist, waere das der falsche
Zeitpunkt.

Zwei Feinheiten dabei:

**Normalen werden nur uebernommen, wenn Max VORGEGEBENE hat.** Sonst blieben
die der Datei stehen - die von Max aus den Glaettungsgruppen berechneten waeren
eine Verschlechterung, keine Aktualisierung.

**UVs werden mit der Cel-Groesse des Materials DIESES Face zurueckgerechnet.**
Dieselbe Zahl wie beim Lesen, sonst rutscht die Textur. Und die Zuordnung
Dreiecksecke -> Quellface-Ecke laeuft ueber den Vertexindex, weil die
Faechertriangulierung die Ecken umsortiert hat.

### Was noch fehlt

**Neue Meshes und neue Knoten anlegen.** Das Addon kann ein Blender-Objekt als
zusaetzliches Koerperteil einhaengen. Hier wird bisher nur ersetzt, nicht
hinzugefuegt.

## 6h. Stufe 8 - ins Archiv schreiben

Der Ablauf ist dreistufig, und die Reihenfolge ist der Punkt:

**1. Sicherung.** Gibt es noch keine `.bak`, wird das Original dorthin kopiert.
**Nur einmal** - eine zweite Sicherung wuerde die erste ueberschreiben und
damit genau den Stand vernichten, zu dem man zurueckwill.

**2. Schreiben.** Zwei Wege, und der erste ist der Normalfall:

* **Gleiche Groesse** - dann werden nur die Bytes des Eintrags an Ort und
  Stelle ersetzt. Die restlichen 115 MB werden nicht angefasst. Beim
  Bearbeiten eines vorhandenen Modells ist das fast immer der Fall, weil der
  Schreiber patcht statt neu zu bauen.
* **Andere Groesse** - dann wird das ganze Archiv neu aufgebaut, erst in eine
  Nebendatei. Ein Abbruch mittendrin laesst eine unfertige Nebendatei zurueck,
  aber ein heiles Archiv.

**3. Gegenprobe.** Das Ergebnis wird wieder eingelesen und **jeder** Eintrag
gegen den alten gehalten - nicht nur der geaenderte. Das ist die Stelle, an
der ein verrutschter Offset auffaellt. Erst wenn das stimmt, tritt die neue
Datei an die Stelle der alten.

Geprueft am echten 115-MB-`big.lab`: beide Wege, plus Wiederherstellen, plus
`cmp` gegen das Original danach - byte-gleich.

Zum Layout: das Retail-Archiv ist lueckenlos gepackt. 1336 von 1336
Uebergaengen ohne ein einziges Fuellbyte, und der letzte Eintrag endet genau
am Dateiende. Der Neuaufbau haelt sich daran.

Zwei Fallen beim Einbauen, beide echt gewesen:

**Das Archiv muss losgelassen werden, bevor es ersetzt wird.** Der eigene
Lesezugriff haelt die Datei sonst offen. `exportToArchive` schliesst, schreibt
und oeffnet wieder - und zwar auch dann wieder, wenn das Schreiben
fehlgeschlagen ist, sonst steht die Oberflaeche ohne Modellliste da.

**`entries` ist eine Referenz auf `archive.Entries()`.** Nach `Close()` ist
sie leer, und die Erfolgsmeldung stand deshalb auf "0 Eintraege". Die Zahl
wird jetzt vorher gemerkt.

## 6l. Das eigene Menue ist weg - Datei -> Importieren und Exportieren

Ab 1.1.0 traegt sich BAF in Max' **eigene** Dialoge ein, ueber eine
`SceneImport`- und eine `SceneExport`-Klasse. Das eigene "DH Tools"-Menue ist
ersatzlos entfallen.

Der Weg ist von XFBIN Import 2.0.0 uebernommen, samt der Korrektur aus 2.0.1 -
und die ist der Teil, den man ohne Vorerfahrung teuer bezahlt.

### Create() muss `new` liefern

    void* Create(BOOL) override { return new BafSceneImport(); }

Das Singleton-Muster, das die Autodesk-Doku zeigt, gilt fuer
**Utility**-Plugins - von denen gibt es genau eines. `SceneImport` und
`SceneExport` sind ein anderer Fall: Max legt eine Instanz an, fragt
`ExtCount`, `Ext` und die Beschreibungen ab und gibt den Speicher danach
wieder frei. Die Klassen haben kein `DeleteThis`, also loescht Max direkt.

Ein statisches Objekt zu loeschen zerlegt den Heap. Bei XFBIN ist 3ds Max
daran abgestuerzt, sobald man den Import-Dialog nur **geoeffnet** hat - noch
bevor irgendetwas gewaehlt war.

### Zwei Endungen beim Import

`.baf` fuer die lose Datei, `.lab` fuer das Archiv. Wer das Archiv waehlt,
bekommt die Modellliste - dann ist die Auswahl im Dialog nur noch die Frage,
wo das Archiv liegt. Die Oberflaeche holt den Pfad beim Oeffnen ueber
`pendingPath()` ab, und das Abholen leert ihn: er gilt fuer genau einen
Fensteraufbau.

`suppressPrompts` wird beachtet. Bei einem Import aus einem Skript oeffnet
sich kein Fenster; der Pfad bleibt hinterlegt.

### Ein Fenster, kein zweites

`DoImport` ruft das vorhandene Makro auf, statt eine eigene Oberflaeche zu
bauen. Zwei leicht abweichende Fassungen waeren die naechste Fehlerquelle -
und die eine bekaeme dann Korrekturen, die der anderen fehlen.

### Export braucht ein geoeffnetes Modell

`DoExport` uebernimmt erst die Szene und schreibt dann - wer ueber den Dialog
exportiert, erwartet den Stand der SZENE, nicht den der Datei, aus der
importiert wurde.

Ohne geoeffnetes Modell geht es nicht, und das ist kein Versaeumnis: der
Schreiber patcht die Aenderungen in die urspruenglichen Bytes zurueck, und
ohne die gibt es nichts zu patchen. Eine `.baf` aus einer beliebigen Szene zu
erfinden waere etwas ganz anderes als ein Export.

### Der Menue-Konflikt ist damit auch erledigt

Er loest sich von selbst: es gibt kein eigenes Menue mehr, also auch keine
zwei gleichnamigen Eintraege. Das MacroScript bleibt registriert und liegt
unter Customize -> Customize User Interface -> Kategorie "DH Tools", fuer
Toolbar oder Shortcut.

Der Abschnitt darunter bleibt als Notiz stehen, weil die Ursache lehrreich
ist - aber sie betrifft den Code nicht mehr.

## 6aj. Eine stille Voreinstellung, die nie stimmen kann (1.8.3)

    Put file into archive: TEST_C_deine_datei.baf: nicht im Archiv

`putFile` hat den DATEINAMEN als Eintragsnamen genommen. Der kann aber nie
stimmen: im Archiv heisst der Eintrag `obiwan.baf`, und wie die Datei auf der
Platte heisst, ist voellig gleichgueltig.

Im Skript stand:

    local sEntry = BafCpp.modelName()
    if (sEntry == "") do (sEntry = filenameFromPath sFile)

Solange ein Modell geladen war, ging es gut - die ersten beiden Testdateien
liefen deshalb durch. Danach nicht mehr.

**Der Fehler ist der Rueckfall selbst**, nicht die Bedingung darum herum. Eine
Voreinstellung, die nie richtig sein kann, ist schlimmer als gar keine: sie
sieht nach einer Entscheidung aus und ist keine.

Jetzt steht der Eintragsname **sichtbar in einem Feld**, vorbelegt aus dem
geladenen Modell oder aus der Herkunft der Szene - beides echte Namen aus dem
Archiv. Ist das Feld leer, passiert nichts und es steht da, warum.

Dazu prueft `putFile` vorher, ob es den Eintrag ueberhaupt gibt, und sagt in
der Meldung, was gemeint ist: der Name IM Archiv, nicht der auf der Platte.

## 6ai. putFile - ein Knopf zum Eingrenzen (1.8.2)

Im Spiel bleibt Obi-Wan in der Ruhelage. Das Modell erscheint also, das
Skelett wird angenommen - nur der Animationsblock wird verworfen.

An der Datei ist nichts zu finden: Ruhelage unberuehrt, 113 von 114 Clips
unveraendert, alle 303 Modelle des Archivs lesbar, keine einzige
Auffaelligkeit gegen die Konventionen des Bestands. Auch der Kopfname stimmt
(`obiwan.baf`, nicht der Dateiname auf der Platte).

Damit ist Weiterraten der falsche Weg. Es braucht einen Versuch, der
eingrenzt, ob der Fehler ueberhaupt im Inhalt steckt: eine **unveraenderte**
Datei ins Archiv legen und im Spiel nachsehen.

Dafuer gab es keinen Weg ausser einem Hexeditor - `exportToArchive` schickt
immer das geladene Modell durch den Schreiber. `putFile` legt jetzt eine
fertige `.baf` von der Platte in einen Archiveintrag, ohne Umweg.

Zwei Vorsichtsmassnahmen:

**Gelesen wird trotzdem.** Was nicht als `.baf` durchgeht, kommt nicht ins
Archiv. Eine kaputte Datei hineinzulegen waere sonst zu leicht.

**Der Eintragsname ist der des geladenen Modells**, nicht der Dateiname. Wer
eine obiwan-Datei unter dem Namen `obi_stand.baf` ablegt, findet sie im Spiel
nie wieder.

## 6ah. Welchen Clip zeigt die Szene ueberhaupt? (1.8.1)

Mit "Animation only" laeuft der Export - aber im Screenshot stand die Clipliste
auf `stnd-no1.key`, waehrend in der Szene `forcpush.key` lag. Das Werkzeug
wusste schlicht nicht, welcher Clip gerade bearbeitet wird.

Unter 114 Eintraegen den richtigen wiederzufinden ist Arbeit, die niemand
machen sollte - und ein Griff daneben ueberschreibt den falschen Clip.
Auffallen wuerde das erst im Spiel.

`buildAnim` schreibt den Clipnamen jetzt beim Import an den ersten Bone, als
`bafSceneClip` neben die uebrigen Rohdaten. Damit ueberlebt er Speichern und
Neustart, genau wie die Herkunft. Das Exportfenster liest ihn ueber
`sceneClip()`, waehlt den Clip in der Liste vor und schreibt darunter

    this clip is in the scene: forcpush.key

Im **Sequenzmodus** wird die Marke geloescht: dort steht kein einzelner Clip in
der Szene, und eine Marke waere dann eine Behauptung.

`SetProp` ersetzt dabei nur die eine Zeile und laesst den uebrigen Puffer in
Ruhe - dort stehen die Rohdaten aus Stufe 6, und die sind der Weg zurueck.

## 6ag. Die Ruhelage war es (1.8.0)

Nachgemessen an der Datei aus dem Spieltest:

**17 von 48 Knoten hatten eine andere Ruhelage.** Linker Oberarm 70 Grad
verdreht, rechter Unterarm 60, Brust 15, dazu die Huefte verschoben.

Das ist die Pose von Frame 0 der Szene, in die Datei geschrieben. Und es
erklaert, warum **gar keine** Animation mehr lief und nicht nur die geaenderte:
die Ruhelage ist das Fundament fuer jeden Knoten, den ein Clip nicht anfasst.

### Der Fehler war meiner

`applyScene` liest die Ruhelage bei Frame 0. Das ist richtig - solange dort
auch die Ruhelage steht. Bei einer Animation von Frame 0 bis 26 ist Frame 0
aber der **erste Frame der Bewegung**, und das ist der voellig normale Fall.
Ich hatte eine Annahme eingebaut, die niemand ausgesprochen hatte.

### Zaehlen statt hoffen

Der Schalter "Animation only" aus 1.7.3 haette geholfen - wenn man ihn setzt.
Ein Schalter, den man vergessen kann und dessen Auswirkung man erst im Spiel
merkt, ist aber keine Loesung.

`applyScene` zaehlt jetzt selbst: traegt auch nur ein Bone Keyframes, ist
Frame 0 keine Ruhelage, und die Ruhelage **bleibt stehen**. Dazu eine Warnung
im Bericht.

`setForceRestPose 1` hebt die Sperre auf, fuer den seltenen Fall, dass
wirklich eine neue Ruhelage gemeint ist. Standard aus - eine kaputte Ruhelage
merkt man erst im Spiel, und dann ist die Datei schon geschrieben.

Das Exportfenster sagt beim Untersuchen der Szene, was passieren wird:

    17 bones carry keys - rest pose stays untouched
    no keys on the bones - frame 0 is the rest pose

### Gezaehlt wird rekursiv

`CountKeys` steigt ueber die Unteranimationen ab, aus demselben Grund wie
`ClearKeys`: ein Position_XYZ haelt selbst keine Keys, sondern drei
Float-Controller darunter. Ohne die Rekursion haette der Zaehler ueberall
null gemeldet - und die Sperre haette nie gegriffen.

## 6af. "Animation only" (1.7.3)

Beim Aufschreiben des Ablaufs ist eine Falle aufgefallen, die vorher niemand
gesehen hat:

**`applyScene` liest die Ruhelage bei FRAME 0.**

Steht die Szene dort in einer Pose statt in der Ruhelage - was bei einer
Animation der Normalfall sein kann - wird diese Pose zur neuen Ruhelage des
Modells. Danach sitzt jeder Clip darauf auf, und das Modell steht schief.

Bei `forcpush.key` ging es gut, weil dessen erster Key auf der Ruhelage liegt.
Das war Glueck, keine Absicht.

Der neue Schalter **"Animation only"** ueberspringt `applyScene` ganz. Wenn
sich nur die Animation geaendert hat, gibt es dort ohnehin nichts zu tun:
Skelett und Meshes sind unveraendert, und sie zurueckzuschreiben fuegt nur
Rundungsrauschen hinzu - beim ersten Export waren das 1.476 Bytes im
Geometrieblock, alle ohne Grund.

Die Vorlage wird trotzdem geladen: ohne sie gaebe es nichts, worin die Keys
landen koennten.

## 6ae. Im Spiel laeuft keine Animation mehr (1.7.2)

Rueckmeldung nach dem ersten Test im Spiel: es geht **gar keine** Animation
mehr. Die Datei selbst ist strukturell in Ordnung - sie liest sich, der
Schreiber gibt sie byte-gleich zurueck, 113 der 114 Clips sind unveraendert,
und der geaenderte ist genau der, der geaendert werden sollte.

Beim Nachmessen gegen den Bestand kamen aber **drei Abweichungen** heraus, die
`captureAnim` bis 1.7.1 eingebaut hat.

### 1. Ein Key je Frame ist nicht, was das Format tut

Im Bestand kommen im Mittel **0,25 Keys auf einen Frame**, im Median 0,06 - der
Normalfall ist ein Key alle paar Frames, oft nur einer fuer den ganzen Track.
`forcpush.key` hatte im Original **105 Keys**, nach dem Aufnehmen **2.304**.

Folge: obiwan.baf wuchs um 123 KB, und die Gesamtzahl der Keys stieg von
**18.254 auf 20.453**. Das ist mehr, als irgendein Modell des Spiels
mitbringt - und obiwan war mit 18.254 vorher schon der Spitzenreiter.

Ob genau das dem Loader zu viel wird, kann ich von hier aus nicht beweisen. Es
ist die einzige Zahl an der Datei, die den Rahmen des Bestands verlaesst.

`captureAnim` duennt jetzt aus: ein Frame wird nur behalten, wenn die gerade
Linie zwischen den benachbarten Keys ihn um mehr als die Toleranz verfehlt.
Ueber alle 2.281 Clips gemessen bleiben bei 0,5 Grad **52 Prozent** der Keys
uebrig, bei einer groessten Abweichung von **0,93 Grad**.

### 2. Die Steigung ist Differenz GETEILT DURCH DEN ABSTAND

Nachgemessen an 21.607 Keypaaren des Bestands mit Abstand ueber einem Frame:
in **jedem einzelnen Fall** steht dort die Differenz durch den Abstand, in
keinem die blosse Differenz.

Bis 1.7.1 stand hier die Differenz. Das ging gut, solange jeder Frame einen Key
hatte - und waere mit dem Ausduennen aus Punkt 1 sofort falsch geworden. Zwei
Fehler, die sich gegenseitig verdeckt haben.

### 3. Die Winkel duerfen nicht entwrappt werden

Ich hatte in 1.5.0 eine Entwrappung eingebaut, damit eine Drehung ueber die
+-180-Grenze nicht zurueckschnappt. Gut gemeint, aber das Format loest das
anders:

    Differenz 359,39 Grad  ->  gespeicherte Steigung -0,61

Es nimmt den **kuerzeren Weg**. Die Entwrappung erzeugte stattdessen Betraege
bis 361 Grad und Differenzen von 1.080 Grad zwischen zwei Fassungen desselben
Keys - dieselbe Lage, drei volle Umdrehungen anders geschrieben.

Jetzt bleiben die Winkel so, wie `BasisToEuler` sie liefert, und die Steigung
wird auf +-180 gewrappt. Das Zurueckschnappen verhindert nicht die
Entwrappung, sondern genau diese gewrappte Steigung.

### Nebenbei: die Flags des letzten Keys

Im Bestand traegt der letzte Key eines Tracks in **46.824 von rund 51.000**
Faellen die Flags 0 - es gibt nichts mehr, wohin interpoliert werden koennte.
`captureAnim` setzte ueberall 3. Jetzt bekommt der letzte Key 0, passend zu
Steigung null.

## 6ad. Objekte in Layer einsortieren (1.7.1)

Ein Charakter bringt schnell hundert Objekte mit - Obi-Wan hat 48 Bones und
46 Meshes, und allein die Waffe besteht aus `gun01` bis `gun24` plus vier
Sabre-Teilen. Im Szenenbaum sucht man sich damit dumm.

Vier Layer, je Modell mit dessen Namen davor:

    obiwan Bones     obiwan Meshes     obiwan Guns     obiwan Sabers

### Ohne neue Plugin-Funktion

Das laeuft vollstaendig in MAXScript, ueber Daten, die seit Stufe 6 ohnehin an
den Objekten stehen: `bafNodeIndex` an den Bones, `bafMeshName` an den Meshes.
Angefasst wird nur, was diese Merkmale traegt - was sonst in der Szene steht,
geht uns nichts an. Es koennte ein Set sein, eine Kamera oder halbfertige
Arbeit.

### Waffen schlagen Bones und Meshes

Ein `gun01`-Bone landet in "Guns", nicht in "Bones". Das ist der Sinn der
Sache: die Waffe soll als EIN Stueck aus dem Weg, nicht ueber zwei Layer
verteilt.

Die Erkennung laeuft ueber den Knotennamen, und der ist im ganzen Bestand
derselbe: `gun01..gun24` und `sabre`/`sabreblad`/`sabreglow` haengen an einem
Knoten namens `weapon` unter der rechten Hand. **194 der 303 Modelle** tragen
einen `gun`-Namen, dazu `pistol` in 43 und `shot` in 21.

### Der Linter hat einen echten Fehler gefunden

Beim ersten Anlauf stand in `roBuild`:

    if roOptions.chkLayers.checked do

`roOptions` wird aber weiter unten deklariert, und MAXScript loest Namen
strikt von oben nach unten auf - dort waere es `undefined` gewesen. Also
derselbe Fehler wie in 1.1.0, nur ueber Rolloutgrenzen hinweg.

`PRUEFE_SCRIPTS.py` hat ihn sofort gemeldet, mit Zeilennummer. Der Schalter
laeuft jetzt ueber eine gemeinsame Variable, die beide Rollouts kennen.

## 6ac. Alles auf Englisch (1.7.0)

Beide Fenster und jede Meldung des Plugins sind jetzt englisch - wie bei XFBIN
Import.

### Was umgestellt wurde

**Die beiden `.mcr` vollstaendig**, neu geschrieben statt uebersetzt:
Beschriftungen, Statustexte, Meldungen und auch die Kommentare. Sie sind die
Dateien, die man beim Anpassen aufmacht; eine halb deutsche Erklaerung darin
waere schlimmer als gar keine.

Dabei sind die Rollouts auch sprechender benannt worden: `roQuelle` heisst
`roSource`, `roAktion` heisst `roBuild`.

**Alle 327 Zeichenketten des Plugins.** Die erscheinen in derselben
Oberflaeche - eine englische Statuszeile mit deutschem Text darin waere
schlechter als beides einzeln.

### Was deutsch geblieben ist

Die **Kommentare im C++-Teil** - rund 1.200 Zeilen. Das sind die
Entwicklungsnotizen, sie stehen niemandem im Weg und sind nicht Teil der
Oberflaeche. Dasselbe fuer DEVNOTES, die Pruefskripte und die Batchdateien.

### tools/uebersetze.py

Die Uebersetzung lief ueber eine Tabelle, nicht von Hand. Zwei Dinge daran
sind erwaehnenswert:

**Ersetzt wird nur INNERHALB von `L"..."`.** Ohne diese Einschraenkung haette
die Tabelle die Kommentare gleich mit zerlegt.

**Die Reihenfolge zaehlt.** Laengere Muster stehen vor kuerzeren, sonst
zerlegt `" Knoten"` das laengere `" Knoten, Faktor "` und uebrig bleibt Unsinn.

Das Skript bleibt liegen. Nicht weil es nochmal laufen soll, sondern weil die
Tabelle zeigt, welche Formulierung wofuer steht - wer eine neue Meldung
schreibt, findet dort den Ton.

### Zwei Anlaeufe

Der erste Durchgang hat **nichts** ersetzt: die Ersetzungstabelle stand in
einem Here-Dokument, und ein Apostroph in "the file's orientation" hat die
Python-Zeichenkette vorzeitig beendet. Der Fehler kam erst am Ende, nachdem
alles andere schon gelaufen war.

Der zweite Filter war zu eng - er suchte nach Umlauten und Woertern wie
"Modell", und Meldungen wie `"buildMeshes: erst buildSkeleton aufrufen"`
enthalten davon keins. Erst ein Filter auf deutsche Fuellwoerter hat die
letzten 26 gefunden.

## 6ab. Max startet nicht mehr: zwei Pakete, eine ID (1.6.2)

    Core interface named BafCpp from module (null) with a
    duplicate id of (0x41F26B07,0x1D9C3E55) will not be registered.

Danach ein Absturz beim Start. Autodesks eigener Artikel zu derselben Meldung
(dort fuer ArnoldFluidUtility) nennt die Ursache: eine **zurueckgebliebene
aeltere Kopie** des Plugins.

Genau das war es. In `ApplicationPlugins` lagen beide Pakete nebeneinander -
das alte `BafImport` und das neue `BafToolkit`. Max laedt beide DLLs, und beide
melden die Schnittstelle `BafCpp` mit derselben `Interface_ID` an.

Dass die IDs gleich geblieben sind, war beim Umbenennen **Absicht**: sie stehen
in vorhandenen Szenen, und ein Wechsel haette die unbrauchbar gemacht. Genau
deshalb kollidieren die beiden Fassungen aber hart - ein umbenanntes Paket mit
gleichen IDs ist fuer Max dasselbe Plugin, zweimal geladen.

### Der Fehler war meiner

Ich hatte das Aufraeumen in `DEINSTALLIERE.bat` gelegt und in der Antwort
erwaehnt. Beides hilft nicht: wer die neue Fassung installiert, hat keinen
Grund, vorher ein Deinstallationsskript zu suchen. Das gehoert in den
Installer.

`INSTALLIERE.bat` entfernt eine vorhandene `BafImport`-Installation jetzt
selbst, bevor es die neue kopiert - an beiden moeglichen Orten, unter
`%APPDATA%` und unter `%ALLUSERSPROFILE%`. Laesst sich der Ordner nicht
entfernen (Max laeuft noch), bricht die Installation ab, statt eine kaputte
Kombination zu hinterlassen.

### Die andere Meldung

Der Dialog zum OSL-Shader davor gehoert nicht dazu. Den zeigt Max nach jedem
harten Abbruch beim Start und meint den Shader, bei dem er gerade war -
"Continue Anyway" ist dort richtig.

## 6aa. Dieselbe Regel, andere Sprache (1.6.1)

    C3861: "ClipFrameToScene": Bezeichner wurde nicht gefunden

Alle zwoelf Jahrgaenge, gleicher Fehler. `captureAnim` kam in Stufe 7 zu
stehen, `ClipFrameToScene` ist eine dateilokale Hilfe aus Stufe 5 - und die
stand weiter unten.

Das ist **exakt die Regel, an der 1.1.0 schon einmal gescheitert ist**, nur in
der anderen Sprache: eine dateilokale Funktion muss in C++ vor ihrem ersten
Aufruf definiert sein, wie eine Rollout-Funktion in MAXScript vor ihrem
Aufrufer. Fuer MAXScript prueft `PRUEFE_SCRIPTS.py` das seit damals; fuer C++
gab es nichts.

Der Block steht jetzt oben bei den uebrigen Hilfen, mit einer Notiz, warum er
nicht dort steht, wo er thematisch hingehoert.

### Der Pruefer hat sich zuerst selbst ausgetrickst

`PRUEFE_SDK.py` prueft die Regel ab sofort mit. Beim ersten Anlauf fand er
nichts - auch nicht am Minimalbeispiel. Der Grund:

Ich hatte den anonymen Namensraum ueber `} // namespace` gesucht. Der Pruefer
laesst den Quelltext aber vorher durch `strip_comments` laufen, und dabei
verschwindet genau dieser Kommentar. Er suchte also nach etwas, das er selbst
weggeworfen hatte.

Jetzt wird die schliessende Klammer **gezaehlt** statt am Kommentar erkannt.
Der Gegentest laeuft in beide Richtungen: am Minimalbeispiel und am echten
Quelltext, wo er die Zeile nennt, die auch der Compiler genannt hat.

## 6z. Komplett neue Animationen anlegen (1.6.0)

`captureAnim` konnte bisher nur in einen VORHANDENEN Clip schreiben. Fuer einen
neuen fehlten der Descriptor und der Key-Header - und darin stehen Felder,
deren Bedeutung wir nicht kennen.

### Erst nachsehen, dann bauen

Statt zu raten habe ich alle 2.281 Clips des Bestands durchgezaehlt. Ergebnis:
es sind **genau drei** Felder, die wir nicht schreiben.

| Feld | Was drinsteht |
|---|---|
| Descriptor + 0x04 | in 2.276 von 2.281 Clips null, sonst eine Zahl in Zeigergroesse |
| Key-Header + 0x3c | Werte zwischen 12 und 116 Millionen |
| Track + 0x28 | dasselbe, ein Feld weiter |

Das sind **alte Zeiger** aus dem Prozess von 1999. Die Engine muss sie beim
Laden neu setzen - mit Adressen von damals koennte sie die Datei sonst gar
nicht benutzen.

Trotzdem werden sie **geklont statt genullt**. Eine Null waere eine Behauptung
darueber, was die Engine dort erwartet; ein Wert aus einem Clip, der
nachweislich laeuft, ist keine.

### Die uebrigen Voreinstellungen

Auch aus dem Bestand erhoben statt erfunden: `fade`, `loop`, `trackFlags` und
`specialUsage` kommen vom ersten Clip der Vorlage. Nur die **Typmaske** wird
fest auf `0xFFFF` gesetzt - "alle Koerperteile". Das ist mit 1.613 von 2.281
Clips der haeufigste Wert und die einzige Wahl, die keinen Knoten
stillschweigend auslaesst. Waere sie zu eng, stuende spaeter ein Arm still und
man wuesste nicht warum.

Ein Track je Knoten, in Knotenreihenfolge - dieselbe Form wie bei den
Retail-Clips. Die Bindung laeuft ueber den Index, der Name steht nur zur
Verstaendlichkeit dabei.

### Geprueft

Ueber alle 303 Modelle je zwei neue Clips angelegt, mit erfundener Bewegung
gefuellt, geschrieben und wieder gelesen: **606 Clips, 0 Fehlschlaege,
Abweichung der Keys 0.** Darunter auch der Ein-Frame-Fall, der uns in 1.2.6
schon einmal beschaeftigt hat.

Mitgeprueft: die **vorhandenen** Clips duerfen sich dabei nicht veraendern.
Tun sie nicht - die laufende Summe der Sound-Events und alle Blockgroessen
rechnet der Schreiber neu.

### In der Oberflaeche

Name, fps und Frameanzahl, dann "Neuen Clip anlegen". Der Knopf legt nicht nur
an, sondern uebernimmt gleich die Bewegung aus der Szene - alles andere waere
ein Zwischenschritt ohne Zweck.

Vorbelegt wird mit dem Animationsbereich der Szene und ihrer Framerate. Wer
gerade animiert hat, will fast immer genau das.

## 6y. Eigene Animationen exportieren (1.5.0)

### Die Luecke

Die Frage war: geht das ohne Probleme? Nein - `applyScene` hat die Animation
**gar nicht angefasst**. Es liest die Ruhelage der Bones bei Frame 0 und die
Geometrie, mehr nicht. Eine in Max gebaute Bewegung waere beim Export spurlos
verschwunden.

Das ist kein Versehen: waere es anders, wuerde jede Ruhelage-Aenderung von der
gerade eingestellten Pose ueberschrieben. Die beiden Dinge muessen getrennt
bleiben.

### captureAnim

Das Gegenstueck zu `buildAnim`. Es tastet die Bewegung ueber die ganze Laenge
eines Clips ab, ein Key je Frame, und schreibt sie in dessen Tracks zurueck.
Woher die Bewegung kommt, ist dabei egal - von Hand gesetzt, IK, ein
Constraint: gelesen wird die fertige Weltmatrix.

Drei Dinge, die dabei zaehlen:

**Die Winkel muessen fortlaufend gehalten werden.** `BasisToEuler` liefert
Werte aus `atan2`, also immer zwischen -180 und +180. Laeuft eine Drehung ueber
diese Grenze, springt der Wert um 360 Grad - und die Engine interpoliert
linear. Aus einer sauberen Drehung wuerde ein Zurueckschnappen ueber den ganzen
Kreis. Jeder Winkel wird deshalb um Vielfache von 360 auf den vorigen
zugezogen.

**Die Steigungen sind Differenzen.** Die Engine rechnet `pos + dpos * t`, wobei
`t` in Frames des Clips zaehlt. Bei einem Key je Frame ist die Steigung also
schlicht die Differenz zum naechsten. Der letzte Key bekommt null, sonst liefe
die Bewegung ueber das Ende hinaus weiter.

**Ein Key-Satz braucht keine Vorlage.** 0x38 Byte, und alle davon werden
beschrieben - anders als beim Clip-Descriptor, wo undurchsichtige Felder
stehen.

Geprueft ohne 3ds Max: alle **2.281 Clips** neu abgetastet, **2.119.481 Keys**
geschrieben, jede Datei danach wieder gelesen. Die Bewegung weicht um
hoechstens **6e-07** ab - Position 5,96e-08, Basis 5,93e-07.

### Das Exportfenster geht jetzt immer auf

Vorher lief der Export durch, sobald die Vorlage sich von selbst fand. Bequem,
aber man sah nie, was uebernommen wurde - und beim Export ist genau das die
Frage: welche Vorlage, welcher Clip, wird die Topologie umgebaut, halten die
Engine-Grenzen.

`DoExport` reicht den Zielpfad jetzt durch und oeffnet das Fenster. Dort steht
er als Ziel bereit, und "Als .baf schreiben" nimmt ihn ohne weitere Nachfrage.

Rueckgabe ist 1, obwohl noch nichts geschrieben ist: aus Max' Sicht hat das
Modul uebernommen. Eine 0 wuerde "Export module failure" ausloesen, und das
waere falsch - es ist ja nichts schiefgegangen.

Ausnahme: bei `suppressPrompts` (Aufruf aus einem Skript) hat ein Fenster
nichts zu suchen. Dann geht der automatische Weg, und die Rueckgabe sagt
ehrlich, ob er geklappt hat.

## 6x. Der erste Rundlauf durch Max - und die Umbenennung (1.4.0)

### Der Export stimmt

Erster echter Durchlauf: obiwan.baf importiert, exportiert, die Datei
zurueckgeschickt. Ergebnis:

| | |
|---|---|
| Groesse | 1.468.372 Byte, **genau wie das Original** |
| Struktur | 48 Knoten, 46 Meshes, 44 Materialien, 114 Clips - unveraendert |
| Knotenlagen | groesste Abweichung **0,000000** |
| Vertexpositionen | 645 verglichen, groesste Abweichung **1,5e-08** |
| Face-Normalen, UVs, Intensitaeten | **bitgenau gleich** |
| Vertexnormalen | 246 von 645 um **1,2e-07** verschoben |

Abweichend sind also 1.164 von 1.468.372 Bytes - 0,08 Prozent, und
ausschliesslich das letzte Mantissenbit einzelner Gleitkommazahlen.

Die Erklaerung ist harmlos und in beiden Faellen dieselbe: der Weg durch Max.
Die Positionen gehen mit dem Faktor 80 hinein und wieder heraus, die Normalen
werden von Max normalisiert. Beides sind Rundungen in der letzten Stelle eines
float, keine inhaltlichen Aenderungen.

Bemerkenswert ist eher, was **nicht** abweicht: alle 227 abweichenden Bereiche
liegen im Geometrieblock. Kopf, Palette, Materialien und der gesamte
Animationsblock sind byte-gleich - dort wurde nichts angefasst, und das Patchen
statt Neubauen haelt sie unveraendert.

### Warum sich das Exportfenster nicht geoeffnet hat

Weil es nicht musste. Die Szene kannte ihre Herkunft, `autoLoadTemplate` hat
die Vorlage nachgeladen, und `DoExport` ist durchgelaufen. Das ist der
Normalfall und soll so sein - ein Dateiexporter, der erst ein Fenster
aufmacht, waere laestig. Das Fenster ist fuer die Faelle da, in denen eine
Entscheidung noetig ist.

### Umbenannt zu BAF Toolkit

"BAF Import" stimmte nicht mehr - das Ding importiert, exportiert, schreibt ins
Archiv und baut Topologie um. Umbenannt wurden:

* `BafImport.dlu` -> `BafToolkit.dlu`
* `src/bafimport.*` -> `src/baftoolkit.*`, samt Klassen und Makros
* Paketordner und Installationsziel -> `BafToolkit`
* **Neuer UpgradeCode.** Max laedt je UpgradeCode nur ein Paket; ohne einen
  neuen waere das umbenannte nicht als eigenes erkannt worden.

Nicht umbenannt: die MaxScript-Schnittstelle heisst weiter **`BafCpp`**. Sie
steht in beiden Skripten an ueber siebzig Stellen, und ein anderer Name haette
nichts verbessert - nur eine Fehlerquelle geschaffen.

Die beiden `.mcr` heissen weiter `BafImport.mcr` und `BafExport.mcr`: das sind
die Fenster, und die heissen ja tatsaechlich so.

**Wichtig beim Umstieg:** `DEINSTALLIERE.bat` entfernt jetzt auch den alten
Ordner `BafImport`. Ohne das laegen beide Fassungen nebeneinander in
ApplicationPlugins, und Max wuerde beide laden.

## 6w. Dreimal denselben Fehler gesucht, der gar nicht mehr da war (1.3.1)

Nach 1.3.0 kam ein Screenshot mit einer Fehlermeldung, die es in 1.3.0 gar
nicht mehr gibt - der Text stammte wortwoertlich aus 1.2.5. Dasselbe war schon
zweimal vorher passiert: einmal fehlte der Import-Dialogeintrag, weil noch
1.0.1 lief, einmal war das Modell durchsichtig, weil noch 1.2.4 lief.

Das ist keine Nachlaessigkeit auf der anderen Seite, sondern ein Loch im
Werkzeug. Beim Bauen und Installieren kann leicht etwas danebengehen: die
`.dlu` ist gesperrt, weil Max noch laeuft, oder es wurde installiert ohne neu
zu bauen. Dann sieht man eine Meldung aus einer alten Fassung und sucht den
Fehler im neuen Code.

Drei Stellen sagen jetzt, welche Fassung wirklich laeuft:

**Jede Meldung des Plugins** traegt die Version am Ende - `[BafImport.dlu
1.3.1]`. Damit faellt es in dem Moment auf, in dem man die Meldung liest.

**Beide Fenster** schreiben beim Oeffnen eine Zeile in den Listener, mit
Plugin- UND Skriptversion nebeneinander.

**Der Plugin Manager** zeigt sie ohnehin, weil sie in `LibDescription` steht -
das ist der schnellste Weg zum Nachsehen, ohne irgendetwas zu oeffnen.

Der Versionsvergleich im `.mcr` gab es schon vorher, aber der greift nur, wenn
Skript und Plugin auseinanderlaufen. Laufen beide auf demselben alten Stand,
meldet er zu Recht nichts.

## 6v. Import und Export getrennt (1.3.0)

Der Einwand war berechtigt und schaerfer, als er zuerst klang: **der Export
darf nicht davon abhaengen, dass dieselbe Max-Sitzung vorher importiert hat.**
Wer eine Szene speichert, Max schliesst und am naechsten Tag weiterarbeitet,
hatte bisher keine Chance.

### Die Loesung stand schon halb da

Seit Stufe 6 traegt jede importierte Szene ihre Herkunft an den Objekten mit
sich - `bafNodeIndex` an den Bones, `bafMeshIndex` an den Meshes, am ersten
Bone der Modellname. Das ueberlebt Speichern und Neustart. Der Export hat es
nur nie gelesen.

`scanScene()` liest es zurueck und stellt daraus die Zuordnung wieder her. Der
entscheidende Unterschied zu vorher: **die Zuordnung laeuft ueber die
Rohdaten, nicht ueber Handles.** Handles gelten nur innerhalb einer Sitzung.

Dazu kommt ein Feld, das nicht aus der Datei stammt: `bafArchive`, der Pfad des
Archivs. Ohne ihn findet der Export die Vorlage spaeter nicht wieder.
`autoLoadTemplate()` holt sie damit selbst nach - der Anwender merkt nichts
davon.

### Der Begriff "Vorlage"

Weil der Schreiber patcht statt neu zu bauen, braucht er die urspruenglichen
Bytes. Das ist keine Einschraenkung, die man wegdiskutieren sollte - es ist der
Grund, warum die Zusicherung "ohne Aenderung byte-gleich" ueberhaupt gilt.

Drei Faelle, und alle drei enden bei einer Vorlage:

1. **Modell noch geladen** - nichts zu tun.
2. **Szene kam aus einem Import**, auch nach Neustart - Vorlage wird
   nachgeladen.
3. **Eigenes Modell** - der Anwender waehlt die Vorlage. Kopf, Palette und
   Materialien kommen von dort, Skelett und Geometrie aus der Szene.

Fall 3 ist damit derselbe Weg wie Fall 2, nur von Hand. Das war der Punkt, an
dem sich die Sache aufgeloest hat: es braucht keinen zweiten Mechanismus.

### Die beiden Fenster

**BafImport.mcr** verliert die ganze Export-Gruppe und schrumpft entsprechend.

**BafExport.mcr** faengt nicht mit einer Dateiauswahl an, sondern mit einem
**Befund**: was steht in dieser Szene, und woher kommt es? Erst danach die
Frage, wohin. Der Rundlauf und das Wiederherstellen sind mitgewandert - sie
gehoeren zum Export, nicht zum Import.

`DoExport` versucht weiterhin den automatischen Weg. Klappt er nicht, oeffnet
sich jetzt das **Exportfenster** statt nur einer Absage, damit die Vorlage von
Hand gewaehlt werden kann.

### Was weiterhin fehlt

Ein Export **ganz ohne Vorlage**. Dafuer braeuchte es eine Farbquantisierung
beliebiger Max-Texturen auf 8-Bit-Paletten und belegte Standardwerte fuer jedes
undurchsichtige Wortfeld. Beides machbar, aber ein eigenes Kapitel - und mit
einer Vorlage kommt man beim Modden praktisch ueberall hin.

## 6u. Absturz bei Ein-Frame-Clips, und ein selbstgemachtes Exportproblem (1.2.6)

### EXCEPTION_INT_DIVIDE_BY_ZERO beim Clip "temp.key"

Der gewaehlte Clip hatte **einen einzigen Frame** (`30 fps, 1 -> 1`). Daraus
wurde

    ip->SetAnimRange(Interval(0, 0));

also ein Animationsbereich der Laenge null. Max rechnet intern durch diese
Laenge - fuer die Zeitleiste, fuer Ticks je Pixel - und das ergibt eine
Integer-Division durch null mitten in Max. Von aussen sieht es aus wie ein
Fehler im Plugin, und die Meldung nennt die MAXScript-Zeile mit dem Aufruf.

Nachgezaehlt: **152 der 2.281 Clips im Bestand haben hoechstens einen Frame.**
Das sind keine Ausreisser, das ist ein normaler Zustand - Platzhalter wie
`temp.key`, oder Posen wie `pushblok`.

Der Bereich ist jetzt mindestens einen Frame lang. Ein try/catch haette hier
nichts geholfen: eine SEH-Ausnahme aus fremdem Code faengt ein C++-catch nicht.

### "Export module failure" - der eigentliche Grund

Der lag nicht im Export, sondern im Schliessen des Fensters:

    on roBafImport close do
    (
      BafCpp.close()
      BafCpp.closeArchive()
    )

Danach kannte das Plugin kein Modell mehr - und "Datei -> Exportieren" schlug
fehl, obwohl die Szene vollstaendig dastand. Der Anwender schliesst das Fenster
ja gerade, WEIL er in der Szene arbeiten will; genau dann das geladene Modell
wegzuwerfen ist die falsche Reaktion.

Beim Schliessen wird jetzt nichts mehr freigegeben. Das Modell kostet wenige
hundert Kilobyte und wird beim naechsten Laden ohnehin ersetzt. Aufgeraeumt
wird, wenn ein anderes Modell geladen wird - nicht, wenn ein Fenster zugeht.

## 6s. Alles durchsichtig (1.2.5)

Die Texturen waren da, aber das Modell war halb durchsichtig - dunkle Stellen
mehr als helle.

Der Grund: ich hatte die TGA als **Transparenz-Map** auf jedes deckende
Material gelegt. Und Max nimmt fuer eine Transparenz-Map standardmaessig die
**RGB-Helligkeit**, nicht den Alphakanal. Schwarz ist dort durchsichtig, weiss
deckend - eine dunkle Textur loest sich also auf.

Zwei Aenderungen:

**Der Mono-Ausgang der Bitmap liefert jetzt das Alpha**, nicht die Helligkeit:

    texture->SetAlphaSource(ALPHA_FILE);
    texture->SetAlphaAsMono(TRUE);

**Und eine Transparenz-Map bekommt nur noch, wer eine braucht** - also ein
Material, dessen Cel wirklich einen Farbschluessel traegt. Nachgezaehlt:
**500 von 2.725 Cels im ganzen Bestand, also 18 Prozent.** Die anderen 82
Prozent hatten eine Map, die sie nicht brauchen, und die hat sie aufgeloest.

## 6t. "Export module failure"

Max' Meldung, wenn `SceneExport::DoExport` eine 0 zurueckgibt. Ausgeloest hat
sie ein Export ohne geladenes Modell.

Das ist kein Fehler im Code, aber eine schlechte Auskunft. Jetzt kommt vorher
eine eigene Meldung mit dem echten Grund und dem Weg drumherum. Max haengt sein
"Export module failure" trotzdem an - das laesst sich bei Rueckgabe 0 nicht
abstellen, und eine 1 zurueckzugeben waere gelogen, weil nichts geschrieben
wurde.

Der Aufruf der Oberflaeche im Fehlerfall ist entfallen: der Anwender hat gerade
einen Dateidialog hinter sich, ein Fenster mehr hilft ihm nicht.

## 6r. MSTR statt const MCHAR* ab Max 2025 (1.2.4)

Die Jahrgaenge 2016 bis 2024 gingen durch, 2025 bis 2027 nicht:

    C2280: "WStr::operator const wchar_t *(void) &&":
    Es wurde versucht, auf eine geloeschte Funktion zu verweisen

Betroffen waren die beiden `GetDir`-Aufrufe in `EnsureTextureDir`. **Ab
3ds Max 2025 liefert `GetDir` eine `MSTR` ALS WERT**, vorher einen
`const MCHAR*`.

Das Bemerkenswerte daran: die Umwandlung ist nicht verschwunden, sie wurde
**absichtlich geloescht**. Die SDK-Doku zu `WStr` sagt es wortwoertlich:

> Delete these two overloads to prevent accidental dangling pointers from
> temporary strings.

Ein direkt zugewiesener Zeiger zeigte naemlich auf eine Zeichenkette, die am
Ende der Zeile schon freigegeben ist - ein Fehler, der frueher unbemerkt durch
alle Builds gelaufen waere und sich erst zur Laufzeit als Muell im Pfad
gezeigt haette. Der Compilerfehler ist hier also der Schutz, nicht das Problem.

Die Loesung gilt fuer beide Seiten der Aenderung: die `MSTR` benannt halten und
ueber `.data()` weitergehen. Bei den alten SDKs wird sie aus dem Zeiger gebaut,
bei den neuen kopiert, und in beiden Faellen lebt sie lang genug.

    MSTR images;
    images = ip->GetDir(APP_IMAGE_DIR);
    if (images.length() > 0) base = images.data();

`PRUEFE_SDK.py` kennt das jetzt und meldet eine `const MCHAR*`-Zuweisung aus
`GetDir`, `GetMaxBuildNumber` oder `GetTextFromFloat`. Gegentest laeuft.

## 6q. Texturen unsichtbar, und der Laengenfaktor (1.2.3)

### Das Material hatte die Textur, der Viewport zeigte sie nicht

    standard->SetActiveTexmap(texture);
    standard->SetMtlFlag(MTL_TEX_DISPLAY_ENABLED, TRUE);

Ohne diese beiden Zeilen ist die Bitmap zwar im Diffuse-Slot, aber das Modell
bleibt grau - und im Materialeditor sieht alles richtig aus. Es ist derselbe
Schalter wie der Knopf "Show Shaded Material in Viewport", nur von hier aus
gesetzt.

Das ist die unangenehme Sorte Fehler: nichts meldet sich, und die Stelle, an
der man nachsieht, sieht in Ordnung aus.

### Ohne gewaehlten Ordner gab es gar keine Texturen

Vorher wurde `exportTextures` uebersprungen, wenn kein Texturordner gesetzt
war - und die Materialien bekamen dann keine Bitmap. Das Modell war grau, ohne
dass irgendwo stand warum; man muss erst wissen, dass da ein Ordner fehlt.

`EnsureTextureDir()` legt jetzt selbst einen an: den Bilderordner des Projekts
(`APP_IMAGE_DIR`), mit einem Unterordner `BAF_<modell>`. Wer einen anderen
will, waehlt ihn weiterhin.

`setTextureDir ""` setzt dabei **nicht** zurueck, sondern fragt nur ab - so
kann die Oberflaeche den automatisch gewaehlten Ordner anzeigen, ohne ihn zu
kennen.

### Der Laengenfaktor steht jetzt auf 80

Die Modelle sind in Einheiten von rund 0,8 hoch. Mit Faktor 1 steht in Max eine
Figur von 0,8 Einheiten da - kleiner als das Raster und praktisch nicht zu
bearbeiten. Mit 80 werden daraus 64, und das ist ungefaehr die Groesse, die
Max' eigener Biped mitbringt.

Auf den Export wirkt sich das nicht aus: `applyScene` teilt durch denselben
Faktor, mit dem gebaut wurde.

## 6p. Der Animationsfehler in 1.2.1: kein einziger Keyframe

Die Animationen kamen nicht an. Der Dump aus Max half dabei nicht weiter - er
zeigt die DATEIDATEN, und die waren in Ordnung: von 54.203 Zeilen wichen nur
177 ab, und die stammten alle aus einem `applyScene`-Rundlauf (Rundungsrauschen
in `bafBindEuler` und 31 Vertexpositionen). Die Keyframe-Zeilen waren
zeichengleich.

Die Ursache stand in XFBIN Import, in einem Kommentar, den ich beim Schreiben
von Stufe 5 nicht gelesen hatte:

    SuspendAnimate();
    AnimateOn();

**`Control::SetValue` legt nur dann einen Keyframe an, wenn Max im
Animationsmodus ist.** Sonst setzt es den KONSTANTEN Wert des Controllers. Es
gibt dabei keine Fehlermeldung und keinen Rueckgabewert, der davon erzaehlt -
`buildAnim` meldete brav die Zahl der gesetzten Keys, und keiner davon
existierte. Der Bone landete stumm auf der zuletzt gesetzten Lage.

Jetzt als `AnimateGuard` mit `SuspendAnimate()` davor, gepaart in einem RAII-
Guard. Ein vergessenes `AnimateOff()` liesse Max im Animationsmodus zurueck -
der rote Rahmen um den Viewport bliebe stehen, und jede weitere Aenderung des
Anwenders wuerde aufgezeichnet.

### Dabei ein zweiter Fehler aufgefallen

`ClearAnim` rief `GetKeyControlInterface` auf dem Positions- und dem
Rotationscontroller auf. Ein **Position_XYZ**-Controller haelt aber selbst
keine Keys, sondern drei Float-Controller darunter - `GetKeyControlInterface`
liefert dafuer NULL, und das `SetNumKeys(0)` lief ins Leere. Die alten Keys
waeren stehengeblieben und der naechste Clip haette sich darueber gelegt.

`ClearKeys` steigt jetzt rekursiv ueber die Unteranimationen hinab und setzt am
TM-Controller selbst an, damit auch die Skalierung erwischt wird.

### Der Pruefer kennt das jetzt

`PRUEFE_SDK.py` meldet `CTRL_ABSOLUTE` ohne ein `AnimateOn` in derselben
Uebersetzungseinheit. Beim Einbauen hat er prompt die korrigierte Datei
gemeldet - der Guard steht im HEADER, nicht in der `.cpp`. Der Pruefer liest
jetzt die eigenen Header mit; sonst haette er eine richtige Datei angemeckert
und man haette angefangen, ihn zu ignorieren.

## 6o. Drei Funde beim ersten Lauf von 1.2.0

### 1. Der Note Track: falsche Funktionen

    Type error: Call needs function or class, got: undefined

Im Callstack standen `noteKey` und `appendItem` als **Locals** mit dem Wert
`undefined`. Das ist die Art, wie MAXScript einen unbekannten Namen behandelt:
es legt still eine leere lokale Variable an, und der Aufruf scheitert erst zur
Laufzeit.

`noteKey` ist keine frei aufrufbare Konstruktorfunktion, und `appendItem` taugt
fuer ein NoteKeyArray nicht. Richtig ist `addNewNoteKey`: es legt den Key **im
Array** an und gibt ihn zum Beschriften zurueck.

    local k0 = addNewNoteKey NT.keys iStart
    k0.value = sName

Dieselbe Fassung steht in XFBIN Import und laeuft dort seit Langem - ich hatte
sie nicht nachgeschlagen, sondern aus dem Gedaechtnis geschrieben.

Jetzt werden zwei Keys je Clip gesetzt, am Anfang und am Ende. Dieselbe Form
wie bei AnimMerge: wer den einen liest, liest den anderen.

### 2. Die Oberflaeche klebte zusammen

Der Grund ist die Regel, die im Kopf derselben Datei steht: `across:N` teilt in
N **gleich breite** Zellen. Bei 266 Breite sind das 133 je Zelle - und ein
`edittext` mit `width:176` ragt in die zweite Zelle hinein, der Knopf daneben
liegt darauf.

Behoben, indem die betroffenen Paare untereinander stehen statt nebeneinander,
und alle verbliebenen `across`-Controls unter der Zellenbreite bleiben. Dazu
Abstandshalter zwischen den Gruppen und etwas mehr Breite: 592 statt 560, die
Spalten 288 statt 272, Hoehe 470 statt 400 - damit ist auch der Rollbalken in
der rechten Spalte weg.

### 3. Die Figur stand mit dem Ruecken zum Betrachter

Das war kein Fehler, sondern die Konvention des Formats - aber unbrauchbar.

Nachgeprueft an der **Fussgeometrie**: der Knoechel liegt im Ursprung des
Fussknotens, und die Zehen reichen bei allen geprueften Modellen deutlich
weiter nach +Y als die Ferse nach -Y. Die Engine schaut also nach +Y, passend
zu `EulerBasis`, wo `forward` bei Nulldrehung genau (0,1,0) ist.

Max' Frontansicht blickt ebenfalls nach +Y. Ein Charakter mit Blickrichtung +Y
steht dort mit dem Ruecken zum Betrachter. In Blender ist es genauso; das Addon
dreht auch nichts.

`setFaceFront` (Standard AN) dreht das ganze Rig um 180 Grad um Z. Zwei Dinge
dabei:

**Gedreht wird beim Bauen die ganze Weltmatrixliste**, nicht nur die Wurzel -
die Weltmatrizen sind dort schon ausgerechnet, und eine Drehung um den Ursprung
wirkt auf alle gleich. Bei den **Animationskeys** dagegen nur der Wurzelknoten:
ein Kind liegt relativ zu seinem Elternteil, und der ist schon gedreht.

**`applyScene` rechnet die Drehung wieder heraus**, ebenfalls nur beim
Wurzelknoten. Geprueft ueber alle 303 Modelle: Ruhelage drehen, lokale Lagen
zurueckrechnen, Drehung herausnehmen, wieder in Euler wandeln - groesste
Abweichung der Ruhelage 4,4e-16. Die Datei bleibt also unveraendert, auch mit
eingeschalteter Drehung.

## 6n. Die Oberflaeche in zwei Spalten (1.2.0)

Die alte Fassung war 1.500 Pixel hoch - auf einem 1080p-Schirm passte sie nicht
aufs Bild. Neun Gruppen untereinander, vierzehn gleich aussehende Knoepfe, keine
erkennbare Reihenfolge, und "Ins Archiv schreiben" stand da, bevor ueberhaupt
ein Modell geladen war.

Jetzt: **560 x ca. 450**, zwei Spalten.

    links    was geladen ist   - Archiv, Suche, Modellliste, Clips
    rechts   was man damit tut - Zahlen, Aufbauen, Zurueckschreiben
    unten    eine Statuszeile

Drei Entscheidungen dahinter:

**Eine Hauptaktion.** "Modell aufbauen" ist hoeher als die anderen und macht
Skelett, Meshes, Texturen und Materialien in einem Zug. Die vier Einzelschritte
stehen darunter, halb so breit.

**Die Clipliste zeigt die umgerechnete Laenge.** `stnd-no1  10 fps  29 -> 87`
heisst: 29 Frames im Clip werden 87 in der Szene. Das will man sehen, BEVOR man
baut - nicht danach, wenn es "irgendwie zaeh" aussieht.

**Werkzeuge sind Werkzeuge.** Rundlauf, die drei Gegenproben, Dump und
Protokoll liegen in einem zugeklappten Rollout. Dasselbe fuer die Optionen.

### Warum das den Aufbau des Skripts aendert

MAXScript hat keine Spalten. Zwei nebeneinanderliegende Bloecke gehen nur ueber
`subRollout` - oder ueber absolute Positionen, und die skalieren nicht mit der
Windows-Anzeigeskalierung.

Damit sind die Rollouts getrennte Gebilde, und die Hilfsfunktionen muessen
Controls in beiden erreichen. Nach der Regel aus 1.1.0 ginge das gar nicht.

Der Ausweg steht in der MAXScript-Doku selbst, unter "Visibility of Locals,
Functions, Structures and User-Interface Items in Rollout Code": Namen vorher
als `local` **anmelden**. Eine so angemeldete Variable ist zunaechst
`undefined`, gilt MAXScript aber als bekannt - es laeuft darueber hinweg und
traegt den Wert nach, sobald die Definition kommt.

    local SetStatus, ReportError, ...     -- anmelden
    rollout roQuelle ...                  -- darf sie benutzen
    rollout roAktion ...
    fn SetStatus sText = ( ... )          -- erst hier definieren

`PRUEFE_SCRIPTS.py` kennt diese Ausnahme jetzt: eine Vorwaertsreferenz ist nur
dann ein Fund, wenn der Name **nicht** vorher angemeldet wurde. Ohne das haette
er 30 Stellen gemeldet, an denen nichts ist - und man haette angefangen, ihn zu
ignorieren.

## 6m. Der Fehler in 1.1.0: Controls vor ihrer Deklaration

Beim ersten Oeffnen eines Modells:

    Unknown property: "text" in undefined
    BafImport.mcr, Zeile 202

Die Zeile war `edtArchive.text = sPath` in `LoadArchive`. Und die Ursache
steht seit der ersten Fassung im Kopf derselben Datei:

> MAXScript loest Namen in einem Rollout strikt von oben nach unten auf.

Ich hatte die Regel auf **Funktionen** bezogen - eine weiter unten definierte
Funktion ist beim Aufruf undefined. Sie gilt aber genauso fuer **Controls**:
eine Funktion, die weiter oben steht als das Control, das sie anspricht, sieht
dort `undefined`.

Betroffen waren sieben Controls: `edtArchive`, `edtFilter`, `edtInfo`,
`lblLimits`, `lblStatus`, `lbxClips`, `lbxModels`.

Behoben durch Vertauschen der beiden Bloecke: **erst die Controls, dann die
Funktionen, dann die Ereignisse.** Das ist ohnehin der uebliche Aufbau eines
Rollouts; ich hatte die Hilfen nach oben gestellt, weil sie so leichter zu
lesen sind.

### Warum der Linter das nicht gemeldet hat

`PRUEFE_SCRIPTS.py` kannte die Vorwaertsreferenz zwischen Funktionen und hat
die Datei sauber gemeldet - die halbe Regel eben. Jetzt prueft er beides. Der
Gegentest laeuft: Bloecke wieder vertauscht, vier der sieben Stellen sofort
gemeldet (die restlichen drei stehen in denselben Funktionen).

Das ist der zweite Fall dieser Art nach XFBIN 2.1.6, und das Muster ist
dasselbe: der Pruefer kannte eine Regel nur zur Haelfte und hat deshalb
Sicherheit vorgetaeuscht, wo keine war.

## 6i. Der Menue-Fehler in 0.8.0 (historisch)

Beim ersten Lauf in Max:

    [BAF] Menue konnte nicht angelegt werden:
    -- Unknown property: "CreateAction" in undefined

Die Ursache war meine: die Submenu-GUID in `BafMenu_2025_2027.ms` war
absichtlich dieselbe wie bei XFBIN Import, damit beide Werkzeuge in ein
gemeinsames "DH Tools" landen.

Das geht nicht. `CreateSubMenu` **legt an und sucht nicht**; fuer eine schon
vergebene GUID kommt `undefined` zurueck, und der `CreateAction`-Aufruf darauf
scheitert. Die alte `menuMan`-API konnte ein vorhandenes Menue ueber
`findMenu` holen - die neue kann das nicht, und eine Suchfunktion ist in der
MAXScript-Doku zu 2025 nicht beschrieben.

Jedes Paket hat jetzt seine eigene Submenu-GUID, und der Rueckgabewert wird
geprueft statt darauf zugegriffen. Folge: laufen beide Werkzeuge nebeneinander,
stehen zwei Eintraege "DH Tools" in der Leiste. Haesslich, aber es
funktioniert - und besser als ein Menue, das gar nicht erst entsteht.

Erledigt hat sich das in 1.1.0 auf anderem Weg: das eigene Menue ist ganz
entfallen, BAF steht jetzt in Max' eigenen Dialogen. Damit gibt es nichts mehr
zu teilen.

## 6j. Stufe 9 - Topologie umbauen

Mit `setRebuild 1` wird ein Mesh mit geaenderter Topologie nicht mehr
uebersprungen, sondern **komplett neu gebaut**: Positionen, Normalen,
Intensitaeten, Faces und der Texturvertex-Vorrat entstehen aus der Szene.
Nichts aus den urspruenglichen Arrays bleibt stehen, weil nichts darin noch
zu dieser Topologie gehoert.

Erhalten bleiben nur die undurchsichtigen Wortfelder je Face, geerbt vom
ersten Face des Mesh - naeher an dessen ueblichem Wert als null.

Geprueft ohne 3ds Max: alle 303 Modelle durchtrianguliert (46.466 Faces werden
58.084), Texturvertex-Vorrat neu aufgebaut (57.434 werden 55.754 - das
Zusammenfassen gleicher Texel spart mehr ein, als die Triangulierung kostet),
dann geschrieben und wieder gelesen. **0 Fehlschlaege.**

### Der Fund bei den Huellkugeln

Beim Einbauen habe ich nachgemessen, wie die mitgelieferten Kugeln zu einer
nachgerechneten stehen. Das Ergebnis hat die Voreinstellung geaendert:

**Von 3.131 Meshes sind 3.127 mit einer Kugel ausgeliefert, die GROESSER ist
als die eng anliegende - im Schnitt um 16 Prozent. Keine einzige ist kleiner.**

Die Kugeln haben also mit Absicht Luft. Nachrechnen wuerde sie alle
schrumpfen, und zwar auch bei Meshes, an denen niemand etwas geaendert hat.
Deshalb ist `setRecomputeBounds` standardmaessig AUS und das Nachrechnen die
Ausnahme, nicht die Regel.

Am **Kopfknoten** bleibt die Kugel sogar dann stehen, wenn der Schalter an
ist. Der Leser der KI-Blickrichtung transformiert die mitgelieferte Kugel und
rechnet sie nicht nach; eine geschrumpfte Kugel dort ist die bekannte Ursache
dafuer, dass die Blickrichtung einfriert. Welcher Knoten das ist, steht als
Nackenverweis im Kopf (hdr+0xf1).

Nebenbei: in 189 der 303 Modelle zeigt dieser Verweis auf Knoten 0, also den
Wurzel-Dummy. Der traegt kein Mesh, damit greift die Ausnahme dort nie - was
richtig ist, aber man sollte es wissen, bevor man sich ueber die Zahl wundert.

### Normalen immer neu normalisieren

Nicht aus Ordnungsliebe. Ein Decimate oder eine Bearbeitung hinterlaesst
Vektoren, die nicht mehr die Laenge eins haben, und ein Vertex, den kein Face
beruehrt, hat gar keine. Die Engine teilt durch die Laenge - und das NaN
daraus landet als Zeigerindex im Speicher. `NormalizeOrUp` faengt das mit
(0,0,1) ab.

### Die Grenzen werden gemeldet, nicht erzwungen

Wer ueber 0x200 Vertices oder Faces kommt, bekommt eine Warnung, und die Datei
wird trotzdem geschrieben. Der Loader des Spiels lehnt sie dann ab - aber das
ist die Entscheidung dessen, der das Modell baut, nicht die des Exporters.

## 6k. Der erste Build gegen das 2016er SDK

Vier Fehlerarten, alle im Plugin-Teil - der Parser und der Schreiber gingen
durch. Drei davon waren Formfehler, einer war ein echter Bug.

### Der echte Bug: zwei Handle-Welten vermischt

`boneHandles_`, `meshHandles_` und `boundsHandles_` hielten `AnimHandle` und
wurden mit `Animatable::GetAnimByHandle` nachgeschlagen. Gefuellt wurden sie
aber aus `INode::GetHandle()` - und die SDK-Doku sagt dazu ausdruecklich:

> Do not confuse Node Handles with Animatable Handles: they are actually quite
> different.

Ein Knoten-Handle ist eine dauerhafte, mit der Szene gespeicherte Nummer und
gilt nur fuer Knoten. Ein AnimHandle ist ein Instanzzaehler, der nur innerhalb
einer Max-Sitzung gilt, dafuer aber fuer jedes Szenenelement. Der eine Wert in
der anderen Welt nachgeschlagen trifft bestenfalls nichts.

Und weil beide Funktionen einen Zahlentyp liefern, **sagt der Compiler dazu
nichts**. Aufgefallen ist es nur, weil `MultiMtl` kein `GetHandle()` hat - ein
Material ist ein Animatable, aber kein Knoten. Ohne diesen Zufall waere der
Fehler erst in Max aufgetaucht, als verschwundenes oder vertauschtes Objekt.

Jetzt: Knoten ueber `Interface::GetINodeByHandle`, Materialien ueber
`Animatable::GetHandleByAnim` und `GetAnimByHandle`.

(Derselbe Punkt gilt fuer `boneHandles_` in XFBIN Import - dort steht `ULONG`,
also der richtige Typ; nachzusehen waere, womit dort nachgeschlagen wird.)

### Die drei Formfehler

**`const Face&` geht nicht.** `Face::getVert` und `Face::getMatID` sind im SDK
nicht als const deklariert und ueber eine const-Referenz deshalb nicht
aufrufbar. Die Meldung lautet "this-Zeiger kann nicht von const Face in Face &
konvertiert werden", was nicht danach klingt, dass ein `const` zu viel
dasteht.

**`DotProd` gibt es fuer `Quat` nicht**, nur fuer `Point3` und `Point4`. Das
Skalarprodukt zweier Quaternionen wird von Hand gerechnet - mit allen VIER
Komponenten, das `w` gehoert dazu.

**`MultiMtl::GetHandle()` existiert nicht.** Siehe oben.

### tools/PRUEFE_SDK.py

Neu, und alle drei Pruefungen stammen aus genau diesen Fehlschlaegen. Der
Gegentest laeuft: alle drei Fehler wieder eingebaut, alle drei gefunden.

Beim Einbauen hat der Pruefer prompt neun Stellen im Parser gemeldet, an denen
nichts war: der Parser hat eine **eigene** Struktur namens `Face`, und
`const baf::Face&` ist dort voellig richtig. Die Pruefung laeuft jetzt nur in
Dateien, die ueberhaupt am SDK haengen, und ignoriert alles mit einem
Namensraum davor.

## 7. Was direkt uebernommen werden kann

Schon uebernommen und angepasst: `BAUE_ALLE.bat`, `INSTALLIERE.bat`,
`DEINSTALLIERE.bat`, `PackageContents.xml`, die beiden Menue-Skripte,
`tools/PRUEFE_API.py`, `tools/PRUEFE_SCRIPTS.py` und die `CMakeLists.txt`.

Noch offen, wenn sie gebraucht werden: `tools/PRUEFE_MELDUNGEN.py`,
`tools/VERSION_SETZEN.py`.

### PackageContents.xml - gegen die Spezifikation geprueft

Die Datei war zuerst nur eine Kopie mit getauschten Namen. Nachtraeglich gegen
die "Autodesk Application Plug-in Package Format Specification" gehalten, mit
drei Ergebnissen, die vorher nur Vermutung waren:

**`<CompanyDetails>` ist Pflicht.** Fehlt der Knoten, laedt das Plugin nicht -
der Inhalt ist dagegen voellig egal. Das ist der stillste Fehlschlag im ganzen
Format: alles andere kann stimmen, und es passiert trotzdem nichts.

**Ein `<Components>`-Block je Jahrgang ist nicht Vorsicht, sondern noetig.**
Die Doku sagt ausdruecklich, dass Max die Binaerkompatibilitaet nicht prueft.
Ein Block mit `SeriesMin="2016" SeriesMax="2027"` wuerde eine einzelne DLU in
alle zwoelf Jahrgaenge laden. Umgekehrt laedt ein zu enger Bereich gar nicht:
steht dort 2015 und laeuft 2016, wird das Plugin uebersprungen, obwohl es
binaerkompatibel waere. Skripte teilen sich dagegen zu Recht einen Block ueber
den ganzen Bereich.

**`ProductCode` ist bei 3ds Max optional, `UpgradeCode` Pflicht** - genau
andersherum als bei AutoCAD, und die Doku hebt den Unterschied eigens hervor.
Gebraucht wird `ProductCode` erst beim Hochladen in den App Store.

Zwei Dinge stehen ausserdem nicht in der Max-Spezifikation, schaden aber
nicht: `FriendlyVersion` (aus der AutoCAD-Welt, wird beim App-Store-Upload
gebraucht) und die Attribute `AppName`, `Version`, `AppDescription` am
`ComponentEntry` - dokumentiert ist dort nur `ModuleName`, aber Autodesks
eigene Beispiele benutzen die anderen auch.

Randnotiz zur Doku selbst: die Seite "Packaging Plug-ins" schreibt im Beispiel
`Description="plugin parts"`, die Formatreferenz dagegen `"plugins parts"`.
Die Formatreferenz ist die Liste der gueltigen Werte, also gilt der Plural.

`tools/PRUEFE_PAKET.py` prueft das alles nach, samt doppelt genannter Dateien
(Max zeigt dann eine Fehlermeldung), fehlender Dateien im gebauten Paket und
auseinandergelaufener Versionsnummern zwischen `bafimport.h`, `BafImport.mcr`
und dem Paket.

Zum Ausprobieren ohne Installation gibt es seit Max 2019
`ADSK_APPLICATION_PLUGINS` - eine Umgebungsvariable, die auf den Elternordner
des Pakets zeigt. Siehe README.

Zwei Details beim Uebernehmen:

**Die Submenu-GUID in `BafMenu_2025_2027.ms` ist dieselbe wie bei XFBIN
Import.** Anders landen die beiden Werkzeuge in zwei gleichnamigen
"DH Tools"-Menues nebeneinander. Die Action-GUID ist dagegen neu.

**Der Versionsvergleich im `.mcr` ist ein Zeichenkettenvergleich auf
`BafCpp.version()`.** Nicht `getPropNames`: Plugin-Funktionen sind Methoden
(`FN_0`/`FN_3`), keine Eigenschaften - die Liste kommt leer zurueck und
blockiert ein funktionierendes Plugin. Das war der Fehlschlag in XFBIN 1.7.2.
Der Fall, den der Vergleich abfaengt, ist der haeufigste ueberhaupt: Skripte
installiert, Plugin nicht neu gebaut.

Der Versionsbereich Max 2016-2027 und die beiden API-Bruecken aus 1.5.1
(`ExecuteMAXScriptScript`-Signatur ab 2022, `NonLocalizedClassName` ab 2022,
`#include <iparamb2.h>` fuer 2016) gelten hier unveraendert.
