# ============================================================
#  PRUEFE_INCLUDES.py - benutztes SDK-Symbol ohne seinen Header
#
#  Die neueren Max-SDKs ziehen fast alles ueber max.h mit herein,
#  die aelteren nicht. Wer nur gegen 2027 baut, merkt davon
#  nichts - und der 2016er Build faellt dann mit einer Lawine
#  Folgefehler um, deren erster irgendwo in der Mitte steht.
#
#  Bei XFBIN Import ist das dreimal passiert (<map>, <string>,
#  notify.h). Deshalb hier von vornherein.
#
#  Geprueft wird: kommt ein Symbol im Quelltext vor, muss der
#  Header, der es deklariert, auch eingebunden sein.
#
#  Aufruf:   python tools\PRUEFE_INCLUDES.py
#  Rueckgabe 0 = sauber, 1 = Fund.
# ============================================================
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(os.path.dirname(HERE), 'src')

# Symbol -> Header, der es deklariert.
SDK_SYMBOLS = {
    'TriObject':          'triobj.h',
    'CreateNewTriObject': 'triobj.h',
    'triObjectClassID':   'object.h',
    'ObjectState':        'object.h',
    'ClassDesc2':         'iparamb2.h',
    'IParamBlock2':       'iparamb2.h',
    'RegisterNotification': 'notify.h',
    'MeshNormalSpec':     'MeshNormalSpec.h',
    'ISkin':              'iskin.h',
    'ISkinImportData':    'iskin.h',
    'StdMat2':            'stdmat.h',
    'NewDefaultStdMat':   'stdmat.h',
    'IKeyControl':        'istdplug.h',
    'GetKeyControlInterface': 'istdplug.h',
    'MultiMtl':           'istdplug.h',
    'NewDefaultMultiMtl': 'istdplug.h',
    'NewDefaultBitmapTex':'stdmat.h',
    'FILTER_NADA':        'stdmat.h',
    'ALPHA_FILE':         'stdmat.h',
    'SetAlphaAsMono':     'stdmat.h',
    'ID_DI':              'imtl.h',
    'ID_OP':              'imtl.h',
    'MTL_TEX_DISPLAY_ENABLED': 'imtl.h',
    'SetActiveTexmap':    'imtl.h',
    'APP_IMAGE_DIR':      'maxapi.h',
    'APP_TEMP_DIR':       'maxapi.h',
    'Mtl':                'imtl.h',
    'BitmapTex':          'stdmat.h',
    'GetCOREInterface':   'maxapi.h',
    'GetINodeByHandle':   'maxapi.h',
    'GetHandleByAnim':    'maxapi.h',
    'IDerivedObject':     'modstack.h',
    'Control':            'control.h',
    'SceneImport':        'impexp.h',
    'SceneExport':        'impexp.h',
    'ImpInterface':       'impexp.h',
    'ExpInterface':       'impexp.h',
    'ExecuteMAXScriptScript': 'maxscript/maxscript.h',
}

# Header, die praktisch jedes SDK ueber max.h mitzieht.
VIA_MAX_H = {'maxapi.h', 'control.h'}

STD_SYMBOLS = {
    'std::vector':        'vector',
    'std::string':        'string',
    'std::wstring':       'string',
    'std::map':           'map',
    'std::set':           'set',
    'std::unordered_set': 'unordered_set',
    'std::unique_ptr':    'memory',
    'std::array':         'array',
    'std::ofstream':      'fstream',
    'std::ostringstream': 'sstream',
    'std::wostringstream':'sstream',
    'std::memcpy':        'cstring',
    'std::sort':          'algorithm',
    'std::to_chars':      'charconv',
}


def strip_comments_and_strings(text):
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
        elif text[i] == '"':
            j = i + 1
            while j < len(text) and text[j] != '"':
                j += 2 if text[j] == '\\' else 1
            out.append(' ')
            i = j + 1
        else:
            out.append(text[i])
            i += 1
    return ''.join(out)


def includes_of(text, own_headers):
    """Alle Header dieser Datei, samt denen der eigenen Header - ein
    #include in baftoolkit.h zaehlt auch fuer baftoolkit.cpp."""
    found = set(re.findall(r'#\s*include\s*[<"]([^>"]+)[>"]', text))
    for name in list(found):
        if name in own_headers:
            found |= includes_of(own_headers[name], own_headers)
    return found


def main():
    if not os.path.isdir(SRC):
        print('Nicht gefunden: %s' % SRC)
        return 1

    own_headers = {}
    for name in os.listdir(SRC):
        if name.endswith('.h'):
            own_headers[name] = open(os.path.join(SRC, name),
                                     encoding='utf-8', errors='replace').read()

    problems = []
    for name in sorted(os.listdir(SRC)):
        if not name.endswith(('.h', '.cpp')):
            continue
        path = os.path.join(SRC, name)
        raw = open(path, encoding='utf-8', errors='replace').read()
        code = strip_comments_and_strings(raw)
        have = includes_of(raw, own_headers)
        has_max_h = 'max.h' in have

        for symbol, header in list(SDK_SYMBOLS.items()) + list(STD_SYMBOLS.items()):
            pattern = r'\b' + re.escape(symbol) + (r'\b' if symbol[-1].isalnum() else '')
            if not re.search(pattern, code):
                continue
            if header in have:
                continue
            if header in VIA_MAX_H and has_max_h:
                continue
            problems.append('%s: benutzt %s, aber <%s> fehlt' % (name, symbol, header))

    if problems:
        for problem in problems:
            print('  %s' % problem)
        print('\n%d Fund(e).' % len(problems))
        return 1
    print('Jedes benutzte Symbol hat seinen Header.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
