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
    GLOB  NQT_IncludeRadiant  — 1 = «Отметить ближайшее» выбирает квестодателей радиантных квестов, пока они предлагают
                                задание (MCM, по умолчанию 1)
    GLOB  NQT_NoSpoilers      — 1 = у маркера старта не писать название квеста (MCM, по умолчанию 0)
    GLOB  NQT_IncludeNoMarker — 1 = хоткей «Отметить ближайший» учитывает и квесты Pip-Boy без маркера, в последнюю
                                очередь в своём мире (MCM, по умолчанию 0)
    QUST  NQT_LocationMarkerQuest — маркер ближайшей непосещённой локации (этап 2.0-1b), устроен как
                                 NQT_StartMarkerQuest: цель 0 на optional-алиас 0 LocationRef (маркер карты)
    GLOB  NQT_AllLocations    — 1 = хоткей локаций берёт любые непосещённые, а не только замеченные (MCM, по умолчанию 0)
    FLST  NQT_ExcludedLocationList — исключённые маркеры карты (копия массива скрипта для DLL)
    GLOB  NQT_IncludeQuests   — 1 = «Отметить ближайшее» выбирает активные квесты Pip-Boy (MCM, по умолчанию 1)
    GLOB  NQT_IncludeLocations — 1 = «Отметить ближайшее» выбирает неоткрытые локации (MCM, по умолчанию 0)
    GLOB  NQT_IncludeInteriors — 1 = «Отметить ближайшее» выбирает непосещённые интерьеры (MCM, по умолчанию 0; маркер —
                                 тот же NQT_LocationMarkerQuest, на наружную дверь; исключённые двери — в NQT_ExcludedLocationList)
    GLOB  NQT_SkipOwnedLocked — 1 = интерьеры: пропускать запертые двери, взлом которых — преступление (красная подпись
                                 «Взломать», TESObjectREFR::IsCrimeToActivate) (MCM, по умолчанию 1)
    GLOB  NQT_ShowQuestInWindow — 1 = хоткей «Показать текущий квест» показывает квест в окне, а не уведомлениями
                                 (MCM, по умолчанию 0)
    MGEF  NQT_OpenWindowEffect — архетип Script, скрипт NQT_OpenWindowEffect: открыть окно мода (этап 3e)
    ALCH  NQT_WindowItem      — предмет «Nearest Quest Tracker» на вкладке «Помощь»: окно с геймпада через избранное
                                (на MCM-хоткей кнопку геймпада не назначить); возвращается в инвентарь после
                                использования. Шаблоны MGEF.DATA / ALCH — как в Explosives Cycler (работает в игре).

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
FID_LOCATION_QUEST = BASE | 0x809   # его же ищет DLL (main.cpp: LookupForm 0x809)
FID_ALL_LOCATIONS = BASE | 0x80A
FID_EXCLUDED_LOCATIONS = BASE | 0x80B
FID_INCLUDE_QUESTS = BASE | 0x80C
FID_INCLUDE_LOCATIONS = BASE | 0x80D
FID_INCLUDE_INTERIORS = BASE | 0x80E
FID_SKIP_OWNED_LOCKED = BASE | 0x80F
FID_SHOW_QUEST_IN_WINDOW = BASE | 0x810
FID_OPEN_WINDOW_EFFECT = BASE | 0x811
FID_WINDOW_ITEM = BASE | 0x812
FID_INCLUDE_RADIANT = BASE | 0x813
NEXT_ID = 0x814

# Ванильные FormID (Fallout4.esm), как у предмета Explosives Cycler.
KYWD_OBJECT_TYPE_STIMPAK = 0x000F4AEB   # вкладка «Помощь»
KYWD_HC_IGNORE_AS_FOOD = 0x00249F52     # иначе Выживание считает предмет едой
SCRIPT_EFFECT = 'NQT_OpenWindowEffect'
ITEM_NAME = 'Nearest Quest Tracker'
ITEM_DESC = ('Opens the Nearest Quest Tracker window. Put it into your favorites to open the window with a '
             'gamepad. Reusable - returns to your inventory after every use.')

# Квесты маркеров (старт, локация); номера совпадают с NQT_MainScript (StartObjective, StartAlias,
# LocationObjective, LocationRefAlias).
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
                      ('NQT_NoSpoilers', FID_NO_SPOILERS), ('NQT_IncludeNoMarker', FID_INCLUDE_NO_MARKER),
                      ('NQT_LocationQuest', FID_LOCATION_QUEST), ('NQT_AllLocations', FID_ALL_LOCATIONS),
                      ('NQT_ExcludedLocationList', FID_EXCLUDED_LOCATIONS),
                      ('NQT_IncludeQuests', FID_INCLUDE_QUESTS), ('NQT_IncludeLocations', FID_INCLUDE_LOCATIONS),
                      ('NQT_IncludeInteriors', FID_INCLUDE_INTERIORS), ('NQT_SkipOwnedLocked', FID_SKIP_OWNED_LOCKED),
                      ('NQT_ShowQuestInWindow', FID_SHOW_QUEST_IN_WINDOW), ('NQT_WindowItem', FID_WINDOW_ITEM),
                      ('NQT_IncludeRadiant', FID_INCLUDE_RADIANT)]:
        s.prop(name, PROP_OBJECT, fid)
    quest.add(b'VMAD', vmad([s]))
    quest.add(b'FULL', zstring('Nearest Quest Tracker'))
    quest.add(b'DNAM', struct.pack('<HBBII', 0x0111, 0, 0x5E, 0, 0))   # Start Game Enabled | Run Once (как FSOC)
    quest.add(b'NEXT', b'')
    quest.add(b'ANAM', struct.pack('<I', 0))

    excluded = Record(b'FLST', FID_EXCLUDED, 'NQT_ExcludedList')
    excluded_locations = Record(b'FLST', FID_EXCLUDED_LOCATIONS, 'NQT_ExcludedLocationList')

    globs = [glob(FID_UNTRACK_OTHERS, 'NQT_UntrackOthers', 1.0),
             glob(FID_NOTIFICATIONS, 'NQT_Notifications', 1.0),
             glob(FID_AUTO_TRACK, 'NQT_AutoTrack', 0.0),
             glob(FID_INCLUDE_STARTS, 'NQT_IncludeStarts', 0.0),
             glob(FID_NO_SPOILERS, 'NQT_NoSpoilers', 0.0),
             glob(FID_INCLUDE_NO_MARKER, 'NQT_IncludeNoMarker', 0.0),
             glob(FID_ALL_LOCATIONS, 'NQT_AllLocations', 0.0),
             glob(FID_INCLUDE_QUESTS, 'NQT_IncludeQuests', 1.0),
             glob(FID_INCLUDE_LOCATIONS, 'NQT_IncludeLocations', 0.0),
             glob(FID_INCLUDE_INTERIORS, 'NQT_IncludeInteriors', 0.0),
             glob(FID_SKIP_OWNED_LOCKED, 'NQT_SkipOwnedLocked', 1.0),
             glob(FID_SHOW_QUEST_IN_WINDOW, 'NQT_ShowQuestInWindow', 0.0),
             glob(FID_INCLUDE_RADIANT, 'NQT_IncludeRadiant', 1.0)]
    # Порядок верхних групп — как в Fallout4.esm: GLOB … QUST … FLST.
    quests = [quest,
              marker_quest(FID_START_QUEST, 'NQT_StartMarkerQuest', 'Nearest Quest Tracker: new quest', 'New quest', 'StartRef'),
              marker_quest(FID_LOCATION_QUEST, 'NQT_LocationMarkerQuest', 'Nearest Quest Tracker: location', 'Location',
                           'LocationRef')]
    # Порядок верхних групп — как в Fallout4.esm: GLOB … MGEF … ALCH … QUST … FLST.
    return [(b'GLOB', globs), (b'MGEF', [window_effect()]), (b'ALCH', [window_item()]), (b'QUST', quests),
            (b'FLST', [excluded, excluded_locations])]


def mgef_data():
    """MGEF.DATA 152 байта: Archetype Script (+0x40 = 1), остальное — как в Explosives Cycler / QuickAid."""
    data = bytearray(152)
    struct.pack_into('<I', data, 0x40, 1)
    struct.pack_into('<I', data, 0x50, 1)
    struct.pack_into('<f', data, 0x70, 1.0)
    struct.pack_into('<I', data, 0x8C, 1)
    return bytes(data)


def window_effect():
    r = Record(b'MGEF', FID_OPEN_WINDOW_EFFECT, 'NQT_OpenWindowEffect')
    s = Script(SCRIPT_EFFECT)
    s.prop('Main', PROP_OBJECT, FID_QUEST)
    s.prop('SelfItem', PROP_OBJECT, FID_WINDOW_ITEM)
    r.add(b'VMAD', vmad([s]))
    r.add(b'FULL', zstring(ITEM_NAME))
    r.add(b'DATA', mgef_data())
    r.add(b'SNDD', b'')
    r.add(b'DNAM', b'\x00')
    return r


def window_item():
    r = Record(b'ALCH', FID_WINDOW_ITEM, 'NQT_WindowItem')
    r.add(b'OBND', struct.pack('<6h', -4, -4, -1, 4, 4, 1))
    r.add(b'FULL', zstring(ITEM_NAME))
    r.add(b'KSIZ', struct.pack('<I', 2))
    r.add(b'KWDA', struct.pack('<II', KYWD_OBJECT_TYPE_STIMPAK, KYWD_HC_IGNORE_AS_FOOD))
    r.add(b'MODL', zstring('Props\\Holotape_Prop.nif'))
    r.add(b'MODT', struct.pack('<I', 4) + bytes(16))
    r.add(b'DESC', zstring(ITEM_DESC))
    r.add(b'DATA', struct.pack('<f', 0.0))          # вес
    # ENIT: цена, флаги (NoAutoCalc | Medicine -> вкладка «Помощь»), зависимость, шанс, звук (0 = без звука).
    r.add(b'ENIT', struct.pack('<iIIfI', 0, 0x00010001, 0, 0.0, 0))
    r.add(b'DNAM', struct.pack('<I', 0))
    r.add(b'EFID', struct.pack('<I', FID_OPEN_WINDOW_EFFECT))
    r.add(b'EFIT', struct.pack('<fII', 0.0, 0, 0))
    return r


def marker_quest(fid, edid, full, objective_text, alias_name):
    """Раскладка QUST как в Nuka-Cola Recipe Markers (проверена в игре), только алиас обычный, а не коллекция:
    EDID, FULL, DNAM, NEXT, <QOBJ, FNAM, NNAM, QSTA>, ANAM, <ALST, ALID, FNAM, VTCK, ALED>. Без скрипта.
    Цель 0 на алиас 0; текст цели DLL заменяет на «Новый квест: …» / «Локация: …»."""
    r = Record(b'QUST', fid, edid)
    r.add(b'FULL', zstring(full))
    # flags Start Game Enabled, priority 30, тип «Разное»: Pip-Boy показывает текст цели в разделе «Разное».
    r.add(b'DNAM', struct.pack('<HBBfI', 0x0001, 30, 0, 0.0, QUEST_TYPE_MISC))
    r.add(b'NEXT', b'')
    r.add(b'QOBJ', struct.pack('<H', START_OBJECTIVE))
    r.add(b'FNAM', struct.pack('<I', 0))
    r.add(b'NNAM', zstring(objective_text))
    r.add(b'QSTA', struct.pack('<iII', START_ALIAS, 0, 0))     # QSTA: alias, flags, keyword
    r.add(b'ANAM', struct.pack('<I', START_ALIAS + 1))
    r.add(b'ALST', struct.pack('<I', START_ALIAS))
    r.add(b'ALID', zstring(alias_name))
    r.add(b'FNAM', struct.pack('<I', ALIAS_FLAG_OPTIONAL))
    r.add(b'VTCK', struct.pack('<I', 0))
    r.add(b'ALED', b'')
    return r


def check(path):
    from esm import Plugin
    plugin = Plugin(path)
    assert plugin.masters == ['Fallout4.esm'], 'мастера: %r' % (plugin.masters,)
    counts = {sig.decode(): sum(1 for _ in plugin.records(sig)) for sig in (b'GLOB', b'MGEF', b'ALCH', b'QUST', b'FLST')}
    assert counts == {'GLOB': 13, 'MGEF': 1, 'ALCH': 1, 'QUST': 3, 'FLST': 2}, counts
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
