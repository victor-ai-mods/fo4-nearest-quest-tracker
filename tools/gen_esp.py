"""
Генератор `NearestQuestTracker.esp` (ESL, единственный мастер — Fallout4.esm).

    GLOB  NQT_UntrackOthers   — 1 = при отметке ближайшего квеста снимать отметку с остальных (MCM, по умолчанию 1)
    GLOB  NQT_Notifications   — 1 = показывать уведомление «Отслеживается: …» (MCM, по умолчанию 1)
    GLOB  NQT_AutoTrack       — 1 = отмечать ближайший квест сам, когда меняются цели в Pip-Boy (MCM, по умолчанию 0)
    QUST  NQT_MainQuest       — Start Game Enabled, скрипт NQT_MainScript (хоткей MCM зовёт его TrackNearest)
    FLST  NQT_ExcludedList    — исключённые квесты (пустой; скрипт держит в нём копию своего массива Excluded для DLL)
    QUST  NQT_StartMarkerQuest — маркер точки старта не начатого квеста (этап 3b): тип «Разное», цель 0 на
                                 optional-алиас 0 StartRef; скрипт NQT_MainScript делает ForceRefTo +
                                 SetObjectiveDisplayed, текст цели ставит DLL (NQT_Native.SetObjectiveText)
    GLOB  NQT_IncludeStarts   — 1 = хоткей «Отметить ближайший» учитывает и не начатые квесты (MCM, по умолчанию 0)
    GLOB  NQT_NoSpoilers      — 1 = у маркера старта не писать название квеста (MCM, по умолчанию 0)
    GLOB  NQT_IncludeNoMarker — 1 = хоткей «Отметить ближайший» учитывает и квесты Pip-Boy без маркера, в последнюю
                                очередь в своём мире (MCM, по умолчанию 0)

Поиск ближайшего квеста и его названия — нативные функции NQT_Native (NearestQuestTracker.dll).

    python tools/gen_esp.py
"""

import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from esp_writer import PROP_OBJECT, TES4_LIGHT, Record, Script, build_plugin, vmad, zstring  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PLUGIN_NAME = 'NearestQuestTracker.esp'
SCRIPT_MAIN = 'NQT_MainScript'

# --- свои FormID (ESL: 0x800..0xFFF); номера зашиты в MCM (tools/gen_mcm.py) -----
BASE = 0x01000000
FID_QUEST = BASE | 0x800
FID_UNTRACK_OTHERS = BASE | 0x801
FID_NOTIFICATIONS = BASE | 0x802
FID_EXCLUDED = BASE | 0x803
FID_AUTO_TRACK = BASE | 0x804
FID_START_QUEST = BASE | 0x805      # его же ищет DLL (main.cpp: LookupForm 0x805)
FID_INCLUDE_STARTS = BASE | 0x806
FID_NO_SPOILERS = BASE | 0x807
FID_INCLUDE_NO_MARKER = BASE | 0x808
NEXT_ID = 0x809

# Квест маркера старта; номера совпадают с NQT_MainScript (StartObjective, StartAlias).
START_OBJECTIVE = 0
START_ALIAS = 0
QUEST_TYPE_MISC = 6
ALIAS_FLAG_OPTIONAL = 0x00000002


def glob(fid, edid, value):
    r = Record(b'GLOB', fid, edid)
    r.add(b'FLTV', struct.pack('<f', value))
    return r


def build():
    quest = Record(b'QUST', FID_QUEST, 'NQT_MainQuest')
    s = Script(SCRIPT_MAIN)
    for name, fid in [('NQT_UntrackOthers', FID_UNTRACK_OTHERS), ('NQT_Notifications', FID_NOTIFICATIONS),
                      ('NQT_ExcludedList', FID_EXCLUDED), ('NQT_AutoTrack', FID_AUTO_TRACK),
                      ('NQT_StartQuest', FID_START_QUEST), ('NQT_IncludeStarts', FID_INCLUDE_STARTS),
                      ('NQT_NoSpoilers', FID_NO_SPOILERS), ('NQT_IncludeNoMarker', FID_INCLUDE_NO_MARKER)]:
        s.prop(name, PROP_OBJECT, fid)
    quest.add(b'VMAD', vmad([s]))
    quest.add(b'FULL', zstring('Nearest Quest Tracker'))
    quest.add(b'DNAM', struct.pack('<HBBII', 0x0111, 0, 0x5E, 0, 0))   # Start Game Enabled | Run Once (как FSOC)
    quest.add(b'NEXT', b'')
    quest.add(b'ANAM', struct.pack('<I', 0))

    excluded = Record(b'FLST', FID_EXCLUDED, 'NQT_ExcludedList')

    globs = [glob(FID_UNTRACK_OTHERS, 'NQT_UntrackOthers', 1.0),
             glob(FID_NOTIFICATIONS, 'NQT_Notifications', 1.0),
             glob(FID_AUTO_TRACK, 'NQT_AutoTrack', 0.0),
             glob(FID_INCLUDE_STARTS, 'NQT_IncludeStarts', 0.0),
             glob(FID_NO_SPOILERS, 'NQT_NoSpoilers', 0.0),
             glob(FID_INCLUDE_NO_MARKER, 'NQT_IncludeNoMarker', 0.0)]
    # Порядок верхних групп — как в Fallout4.esm: GLOB … QUST … FLST.
    return [(b'GLOB', globs), (b'QUST', [quest, start_quest()]), (b'FLST', [excluded])]


def start_quest():
    """Раскладка QUST как в Nuka-Cola Recipe Markers (проверена в игре), только алиас обычный, а не коллекция:
    EDID, FULL, DNAM, NEXT, <QOBJ, FNAM, NNAM, QSTA>, ANAM, <ALST, ALID, FNAM, VTCK, ALED>. Без скрипта."""
    r = Record(b'QUST', FID_START_QUEST, 'NQT_StartMarkerQuest')
    r.add(b'FULL', zstring('Nearest Quest Tracker: new quest'))
    # flags Start Game Enabled, priority 30, тип «Разное»: Pip-Boy показывает текст цели в разделе «Разное».
    r.add(b'DNAM', struct.pack('<HBBfI', 0x0001, 30, 0, 0.0, QUEST_TYPE_MISC))
    r.add(b'NEXT', b'')
    r.add(b'QOBJ', struct.pack('<H', START_OBJECTIVE))
    r.add(b'FNAM', struct.pack('<I', 0))
    r.add(b'NNAM', zstring('New quest'))                       # DLL заменяет текст на «Новый квест: …»
    r.add(b'QSTA', struct.pack('<iII', START_ALIAS, 0, 0))     # QSTA: alias, flags, keyword
    r.add(b'ANAM', struct.pack('<I', START_ALIAS + 1))
    r.add(b'ALST', struct.pack('<I', START_ALIAS))
    r.add(b'ALID', zstring('StartRef'))
    r.add(b'FNAM', struct.pack('<I', ALIAS_FLAG_OPTIONAL))
    r.add(b'VTCK', struct.pack('<I', 0))
    r.add(b'ALED', b'')
    return r


def check(path):
    from esm import Plugin
    plugin = Plugin(path)
    assert plugin.masters == ['Fallout4.esm'], 'мастера: %r' % (plugin.masters,)
    counts = {sig.decode(): sum(1 for _ in plugin.records(sig)) for sig in (b'GLOB', b'QUST', b'FLST')}
    assert counts == {'GLOB': 6, 'QUST': 2, 'FLST': 1}, counts
    print('  проверка: мастер один, записи %s' % counts)


def main():
    out_path = os.path.join(ROOT, 'build', PLUGIN_NAME)
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    blob = build_plugin(['Fallout4.esm'], build(), NEXT_ID, flags=TES4_LIGHT)
    with open(out_path, 'wb') as f:
        f.write(blob)
    print('%s: %d байт' % (out_path, len(blob)))
    check(out_path)


if __name__ == '__main__':
    main()
