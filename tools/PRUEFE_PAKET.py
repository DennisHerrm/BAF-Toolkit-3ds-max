# ============================================================
#  PRUEFE_PAKET.py - PackageContents.xml gegen die Spezifikation
#
#  Grundlage ist die "Autodesk Application Plug-in Package Format
#  Specification" fuer 3ds Max:
#    help.autodesk.com -> Max Developer Help -> Writing Plug-ins
#    -> Packaging Plug-ins -> packagexml_format
#
#  Geprueft wird, was dort steht - nicht, was in einem anderen
#  Projekt zufaellig funktioniert hat. Die Faelle, die sonst erst
#  in Max auffallen, und zwar meist gar nicht:
#
#    1. <CompanyDetails> fehlt. Laut Doku laedt das Plugin dann
#       NICHT. Der Inhalt ist egal, der Knoten muss da sein.
#       Das ist der stillste Fehlschlag von allen.
#    2. AppVersion fehlt oder hat nicht die Form MMM.mmm.bbb.
#    3. UpgradeCode fehlt. Bei 3ds Max ist er Pflicht (anders als
#       ProductCode, der hier optional ist - umgekehrt als bei
#       AutoCAD).
#    4. Ein <Components> ohne <RuntimeRequirements> oder ohne
#       SeriesMax. SeriesMax ist laut Doku zwingend.
#    5. Ein Description-Wert, den es nicht gibt. Ein Tippfehler
#       dort laedt die Dateien schlicht nicht, ohne Meldung.
#    6. Eine .dlu, deren Versionsbereich mehr als einen Jahrgang
#       umfasst. Max prueft KEINE Binaerkompatibilitaet - eine
#       fuer 2024 gebaute DLU in 2026 zu laden endet im Absturz.
#    7. Dieselbe ModuleName-Datei zweimal genannt. Laut Doku
#       zeigt Max dann eine Fehlermeldung.
#    8. Ein ModuleName, den es im Paket gar nicht gibt (nur wenn
#       package\...\Contents schon gebaut ist, also nach
#       INSTALLIERE.bat).
#
#  Aufruf:   python tools\PRUEFE_PAKET.py
#  Rueckgabe 0 = stimmig, 1 = Fund.
# ============================================================
import os
import re
import sys
import xml.etree.ElementTree as ET

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PKG_DIR = os.path.join(ROOT, 'package', 'BafToolkit')
PKG_XML = os.path.join(PKG_DIR, 'PackageContents.xml')

# Die gueltigen Description-Werte, wortwoertlich aus der Doku.
VALID_DESCRIPTIONS = {
    'plugins parts',
    'assemblies parts',
    'ui schemes parts',
    'default setting paths parts',
    'pre-start-up scripts parts',
    'macroscripts parts',
    'post-start-up scripts parts',
    'light icon paths parts',
    'dark icon paths parts',
    'scene converter folders parts',
    'osl folders parts',
    'amg folders parts',
    'hotkey parts',
}

# Bereich, den dieses Projekt abdeckt.
YEAR_MIN, YEAR_MAX = 2016, 2027


def main():
    if not os.path.isfile(PKG_XML):
        print('Nicht gefunden: %s' % PKG_XML)
        return 1

    try:
        root = ET.parse(PKG_XML).getroot()
    except ET.ParseError as error:
        print('PackageContents.xml ist kein gueltiges XML: %s' % error)
        return 1

    problems = []

    if root.tag != 'ApplicationPackage':
        problems.append('Wurzelknoten ist <%s>, erwartet <ApplicationPackage>' % root.tag)

    # ---- Pflichtattribute von <ApplicationPackage> ----
    if root.get('AutodeskProduct') != '3ds Max':
        problems.append('AutodeskProduct muss "3ds Max" sein, ist "%s"'
                        % root.get('AutodeskProduct'))
    if root.get('ProductType') != 'Application':
        problems.append('ProductType muss "Application" sein, ist "%s"'
                        % root.get('ProductType'))

    app_version = root.get('AppVersion')
    if not app_version:
        problems.append('AppVersion fehlt (Pflicht)')
    elif not re.fullmatch(r'\d+\.\d+\.\d+', app_version):
        problems.append('AppVersion "%s" hat nicht die Form MMM.mmm.bbb' % app_version)

    for name in ('UpgradeCode',):
        value = root.get(name)
        if not value:
            problems.append('%s fehlt - bei 3ds Max Pflicht' % name)
        elif not re.fullmatch(r'\{[0-9A-Fa-f-]{36}\}', value):
            problems.append('%s ist keine GUID in geschweiften Klammern: %s' % (name, value))
    product_code = root.get('ProductCode')
    if product_code and not re.fullmatch(r'\{[0-9A-Fa-f-]{36}\}', product_code):
        problems.append('ProductCode ist keine GUID in geschweiften Klammern: %s'
                        % product_code)
    if product_code and product_code == root.get('UpgradeCode'):
        problems.append('ProductCode und UpgradeCode sind gleich - '
                        'das sind zwei verschiedene Kennungen')

    # ---- <CompanyDetails>: fehlt er, laedt das Plugin nicht ----
    if root.find('CompanyDetails') is None:
        problems.append('<CompanyDetails> fehlt. Laut Doku laedt das Plugin dann NICHT, '
                        'auch wenn sonst alles stimmt')

    # ---- <Components> ----
    components = root.findall('Components')
    if not components:
        problems.append('Kein einziger <Components>-Knoten')

    seen_modules = {}
    covered_years = {}

    for index, node in enumerate(components):
        where = 'Components %d' % (index + 1)
        description = node.get('Description')
        if description is None:
            problems.append('%s: Description fehlt (Pflicht)' % where)
        elif description not in VALID_DESCRIPTIONS:
            problems.append('%s: Description "%s" gibt es nicht - die Dateien werden '
                            'dann stillschweigend nicht geladen' % (where, description))
        else:
            where = '%s [%s]' % (where, description)

        requirements = node.find('RuntimeRequirements')
        if requirements is None:
            problems.append('%s: <RuntimeRequirements> fehlt (in Components Pflicht)' % where)
            series_min = series_max = None
        else:
            if requirements.get('OS') != 'Win64':
                problems.append('%s: OS sollte "Win64" sein, ist "%s"'
                                % (where, requirements.get('OS')))
            if requirements.get('Platform') != '3ds Max':
                problems.append('%s: Platform muss "3ds Max" sein, ist "%s"'
                                % (where, requirements.get('Platform')))
            series_min = requirements.get('SeriesMin')
            series_max = requirements.get('SeriesMax')
            if series_max is None:
                problems.append('%s: SeriesMax fehlt - laut Doku zwingend' % where)

        entries = node.findall('ComponentEntry')
        if not entries:
            problems.append('%s: kein <ComponentEntry>, mindestens einer noetig' % where)

        for entry in entries:
            module = entry.get('ModuleName')
            if not module:
                problems.append('%s: ComponentEntry ohne ModuleName' % where)
                continue
            if module in seen_modules:
                problems.append('%s: "%s" ist schon in %s genannt - Max zeigt bei einer '
                                'doppelt genannten Datei eine Fehlermeldung'
                                % (where, module, seen_modules[module]))
            else:
                seen_modules[module] = where

            # Liegt die Datei da? Nur pruefbar, wenn Contents gebaut ist.
            if not module.endswith('/') and '*' not in module:
                target = os.path.normpath(os.path.join(PKG_DIR, module))
                if os.path.isdir(os.path.join(PKG_DIR, 'Contents')) \
                        and not os.path.isfile(target):
                    problems.append('%s: "%s" ist im gebauten Paket nicht vorhanden'
                                    % (where, module))

            # ---- DLU: genau ein Jahrgang je Block ----
            if module.lower().endswith('.dlu'):
                if series_min != series_max:
                    problems.append('%s: "%s" gilt fuer %s bis %s. Eine DLU ist gegen '
                                    'GENAU EIN SDK gebaut, und Max prueft keine '
                                    'Binaerkompatibilitaet - je Jahrgang ein eigener '
                                    'Components-Block'
                                    % (where, module, series_min, series_max))
                elif series_min is not None:
                    covered_years[series_min] = module
                    # Der Jahrgang im Pfad muss zum Versionsbereich passen
                    found = re.search(r'/(\d{4})/', module)
                    if found and found.group(1) != series_min:
                        problems.append('%s: Pfad nennt %s, RuntimeRequirements aber %s'
                                        % (where, found.group(1), series_min))

    # ---- Deckt das Paket den beabsichtigten Bereich ab? ----
    missing = [str(year) for year in range(YEAR_MIN, YEAR_MAX + 1)
               if str(year) not in covered_years]
    if missing:
        problems.append('Kein DLU-Block fuer: %s' % ', '.join(missing))

    # ---- Versionsnummern quer durch das Projekt ----
    header = os.path.join(ROOT, 'src', 'baftoolkit.h')
    if os.path.isfile(header) and app_version:
        text = open(header, encoding='utf-8', errors='replace').read()
        found = re.search(r'BAFIMPORT_VERSION_STR\s+_T\("([^"]+)"\)', text)
        if found and found.group(1) != app_version:
            problems.append('AppVersion ist %s, baftoolkit.h meldet aber %s'
                            % (app_version, found.group(1)))

    for name in ('BafImport.mcr', 'BafExport.mcr'):
        mcr = os.path.join(ROOT, 'scripts', name)
        if not (os.path.isfile(mcr) and app_version):
            continue
        text = open(mcr, encoding='utf-8', errors='replace').read()
        found = re.search(r'sExpectedVersion\s*=\s*"([^"]+)"', text)
        if found and found.group(1) != app_version:
            problems.append('AppVersion ist %s, %s erwartet aber %s'
                            % (app_version, name, found.group(1)))

    print('%d Components-Bloecke, %d Dateien, DLU fuer %d Jahrgaenge'
          % (len(components), len(seen_modules), len(covered_years)))
    if problems:
        print('')
        for problem in problems:
            print('  %s' % problem)
        print('\n%d Fund(e).' % len(problems))
        return 1
    print('PackageContents.xml entspricht der Spezifikation.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
