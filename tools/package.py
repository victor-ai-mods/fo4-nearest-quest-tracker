"""
Архив для Nexus: release/NearestQuestTracker/ (раскладка как в Data) и release/NearestQuestTracker-<версия>.zip.

    python tools/package.py 1.0.0

Внутри: NearestQuestTracker.esp, Release-DLL в F4SE\\Plugins, Scripts\\NQT_*.pex, исходники
Scripts\\Source\\User\\NQT_*.psc, MCM\\Config\\NearestQuestTracker\\*.json,
Interface\\Translations\\NearestQuestTracker_{en,ru}.txt,
F4SE\\Plugins\\NearestQuestTracker\\QuestStarts.json, QuestCategories.json + их *-user.example.json.
"""

import os
import shutil
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, 'build')
OUT = os.path.join(ROOT, 'release')
NAME = 'NearestQuestTracker'
SCRIPTS = ['NQT_MainScript', 'NQT_Native', 'NQT_OpenWindowEffect']


def files():
    """(источник, путь в архиве)."""
    out = [(os.path.join(BUILD, NAME + '.esp'), NAME + '.esp'),
           (os.path.join(ROOT, 'f4se_plugin', 'build', 'vs', 'Release', NAME + '.dll'),
            os.path.join('F4SE', 'Plugins', NAME + '.dll'))]
    for script in SCRIPTS:
        out.append((os.path.join(BUILD, 'scripts', script + '.pex'), os.path.join('Scripts', script + '.pex')))
        out.append((os.path.join(ROOT, 'papyrus', script + '.psc'),
                    os.path.join('Scripts', 'Source', 'User', script + '.psc')))
    mod = os.path.join(ROOT, 'mod')
    for base, _, names in os.walk(mod):
        for n in names:
            src = os.path.join(base, n)
            out.append((src, os.path.relpath(src, mod)))
    return sorted(out, key=lambda t: t[1])


def main():
    if len(sys.argv) != 2:
        sys.exit('версия? python tools/package.py 1.0.0')
    version = sys.argv[1]
    folder = os.path.join(OUT, NAME)
    os.makedirs(OUT, exist_ok=True)
    if os.path.exists(folder):
        shutil.rmtree(folder)
    archive = os.path.join(OUT, '%s-%s.zip' % (NAME, version))
    with zipfile.ZipFile(archive, 'w', zipfile.ZIP_DEFLATED) as z:
        for src, rel in files():
            if not os.path.exists(src):
                sys.exit('нет %s — сначала собери мод (README.ru.md, «Сборка»)' % src)
            dst = os.path.join(folder, rel)
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copy2(src, dst)
            z.write(src, rel.replace(os.sep, '/'))
            print('  %s' % rel)
    print('%s: %d байт' % (archive, os.path.getsize(archive)))


if __name__ == '__main__':
    main()
