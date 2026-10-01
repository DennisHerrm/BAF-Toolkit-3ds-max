# ============================================================
#  PRUEFE_API.py - prueft die drei Eintragungsstellen
#
#  Eine Plugin-Funktion muss an DREI Stellen stehen:
#     1. im enum BafToolkitFnID          (baftoolkit.h)
#     2. in der BEGIN_FUNCTION_MAP      (baftoolkit.h)
#     3. im Interface-Deskriptor        (baftoolkit.cpp)
#
#  Fehlt die dritte, ist die Funktion bei MaxScript gar nicht
#  angemeldet - und kein Neubau behebt das. Genau dieser Fehler
#  hat bei XFBIN Import eine Version gekostet (1.7.2), weil man
#  ihn im Quelltext nicht sieht: enum und FUNCTION_MAP sahen
#  vollstaendig aus.
#
#  Geprueft wird ausserdem, ob die Argumentzahl zwischen
#  FUNCTION_MAP (FN_2 = zwei Argumente) und Deskriptor
#  (..., 0, 2, ...) uebereinstimmt. Eine Abweichung dort ergibt
#  in Max keinen Uebersetzungsfehler, sondern falsch belegte
#  Argumente zur Laufzeit.
#
#  Aufruf:   python tools\PRUEFE_API.py
#  Rueckgabe 0 = alles stimmig, 1 = Fund.
# ============================================================
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
HEADER = os.path.join(ROOT, 'src', 'baftoolkit.h')
SOURCE = os.path.join(ROOT, 'src', 'baftoolkit.cpp')


def read(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as handle:
        return handle.read()


def strip_comments(text):
    """Kommentare raus, sonst zaehlt ein auskommentierter Eintrag mit.
    Stringliterale bleiben unangetastet - in ihnen stecken die
    MaxScript-Namen."""
    out = []
    i = 0
    while i < len(text):
        two = text[i:i + 2]
        if two == '//':
            i = text.find('\n', i)
            if i < 0:
                break
        elif two == '/*':
            end = text.find('*/', i + 2)
            i = len(text) if end < 0 else end + 2
        elif text[i] == '"':
            j = i + 1
            while j < len(text) and text[j] != '"':
                j += 2 if text[j] == '\\' else 1
            out.append(text[i:j + 1])
            i = j + 1
        else:
            out.append(text[i])
            i += 1
    return ''.join(out)


def enum_ids(header):
    match = re.search(r'enum\s+BafToolkitFnID\s*\{(.*?)\}', header, re.S)
    if not match:
        return None
    ids = []
    for line in match.group(1).split(','):
        name = line.strip().split('=')[0].strip()
        if re.fullmatch(r'fn_\w+', name):
            ids.append(name)
    return ids


def function_map(header):
    """{fn_id: Argumentzahl} aus der FUNCTION_MAP."""
    match = re.search(r'BEGIN_FUNCTION_MAP(.*?)END_FUNCTION_MAP', header, re.S)
    if not match:
        return None
    found = {}
    for arity, fn_id in re.findall(r'FN_(\d)\s*\(\s*(fn_\w+)', match.group(1)):
        found[fn_id] = int(arity)
    return found


def descriptor(source):
    """{fn_id: (MaxScript-Name, Argumentzahl)} aus dem Deskriptor.

    Der Aufbau je Eintrag ist:
        fn_id, _T("name"), 0, TYPE_XXX, 0, <argumentzahl>,
    danach folgen genau so viele Argumentbeschreibungen."""
    match = re.search(r'theBafToolkitInterface\s*\((.*?)\n\);', source, re.S)
    if not match:
        return None
    pattern = re.compile(
        r'(fn_\w+)\s*,\s*_T\(\s*"([^"]*)"\s*\)\s*,\s*\d+\s*,\s*TYPE_\w+\s*,\s*\d+\s*,\s*(\d+)')
    return {m.group(1): (m.group(2), int(m.group(3)))
            for m in pattern.finditer(match.group(1))}


def main():
    for path in (HEADER, SOURCE):
        if not os.path.isfile(path):
            print('Nicht gefunden: %s' % path)
            return 1

    header = strip_comments(read(HEADER))
    source = strip_comments(read(SOURCE))

    ids = enum_ids(header)
    mapped = function_map(header)
    declared = descriptor(source)

    if ids is None:
        print('enum BafToolkitFnID nicht gefunden')
        return 1
    if mapped is None:
        print('BEGIN_FUNCTION_MAP nicht gefunden')
        return 1
    if declared is None:
        print('Interface-Deskriptor nicht gefunden')
        return 1

    problems = []

    for fn_id in ids:
        if fn_id not in mapped:
            problems.append('%s: im enum, aber nicht in der FUNCTION_MAP' % fn_id)
        if fn_id not in declared:
            problems.append('%s: im enum, aber NICHT im Deskriptor - '
                            'bei MaxScript nicht angemeldet' % fn_id)

    for fn_id in mapped:
        if fn_id not in ids:
            problems.append('%s: in der FUNCTION_MAP, aber nicht im enum' % fn_id)
    for fn_id in declared:
        if fn_id not in ids:
            problems.append('%s: im Deskriptor, aber nicht im enum' % fn_id)

    for fn_id in sorted(set(mapped) & set(declared)):
        if mapped[fn_id] != declared[fn_id][1]:
            problems.append('%s: FN_%d in der FUNCTION_MAP, aber %d Argumente '
                            'im Deskriptor' % (fn_id, mapped[fn_id], declared[fn_id][1]))

    # Vierte Pruefung: die C++-Methode hinter jedem FN_x muss in der
    # Klasse deklariert UND in der .cpp definiert sein. Fehlt die
    # Definition, meldet erst der Linker etwas - und zwar gegen ein
    # Max SDK, also fruehestens beim vollen Build.
    map_block = re.search(r'BEGIN_FUNCTION_MAP(.*?)END_FUNCTION_MAP', header, re.S).group(1)
    methods = re.findall(r'FN_\d\s*\(\s*fn_\w+\s*,\s*TYPE_\w+\s*,\s*(\w+)', map_block)
    class_body = re.search(r'class BafToolkitInterface.*?\n\};', header, re.S)
    after_map = ''
    if class_body:
        text = class_body.group(0)
        after_map = text[text.find('END_FUNCTION_MAP'):]
    for method in methods:
        if not re.search(r'\b%s\s*\(' % re.escape(method), after_map):
            problems.append('%s: in der FUNCTION_MAP, aber nicht in der Klasse deklariert'
                            % method)
        if not re.search(r'BafToolkitInterface::%s\s*\(' % re.escape(method), source):
            problems.append('%s: deklariert, aber in baftoolkit.cpp nicht definiert' % method)

    names = {}
    for fn_id, (name, _arity) in declared.items():
        names.setdefault(name, []).append(fn_id)
    for name, owners in names.items():
        if len(owners) > 1:
            problems.append('MaxScript-Name "%s" zweimal vergeben: %s'
                            % (name, ', '.join(sorted(owners))))

    print('enum: %d, FUNCTION_MAP: %d, Deskriptor: %d'
          % (len(ids), len(mapped), len(declared)))
    if problems:
        print('')
        for problem in problems:
            print('  %s' % problem)
        print('\n%d Fund(e).' % len(problems))
        return 1
    print('Alle drei Stellen stimmen ueberein.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
