"""
Раскладка мода по игре.

  python tools/deploy.py                — NearestQuestTracker.esp, NQT_*.pex, Debug-DLL, MCM, переводы; включить
  python tools/deploy.py --dll release  — то же, но Release-DLL
  python tools/deploy.py --remove       — снять плагин и убрать его файлы
  python tools/deploy.py --ae [--dll …] — на стенд AE: MO2-мод «Nearest Quest Tracker (AE dev)» в
                                          D:\\Games\\MO2 Fallout 4 AE, плагин — в plugins.txt профиля
                                          «Nearest Quest Tracker» (профиль и строку в modlist.txt — один раз руками)

DLL заменяется только при закрытой игре (иначе файл занят).

`Plugins.txt` в этой установке живёт в ДВУХ местах (игра читает
`%LOCALAPPDATA%\\Fallout4\\Plugins.txt`) — правятся оба. Всё, что перезаписывается,
сначала уезжает в `<игра>\\Backup`.
"""

import argparse
import datetime
import os
import shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
GAME = os.environ.get('FO4_PATH', r'D:\Games\Fallout 4')
DATA = os.path.join(GAME, 'Data')
PLUGIN = 'NearestQuestTracker.esp'
SCRIPTS = ['NQT_MainScript.pex', 'NQT_Native.pex', 'NQT_OpenWindowEffect.pex']
DLL = 'NearestQuestTracker.dll'
MOD_FILES = os.path.join(ROOT, 'mod')           # пути внутри — как от Data

PLUGIN_LISTS = [
    os.path.join(os.environ['LOCALAPPDATA'], 'Fallout4', 'Plugins.txt'),
    os.path.join(GAME, 'fallout4', 'Plugins.txt'),
]
BUILD = 'vs'                                    # f4se_plugin/build/<BUILD>/<config>

# Стенд AE 1.11.191: свой портативный MO2, мод = папка как Data, профиль со своими ini и сохранениями.
AE_MO2 = r'D:\Games\MO2 Fallout 4 AE'
AE_MOD_NAME = 'Nearest Quest Tracker (AE dev)'
AE_PROFILE = os.path.join(AE_MO2, 'profiles', 'Nearest Quest Tracker')


def use_ae():
    global GAME, DATA, PLUGIN_LISTS, BUILD
    GAME = AE_MO2
    DATA = os.path.join(AE_MO2, 'mods', AE_MOD_NAME)
    PLUGIN_LISTS = [os.path.join(AE_PROFILE, 'plugins.txt')]
    BUILD = 'ae'


def backup(path):
    if not os.path.exists(path):
        return None
    stamp = datetime.datetime.now().strftime('%Y%m%d-%H%M%S')
    target_dir = os.path.join(GAME, 'Backup')
    os.makedirs(target_dir, exist_ok=True)
    target = os.path.join(target_dir, '%s.%s.bak' % (os.path.basename(path), stamp))
    shutil.copy2(path, target)
    return target


def copy(src, dst):
    saved = backup(dst)
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    shutil.copy2(src, dst)
    print('  %s%s' % (dst, ('  (старый -> %s)' % os.path.basename(saved)) if saved else ''))


def set_enabled(enabled):
    for path in PLUGIN_LISTS:
        if not os.path.exists(path):
            print('  нет %s — пропущено' % path)
            continue
        with open(path, encoding='utf-8-sig') as f:
            lines = f.read().splitlines()
        kept = [ln for ln in lines if ln.lstrip('*').strip().lower() != PLUGIN.lower()]
        if enabled:
            kept.append('*' + PLUGIN)
        if kept != lines:
            backup(path)
            with open(path, 'w', encoding='utf-8', newline='\n') as f:
                f.write('\n'.join(kept) + '\n')
        print('  %s: %s' % (path, 'включён' if enabled else 'выключен'))


def mod_files():
    out = []
    for base, _, names in os.walk(MOD_FILES):
        for name in names:
            out.append(os.path.relpath(os.path.join(base, name), MOD_FILES))
    return sorted(out)


def script_paths():
    return [(os.path.join(ROOT, 'build', 'scripts', n), os.path.join(DATA, 'Scripts', n)) for n in SCRIPTS]


def dll_paths(config):
    """(собранная DLL, куда в игре); config — 'Debug' или 'Release'."""
    return (os.path.join(ROOT, 'f4se_plugin', 'build', BUILD, config, DLL),
            os.path.join(DATA, 'F4SE', 'Plugins', DLL))


def install(config):
    print('Файлы:')
    copy(os.path.join(ROOT, 'build', PLUGIN), os.path.join(DATA, PLUGIN))
    copy(*dll_paths(config))
    for src, dst in script_paths():
        copy(src, dst)
    for rel in mod_files():
        copy(os.path.join(MOD_FILES, rel), os.path.join(DATA, rel))
    print('Порядок загрузки:')
    set_enabled(True)


def remove():
    print('Порядок загрузки:')
    set_enabled(False)
    print('Файлы:')
    paths = [os.path.join(DATA, PLUGIN), dll_paths('Release')[1]] + [dst for _, dst in script_paths()]
    paths += [os.path.join(DATA, rel) for rel in mod_files()]
    for path in paths:
        if os.path.exists(path):
            backup(path)
            os.remove(path)
            print('  удалён %s' % path)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--remove', action='store_true')
    ap.add_argument('--dll', choices=['debug', 'release'], default='debug')
    ap.add_argument('--ae', action='store_true')
    args = ap.parse_args()
    if args.ae:
        use_ae()
    if args.remove:
        remove()
    else:
        install(args.dll.capitalize())
        print('\nesp, .pex и DLL подхватываются только при запуске игры.')


if __name__ == '__main__':
    main()
