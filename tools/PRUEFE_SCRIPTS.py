# ============================================================
#  PRUEFE_SCRIPTS.py - Linter fuer die MaxScript-Dateien
#  (uebernommen aus XFBIN Import, API-Liste an BAF angepasst)
#
#  Aufruf (aus dem Projektordner):
#    python tools\PRUEFE_SCRIPTS.py
#
#  Prueft in scripts\ und tools\ die vier Fehlerarten, die in
#  diesem Projekt tatsaechlich vorgekommen sind - jede davon
#  faellt sonst erst in 3ds Max auf:
#
#    1. Nicht-ASCII-Zeichen. Die Kodierung von .mcr-Dateien ist
#       ueber die Max-Versionen hinweg nicht verlaesslich.
#    2. C-Kommentare (//). MAXScript kommentiert mit --; ein //
#       ergibt "Syntax error: at /, expected <rollout clause>".
#    3. Unausgeglichene Klammern.
#    4. Vorwaertsreferenzen auf fn. MAXScript loest Namen in
#       einem Rollout von oben nach unten auf; eine weiter unten
#       definierte Funktion ist beim Aufruf undefined.
#    5. Ereignishandler fuer Controls, die es nicht gibt - meist
#       ein Tippfehler oder ein umbenanntes Control.
#    6. Aufrufe von BafCpp.*, die die Plugin-API nicht kennt.
#       Faellt sonst erst zur Laufzeit auf, und auch nur wenn man
#       den Knopf drueckt.
#    7. "continue" in Schleifen. Gibt es in MAXScript nicht -
#       gewohnt aus anderen Sprachen, und faellt erst auf, wenn
#       die Schleife tatsaechlich laeuft.
#
#  Rueckgabe: 0 = sauber, 1 = Probleme gefunden.
# ============================================================

import os
import re
import glob
import sys


# Die von baftoolkit.h veroeffentlichte MaxScript-API. Beim
# Erweitern des Plugins hier mitpflegen - dann meldet der Linter
# einen Tippfehler im Skript, statt ihn erst im Listener zu zeigen.
def plugin_api():
    """Die MaxScript-Namen, die das Plugin anmeldet - gelesen aus dem
    Interface-Deskriptor in baftoolkit.cpp.

    Bewusst NICHT als Liste hier im Skript gepflegt. Eine handgepflegte
    Liste kann eine Funktion enthalten, die es gar nicht gibt; der Linter
    winkt den Aufruf dann durch, und der Fehler faellt erst in Max auf.
    Genau so ist bei XFBIN Import eine Version verlorengegangen.

    Der Deskriptor ist die Stelle, die auch Max liest. Steht ein Name
    dort nicht, gibt es ihn in MaxScript nicht - egal was sonst wo steht.
    """
    path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                        'src', 'baftoolkit.cpp')
    if not os.path.isfile(path):
        return None
    text = open(path, encoding='utf-8', errors='replace').read()
    block = re.search(r'theBafToolkitInterface\s*\((.*?)\n\);', text, re.S)
    if block is None:
        return None
    return set(re.findall(r'fn_\w+\s*,\s*_T\(\s*"(\w+)"\s*\)', block.group(1)))


PLUGIN_API = plugin_api()


def check(path):
    raw = open(path, encoding='utf-8').read()
    problems = []

    nonascii = [(i + 1, c) for i, ln in enumerate(raw.split('\n'))
                for c in ln if ord(c) > 127]
    if nonascii:
        problems.append('%d Nicht-ASCII-Zeichen (erste Zeile %d)'
                        % (len(nonascii), nonascii[0][0]))

    cstyle = [i + 1 for i, ln in enumerate(raw.split('\n'))
              if re.match(r'^\s*//', ln)]
    if cstyle:
        problems.append('C-Kommentar // in Zeile(n) %s'
                        % ', '.join(str(x) for x in cstyle[:5]))

    src = re.sub(r'/\*.*?\*/', '', raw, flags=re.S)
    clean = '\n'.join(re.sub(r'"(\\.|[^"\\])*"', '""', re.sub(r'--.*$', '', l))
                      for l in src.split('\n'))
    par = clean.count('(') - clean.count(')')
    brk = clean.count('[') - clean.count(']')
    if par:
        problems.append('Klammerbilanz () = %+d' % par)
    if brk:
        problems.append('Klammerbilanz [] = %+d' % brk)

    lines = raw.split('\n')

    # --- Vorwaertsreferenz auf fn ---
    #
    # Eine Ausnahme gibt es: die VORABDEKLARATION. Steht der Name
    # weiter oben als "local Name", ist er zwar noch undefined,
    # gilt MAXScript aber als bekannt - es laeuft darueber hinweg
    # und traegt den Wert nach, sobald die Definition kommt. Das
    # ist der Weg, den die Doku selbst fuer Rollouts beschreibt,
    # die sich gegenseitig ansprechen, und ohne ihn laesst sich
    # eine zweispaltige Oberflaeche gar nicht bauen.
    declared = {}
    for i, ln in enumerate(lines):
        m = re.match(r'\s*local\s+(.+)$', re.sub(r'--.*$', '', ln))
        if not m:
            continue
        for part in m.group(1).split(','):
            name = part.strip().split('=')[0].strip()
            if re.fullmatch(r'\w+', name) and name not in declared:
                declared[name] = i

    defs = {}
    for i, ln in enumerate(lines):
        m = re.match(r'\s*fn (\w+)', ln)
        if m:
            defs[m.group(1)] = i
    for i, ln in enumerate(lines):
        t = re.sub(r'--.*$', '', ln)
        for name, line in defs.items():
            if (re.search(r'\b' + name + r'\s*\(', t)
                    and not re.match(r'\s*fn ' + name, ln)
                    and i < line):
                # Vorher angemeldet? Dann ist es Absicht.
                if name in declared and declared[name] < i:
                    continue
                problems.append('%s in Zeile %d aufgerufen, aber erst in '
                                'Zeile %d definiert und nirgends vorher als '
                                '"local %s" angemeldet' % (name, i + 1, line + 1, name))

    # --- continue: gibt es in MAXScript nicht ---
    for i, ln in enumerate(lines):
        t = re.sub(r'--.*$', '', ln)
        if re.search(r'\bcontinue\b', t):
            problems.append('Zeile %d: "continue" gibt es in MAXScript nicht'
                            % (i + 1))

    # --- Control vor seiner Deklaration benutzt ---
    #
    # MAXScript loest Namen in einem Rollout strikt von oben nach
    # unten auf. Das gilt nicht nur fuer Funktionen untereinander,
    # sondern GENAUSO fuer Controls: eine Funktion, die weiter oben
    # steht als das Control, das sie anspricht, sieht dort
    # "undefined" - und meldet beim Aufruf
    #     Unknown property: "text" in undefined
    #
    # Das ist in BafImport 1.1.0 an sieben Controls passiert. Der
    # Linter kannte bis dahin nur die Vorwaertsreferenz zwischen
    # Funktionen und hat die Datei sauber gemeldet.
    control_line = {}
    for i, ln in enumerate(lines):
        m = re.match(r'\s*(label|button|checkBox|checkbox|editText|edittext|'
                     r'dropDownList|spinner|listBox|listbox|comboBox|radioButtons|'
                     r'colorPicker|slider|mapButton|materialButton|pickButton|'
                     r'progressBar|imgTag|bitmap|groupBox|timer)\s+(\w+)', ln)
        if m and m.group(2) not in control_line:
            control_line[m.group(2)] = i
    for i, ln in enumerate(lines):
        t = re.sub(r'--.*$', '', ln)
        for ctrl, line in control_line.items():
            if i < line and re.search(r'\b' + ctrl + r'\s*\.', t):
                problems.append('%s in Zeile %d angesprochen, aber erst in Zeile %d '
                                'angelegt - MAXScript sieht dort undefined'
                                % (ctrl, i + 1, line + 1))

    # --- Handler auf existierende Controls ---
    ctrls = set()
    for ln in lines:
        m = re.match(r'\s*(label|button|checkBox|editText|dropDownList|spinner'
                     r'|listBox|comboBox|radioButtons|colorPicker|slider|'
                     r'mapButton|materialButton|pickButton|progressBar|'
                     r'imgTag|bitmap|groupBox|timer)\s+(\w+)', ln)
        if m:
            ctrls.add(m.group(2))
    rollouts = set(re.findall(r'^\s*rollout\s+(\w+)', raw, re.M))
    for i, ln in enumerate(lines):
        m = re.match(r'\s*on (\w+) (\w+) do', ln)
        if m and m.group(1) not in ctrls and m.group(1) not in rollouts:
            problems.append('Zeile %d: Handler fuer unbekanntes Control "%s"'
                            % (i + 1, m.group(1)))

    # --- Aufrufe gegen die Plugin-API ---
    if PLUGIN_API is None:
        problems.append('src\\baftoolkit.cpp nicht lesbar - Aufrufe ungeprueft')
    else:
        unknown = sorted(set(re.findall(r'BafCpp\.(\w+)', raw)) - PLUGIN_API)
        if unknown:
            problems.append('Unbekannte Plugin-Funktion(en): %s'
                            % ', '.join(unknown))

    return problems


def main():
    files = sorted(glob.glob('scripts/*.mcr') + glob.glob('scripts/*.ms')
                   + glob.glob('tools/*.ms'))
    if not files:
        print('Keine Skriptdateien gefunden - aus dem Projektordner starten.')
        return 2

    bad = 0
    for f in files:
        problems = check(f)
        if problems:
            bad += 1
            print('%s:' % f)
            for p in problems:
                print('   %s' % p)
        else:
            print('%-34s ok' % f)

    print()
    if bad:
        print('%d Datei(en) mit Problemen.' % bad)
        return 1
    print('Alle sauber.')
    return 0


sys.exit(main())
