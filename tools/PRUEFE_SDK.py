# ============================================================
#  PRUEFE_SDK.py - Eigenheiten des Max SDK, die der Compiler
#  erst beim vollen Build meldet - und dann als Lawine.
#
#  Alle drei Pruefungen hier stammen aus echten Fehlschlaegen beim
#  ersten Build von 1.0.0 gegen das 2016er SDK.
#
#  ------------------------------------------------------------
#  1. Knoten-Handle gegen Animatable-Handle
#  ------------------------------------------------------------
#
#  Die SDK-Doku sagt es ausdruecklich:
#    "Do not confuse Node Handles with Animatable Handles:
#     they are actually quite different."
#
#  Ein Knoten-Handle (INode::GetHandle) ist eine dauerhafte, mit
#  der Szene gespeicherte Nummer und gilt nur fuer Knoten.
#  Ein AnimHandle (Animatable::GetHandleByAnim) ist ein
#  Instanzzaehler, der nur innerhalb einer Max-Sitzung gilt, dafuer
#  aber fuer JEDES Szenenelement - auch Materialien und Controller.
#
#  Der eine Wert in der anderen Welt nachgeschlagen trifft
#  bestenfalls nichts. Und weil beide Funktionen einen Zahlentyp
#  liefern, sagt der Compiler dazu nichts.
#
#  Genau das stand bis 1.0.0 im Code: INode::GetHandle() gespeichert
#  und mit Animatable::GetAnimByHandle nachgeschlagen.
#
#  Geprueft wird deshalb paarweise:
#    GetINodeByHandle  gehoert zu  INode::GetHandle
#    GetAnimByHandle   gehoert zu  Animatable::GetHandleByAnim
#
#  ------------------------------------------------------------
#  2. const auf SDK-Typen, deren Zugriffe nicht const sind
#  ------------------------------------------------------------
#
#  Face::getVert, Face::getMatID und einige andere sind im SDK
#  NICHT als const deklariert. Ueber ein "const Face&" sind sie
#  deshalb nicht aufrufbar, und die Meldung lautet
#    C2662: this-Zeiger kann nicht von "const Face" in "Face &"
#    konvertiert werden
#  was nicht danach klingt, dass ein const zu viel dasteht.
#
#  ------------------------------------------------------------
#  3. ExecuteMAXScriptScript ohne Versionsweiche
#  ------------------------------------------------------------
#
#  Die Signatur hat sich mit Max 2022 geaendert: an zweiter Stelle
#  steht seither ein MAXScript::ScriptSource statt eines BOOL - ein
#  Namensraum, den es vorher gar nicht gab. Ohne #if auf
#  MAX_RELEASE baut entweder die alte oder die neue Haelfte nicht.
#
#  ------------------------------------------------------------
#  4. Control::SetValue ohne Animationsmodus
#  ------------------------------------------------------------
#
#  SetValue legt nur dann einen Keyframe an, wenn Max im
#  Animationsmodus ist (AnimateOn). Sonst setzt es den KONSTANTEN
#  Wert des Controllers - ohne Fehlermeldung, ohne Rueckgabewert,
#  der davon erzaehlt. Die Animation fehlt dann vollstaendig.
#
#  Genau das steckte in BAF Import 1.2.1.
#
#  ------------------------------------------------------------
#  5. MSTR-Rueckgabe an einen const MCHAR* zuweisen
#  ------------------------------------------------------------
#
#  Einige SDK-Funktionen liefern seit 3ds Max 2025 eine MSTR ALS
#  WERT, wo vorher ein const MCHAR* stand - GetDir zum Beispiel.
#  WStr hat seine Umwandlung nach const wchar_t* fuer Rvalues
#  absichtlich geloescht, weil der Zeiger sonst auf eine schon
#  freigegebene Zeichenkette zeigte. Die Meldung lautet
#    C2280: WStr::operator const wchar_t*(void) &&
#
#  Richtig ist, die MSTR benannt zu halten und ueber .data() zu
#  gehen - das uebersetzt auch mit den aelteren SDKs.
#
#  ------------------------------------------------------------
#  6. Dateilokale Funktion vor ihrer Definition benutzt
#  ------------------------------------------------------------
#
#  Eine Funktion im anonymen Namensraum muss in C++ VOR ihrem
#  ersten Aufruf definiert sein - anders als eine Methode, die
#  im Header deklariert ist. Steht sie weiter unten, meldet der
#  Compiler nur
#    C3861: Bezeichner wurde nicht gefunden
#  und dazu Folgefehler an ganz anderen Stellen.
#
#  Dieselbe Regel wie in MAXScript, wo PRUEFE_SCRIPTS.py sie
#  schon prueft. Sie ist hier zweimal aufgetreten, zuletzt beim
#  Verschieben von captureAnim.
#
#  ------------------------------------------------------------
#  7. DotProd auf Quat
#  ------------------------------------------------------------
#
#  DotProd ist fuer Point3 und Point4 ueberladen, nicht fuer Quat.
#  Das Skalarprodukt zweier Quaternionen muss von Hand gerechnet
#  werden - mit allen VIER Komponenten.
#
#  Aufruf:   python tools\\PRUEFE_SDK.py
#  Rueckgabe 0 = sauber, 1 = Fund.
# ============================================================
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(HERE), 'src')


def strip_comments(text):
    out, i = [], 0
    while i < len(text):
        two = text[i:i + 2]
        if two == '//':
            i = text.find('\n', i)
            if i < 0:
                break
        elif two == '/*':
            end = text.find('*/', i + 2)
            i = len(text) if end < 0 else end + 2
        else:
            out.append(text[i])
            i += 1
    return ''.join(out)


def main():
    problems = []
    for name in sorted(os.listdir(SRC)):
        if not name.endswith(('.h', '.cpp')):
            continue
        raw = open(os.path.join(SRC, name), encoding='utf-8', errors='replace').read()
        code = strip_comments(raw)
        # Auch die eigenen Header mitlesen: ein RAII-Guard steht
        # dort, nicht in der .cpp, und der Pruefer wuerde ihn sonst
        # uebersehen und eine korrekte Datei melden.
        own = code
        for header in re.findall(r'#\s*include\s*"([^"]+)"', raw):
            path = os.path.join(SRC, os.path.basename(header))
            if os.path.isfile(path):
                own += strip_comments(open(path, encoding='utf-8',
                                           errors='replace').read())
        # Haengt diese Datei ueberhaupt am SDK? Der Parser tut das
        # bewusst nicht, und seine Typen heissen teils genauso.
        uses_sdk = ('max.h' in raw) or ('baftoolkit.h' in raw)

        # Welche Nachschlagefunktionen kommen vor?
        node_lookup = 'GetINodeByHandle' in code
        anim_lookup = 'GetAnimByHandle' in code
        # Welche Quellen?
        node_source = re.search(r'->\s*GetHandle\s*\(\s*\)', code) is not None
        anim_source = 'GetHandleByAnim' in code

        # Ein Knoten-Handle darf nur mit GetINodeByHandle nachgeschlagen
        # werden. Kommen in EINER Datei ein ->GetHandle() und ein
        # GetAnimByHandle vor, ohne dass es ein GetHandleByAnim gibt,
        # ist die Vermischung sehr wahrscheinlich.
        if node_source and anim_lookup and not anim_source:
            problems.append('%s: benutzt INode::GetHandle() und '
                            'Animatable::GetAnimByHandle, aber nirgends '
                            'GetHandleByAnim - die beiden Handle-Welten sind '
                            'vermutlich vermischt' % name)
        if anim_source and not anim_lookup:
            problems.append('%s: holt ein AnimHandle, schlaegt aber nie mit '
                            'GetAnimByHandle nach' % name)
        if node_lookup and not node_source:
            problems.append('%s: schlaegt mit GetINodeByHandle nach, holt aber '
                            'nirgends ein Knoten-Handle' % name)

        # --- 2. const auf Typen mit nicht-const Zugriffen ---
        #
        # Nur in Dateien, die ueberhaupt am SDK haengen. Der Parser
        # hat eine EIGENE Struktur namens Face - "const baf::Face&"
        # ist dort voellig richtig, und ohne diese Einschraenkung
        # meldet der Pruefer neun Stellen, an denen nichts ist.
        # (Genau das ist beim Einbauen passiert.)
        if not uses_sdk:
            continue
        for line_number, line in enumerate(code.split('\n'), 1):
            for type_name in ('Face', 'TVFace'):
                # Nicht mit einem Namensraum davor: baf::Face ist
                # nicht Max' Face.
                if re.search(r'\bconst\s+%s\s*&' % type_name, line) \
                        and not re.search(r'::%s\s*&' % type_name, line):
                    problems.append('%s Zeile %d: "const %s &" - getVert und '
                                    'getMatID sind im SDK nicht const und darueber '
                                    'nicht aufrufbar'
                                    % (name, line_number, type_name))

        # --- 3. ExecuteMAXScriptScript ohne Versionsweiche ---
        if 'ExecuteMAXScriptScript' in code and 'MAX_RELEASE' not in code:
            problems.append('%s: ruft ExecuteMAXScriptScript ohne #if auf '
                            'MAX_RELEASE - die Signatur hat sich mit Max 2022 '
                            'geaendert' % name)

        # --- 4. SetValue ohne Animationsmodus ---
        if 'CTRL_ABSOLUTE' in code and 'AnimateOn' not in own:
            problems.append('%s: setzt Controllerwerte mit CTRL_ABSOLUTE, aber '
                            'nirgends steht AnimateOn - ohne Animationsmodus '
                            'entsteht kein einziger Keyframe' % name)

        # --- 5. MSTR-Rueckgabe an const MCHAR* ---
        mstr_returning = ('GetDir', 'GetMaxBuildNumber', 'GetTextFromFloat')
        for line_number, line in enumerate(code.split('\n'), 1):
            if not re.search(r'\bconst\s+MCHAR\s*\*', line):
                continue
            for call in mstr_returning:
                if re.search(r'\b%s\s*\(' % call, line):
                    problems.append('%s Zeile %d: %s liefert ab Max 2025 eine '
                                    'MSTR als Wert - erst in eine benannte MSTR, '
                                    'dann ueber .data()'
                                    % (name, line_number, call))

        # --- 6. Dateilokale Funktion vor ihrer Definition ---
        #
        # Nur der anonyme Namensraum zaehlt. Methoden stehen im
        # Header und duerfen in beliebiger Reihenfolge kommen.
        if name.endswith('.cpp'):
            # Den anonymen Namensraum ueber die KLAMMERN finden, nicht
            # ueber den Kommentar "} // namespace" dahinter:
            # strip_comments hat den laengst entfernt. Genau daran ist
            # diese Pruefung beim ersten Anlauf gescheitert - sie suchte
            # nach etwas, das der Pruefer selbst weggeworfen hatte.
            local_defs = {}
            for opener in re.finditer(r'\nnamespace\s*\{', code):
                depth = 0
                end = opener.end()
                for i in range(opener.end() - 1, len(code)):
                    if code[i] == '{':
                        depth += 1
                    elif code[i] == '}':
                        depth -= 1
                        if depth == 0:
                            end = i
                            break
                inner = code[opener.end():end]
                base = opener.end()
                for m in re.finditer(
                        r'^[A-Za-z_][\w:<>,*&\s]*?\b(\w+)\s*\([^;]*?\)\s*\{',
                        inner, re.M):
                    fn = m.group(1)
                    if fn in ('if', 'for', 'while', 'switch', 'return', 'catch'):
                        continue
                    at = base + m.start()
                    if fn not in local_defs or at < local_defs[fn]:
                        local_defs[fn] = at
            for fn, at in local_defs.items():
                for m in re.finditer(r'\b' + re.escape(fn) + r'\s*\(', code):
                    if m.start() >= at:
                        continue
                    # Die eigene Deklaration zaehlt nicht als Aufruf
                    line_start = code.rfind('\n', 0, m.start()) + 1
                    if re.match(r'\s*(static\s+)?[\w:<>,*&\s]+$',
                                code[line_start:m.start()]):
                        continue
                    line_number = code.count('\n', 0, m.start()) + 1
                    problems.append('%s Zeile %d: "%s" wird benutzt, ist aber erst '
                                    'weiter unten definiert - eine dateilokale '
                                    'Funktion muss vor ihrem Aufruf stehen'
                                    % (name, line_number, fn))
                    break

        # --- 7. DotProd auf Quat ---
        for line_number, line in enumerate(code.split('\n'), 1):
            if 'DotProd' in line and re.search(r'\b(quat|rotation|previous)\w*\b',
                                               line, re.I):
                problems.append('%s Zeile %d: DotProd ist fuer Quat nicht '
                                'ueberladen - von Hand rechnen, mit allen vier '
                                'Komponenten' % (name, line_number))

    if problems:
        for problem in problems:
            print('  %s' % problem)
        print('\n%d Fund(e).' % len(problems))
        return 1
    print('Keine bekannte SDK-Eigenheit gefunden.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
