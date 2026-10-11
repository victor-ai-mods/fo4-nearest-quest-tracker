"""
Полная сборка мода: DLL (CMake из VS 18), NearestQuestTracker.esp, MCM, окно (SWF), скрипты Papyrus.

    python tools/build.py              — Debug-DLL (для разработки: лог уровня debug)
    python tools/build.py --release    — Release-DLL (в пакет)
    python tools/build.py --all        — обе DLL

Затем `python tools/deploy.py` (или `--dll release`).
"""

import argparse
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.environ.get('FO4_PATH', r'D:\Games\Fallout 4')
VS = r'C:\Program Files\Microsoft Visual Studio\18\Enterprise'
CMAKE = os.path.join(VS, r'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe')
VCPKG = os.path.join(VS, r'VC\vcpkg')
PAPYRUS = os.path.join(GAME, 'Papyrus Compiler', 'PapyrusCompiler.exe')
# Исходники Papyrus из f4se_0_06_23.7z (Data/Scripts/Source/*.psc): UI, ScriptObject.RegisterForExternalEvent…
# В игре их нет (там только .pex F4SE), в репозиторий не кладём — чужие файлы.
F4SE_SCRIPTS = os.environ.get('F4SE_SCRIPTS', os.path.join(os.path.dirname(ROOT), '_tools', 'f4se_0_06_23',
                                                           'Scripts', 'Source'))


def run(args, cwd):
    print('>', ' '.join(os.path.basename(a) if i == 0 else a for i, a in enumerate(args)))
    result = subprocess.run(args, cwd=cwd, env=dict(os.environ, VCPKG_ROOT=VCPKG))
    if result.returncode:
        sys.exit('ошибка: код %d' % result.returncode)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--release', action='store_true')
    ap.add_argument('--all', action='store_true')
    args = ap.parse_args()
    presets = ['debug', 'release'] if args.all else ['release' if args.release else 'debug']

    plugin = os.path.join(ROOT, 'f4se_plugin')
    run([CMAKE, '--preset', 'vs'], plugin)
    for preset in presets:
        run([CMAKE, '--build', '--preset', preset], plugin)
    run([sys.executable, os.path.join(ROOT, 'tools', 'gen_esp.py')], ROOT)
    run([sys.executable, os.path.join(ROOT, 'tools', 'gen_mcm.py')], ROOT)
    run([sys.executable, os.path.join(ROOT, 'tools', 'gen_menu.py')], ROOT)
    if not os.path.exists(os.path.join(F4SE_SCRIPTS, 'UI.psc')):
        sys.exit('нет %s — распакуйте Data/Scripts/Source из f4se_0_06_23.7z (или задайте F4SE_SCRIPTS)'
                 % os.path.join(F4SE_SCRIPTS, 'UI.psc'))
    # F4SE раньше Base: его версии ванильных классов (ScriptObject, Form…) — расширенные.
    # Source\User — только ради MCM.psc (MCM.RefreshMenu).
    run([PAPYRUS, 'papyrus', '-i=papyrus;' + F4SE_SCRIPTS + ';' + os.path.join(GAME, r'Data\Scripts\Source\Base')
         + ';' + os.path.join(GAME, r'Data\Scripts\Source\User'),
         '-o=' + os.path.join('build', 'scripts'), '-f=Institute_Papyrus_Flags.flg', '-all'], ROOT)
    print('готово: %s' % ', '.join(presets))


if __name__ == '__main__':
    main()
