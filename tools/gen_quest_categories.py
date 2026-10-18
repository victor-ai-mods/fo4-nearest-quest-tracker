"""
Этап 2.0-2: категории квестов с вики -> mod/F4SE/Plugins/NearestQuestTracker/QuestCategories.json.

Дерево категорий — разделы русской страницы «Квесты Fallout 4» (fallout.fandom.com/ru); квесты — ID
из её таблиц ({{ID|…}} основной игры, {{DLC ID|…}} / {{DLC_ID|…}} дополнений: плагин — по разделу).
Каждый ID сверяется с QUST в мастерах (индекс tools/esm_index.py); не-квесты и ID без записи — в отчёт.
Квесты, которых нет в русских таблицах, но есть в английских (en-страница «Fallout 4 quests»),
добавляются по соответствию разделов EN_SECTIONS. Остальное — вручную в MANUAL.
Английские названия категорий — из таблицы TITLES (en-страница устроена по-другому).

Creation Club (AE 1.11.x): на ru-вики раздела нет, на en-вики — только список названий, поэтому квесты берутся из самих
cc-плагинов AE: все QUST с целями (кроме CC_SKIP) -> «Дополнения / Creation Club». Нужен индекс AE
(python tools/esm_index.py --ae); без него раздел не строится. Названия в JSON — из строк cc-плагинов (ba2_strings.py).

Wikitext страниц кэшируется в tools/cache/ (чужой текст, в репозиторий не кладётся); --refresh — скачать заново.

python tools/gen_quest_categories.py [--refresh] [--index <pickle>] [--ae-index <pickle>]
"""

import argparse
import json
import os
import pickle
import re
import subprocess
import urllib.parse

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
CACHE = os.path.join(HERE, 'cache')
OUT_DIR = os.path.join(ROOT, 'mod', 'F4SE', 'Plugins', 'NearestQuestTracker')
DEFAULT_INDEX = os.path.join(ROOT, '..', '_research', 'nqt-quests', 'esm_index.pickle')
DEFAULT_AE_INDEX = os.path.join(ROOT, '..', '_research', 'nqt-quests', 'esm_index_ae.pickle')
AE_DATA = r'D:\Games\F4_AE\Data'

# Creation Club: категория и квесты с целями, которые не квесты игрока.
CC_CATEGORY = {'key': 'dlc.cc', 'ru': 'Creation Club', 'en': 'Creation Club'}
CC_SKIP = {
    'ccOTMFO4001-Remnants.esl|0000E8',  # ccOTMFO4001_VB «Vertibird Scene Controller»: цель «VB Location» — служебная
}

PAGES = {
    'ru': ('https://fallout.fandom.com/ru/api.php', 'Квесты Fallout 4'),
    'en': ('https://fallout.fandom.com/api.php', 'Fallout 4 quests'),
}

# Заголовок раздела ru-вики -> (ключ, ru-название, en-название). Ключ категории — ключи разделов через точку.
# Неизвестный заголовок — ошибка: таблицу надо дополнить (вики поменялась).
TITLES = {
    'Основные квесты': ('main', 'Основные квесты', 'Main quests'),
    'Пролог': ('prologue', 'Пролог', 'Prologue'),
    'Первый акт': ('act1', 'Первый акт', 'Act One'),
    'Второй акт': ('act2', 'Второй акт', 'Act Two'),
    'Третий акт': ('act3', 'Третий акт', 'Act Three'),
    'Фракционные квесты': ('faction', 'Фракционные квесты', 'Faction quests'),
    'Минитмены': ('minutemen', 'Минитмены', 'The Minutemen'),
    'Братство Стали': ('bos', 'Братство Стали', 'Brotherhood of Steel'),
    'Подземка': ('railroad', 'Подземка', 'The Railroad'),
    'Институт': ('institute', 'Институт', 'The Institute'),
    'Побочные квесты': ('side', 'Побочные квесты', 'Side quests'),
    'Повторяющиеся квесты': ('repeatable', 'Повторяющиеся квесты', 'Repeatable quests'),
    'Нефракционные квесты': ('nonfaction', 'Нефракционные квесты', 'Non-faction quests'),
    'Даймонд-сити': ('diamondcity', 'Даймонд-сити', 'Diamond City'),
    'Добрососедство': ('goodneighbor', 'Добрососедство', 'Goodneighbor'),
    'Убежище 81': ('vault81', 'Убежище 81', 'Vault 81'),
    'Кэботы': ('cabots', 'Кэботы', 'The Cabots'),
    'Банкер-хилл': ('bunkerhill', 'Банкер-хилл', 'Bunker Hill'),
    'Остальные': ('other', 'Остальные', 'Other'),
    'Остальное': ('other', 'Остальные', 'Other'),
    'Разное': ('misc', 'Разное', 'Miscellaneous'),
    'Неотмечаемые квесты': ('unmarked', 'Неотмечаемые квесты', 'Unmarked quests'),
    'Квесты напарников': ('companion', 'Квесты напарников', 'Companion quests'),
    'Квесты поселений': ('settlement', 'Квесты поселений', 'Settlement quests'),
    'Дополнения': ('dlc', 'Дополнения', 'Add-ons'),
    'Automatron': ('automatron', 'Automatron', 'Automatron'),
    'Far Harbor': ('farharbor', 'Far Harbor', 'Far Harbor'),
    'Побочные квесты Акадии': ('acadia', 'Акадия', 'Acadia'),
    'Побочные квесты Детей Атома': ('atom', 'Дети Атома', 'Children of Atom'),
    'Побочные квесты Фар-Харбора': ('farharbor', 'Фар-Харбор', 'Far Harbor'),
    'Побочные квесты Содружества': ('commonwealth', 'Фракции Содружества', 'Commonwealth factions'),
    'Vault-Tec Workshop': ('vaulttec', 'Vault-Tec Workshop', 'Vault-Tec Workshop'),
    'Wasteland Workshop': ('wasteland', 'Wasteland Workshop', 'Wasteland Workshop'),
    'Nuka-World': ('nukaworld', 'Nuka-World', 'Nuka-World'),
}
SKIP_SECTIONS = {'Вырезанные квесты', 'Галерея'}

# Ключ раздела дополнения -> плагин для {{DLC ID|…}} (у шаблона нет индекса плагина).
DLC_PLUGINS = {
    'dlc.automatron': 'DLCRobot.esm',
    'dlc.farharbor': 'DLCCoast.esm',
    'dlc.vaulttec': 'DLCworkshop03.esm',
    'dlc.wasteland': 'DLCworkshop01.esm',
    'dlc.nukaworld': 'DLCNukaWorld.esm',
}
ALL_PLUGINS = ['Fallout4.esm', 'DLCRobot.esm', 'DLCworkshop01.esm', 'DLCCoast.esm', 'DLCworkshop02.esm',
               'DLCworkshop03.esm', 'DLCNukaWorld.esm']

# Раздел en-вики (заголовки через « / ») -> категория: для квестов, которых нет в ru-таблицах.
EN_SECTIONS = {
    'Main quests / Prologue': 'main.prologue',
    'Main quests / Act One': 'main.act1',
    'Main quests / Act Two': 'main.act2',
    'Faction quests / The Minutemen / Main quests': 'faction.minutemen.main',
    'Faction quests / The Minutemen / Side quests': 'faction.minutemen.side',
    'Faction quests / The Minutemen / Miscellaneous quests': 'faction.minutemen.side',
    'Faction quests / The Minutemen / Radiant quests': 'faction.minutemen.repeatable',
    'Faction quests / Brotherhood of Steel / Main quests': 'faction.bos.main',
    'Faction quests / Brotherhood of Steel / Side quests': 'faction.bos.side',
    'Faction quests / Brotherhood of Steel / Radiant quests': 'faction.bos.repeatable',
    'Faction quests / Brotherhood of Steel / Repeatable quests': 'faction.bos.repeatable',
    'Faction quests / The Railroad / Main quests': 'faction.railroad.main',
    'Faction quests / The Railroad / Side quests': 'faction.railroad.side',
    'Faction quests / The Railroad / Radiant quests': 'faction.railroad.repeatable',
    'Faction quests / The Institute / Main quests': 'faction.institute.main',
    'Faction quests / The Institute / Side quests': 'faction.institute.side',
    'Faction quests / The Institute / Radiant quests': 'faction.institute.repeatable',
    'Side quests / Secondary quests': 'side.nonfaction.other',
    'Side quests / Miscellaneous quests': 'side.misc.other',
    'Side quests / Unmarked quests': 'side.unmarked',
    'Side quests / Companion quests': 'side.companion',
    'Add-on quests / Automatron / Main quests': 'dlc.automatron.main',
    'Add-on quests / Automatron / Radiant quests': 'dlc.automatron.repeatable',
    'Add-on quests / Far Harbor / Main quests': 'dlc.farharbor.main',
    'Add-on quests / Far Harbor / General side quests': 'dlc.farharbor.side.other',
    'Add-on quests / Far Harbor / Acadia side quests': 'dlc.farharbor.side.acadia',
    'Add-on quests / Far Harbor / Children of Atom side quests': 'dlc.farharbor.side.atom',
    'Add-on quests / Far Harbor / Far Harbor side quests': 'dlc.farharbor.side.farharbor',
    'Add-on quests / Far Harbor / Commonwealth Factions quests': 'dlc.farharbor.side.commonwealth',
    'Add-on quests / Far Harbor / Radiant quests': 'dlc.farharbor.repeatable',
    'Add-on quests / Vault-Tec Workshop': 'dlc.vaulttec.main',
    'Add-on quests / Nuka-World / Main quests': 'dlc.nukaworld.main',
    'Add-on quests / Nuka-World / Side quests': 'dlc.nukaworld.side',
    'Add-on quests / Nuka-World / Radiant quests': 'dlc.nukaworld.repeatable',
}
EN_DLC_PLUGINS = {
    'Automatron': 'DLCRobot.esm', 'Far Harbor': 'DLCCoast.esm', 'Vault-Tec Workshop': 'DLCworkshop03.esm',
    'Nuka-World': 'DLCNukaWorld.esm',
}

# Квесты без ID в таблицах вики (или с неверным): ключ квеста -> категория. Заполняется по отчёту.
MANUAL = {
    'DLCRobot.esm|00AF50': 'dlc.automatron.repeatable',  # Rogue Robot (Minutemen); ru-вики: опечатка {{DLC ID|00ad50}}
    # Квесты-указатели, чей editor ID не начинается с editor ID квеста с вики (родитель — по фрагментам скриптов).
    'Fallout4.esm|14B718': 'side.nonfaction.other',         # MS05MiscSalem -> MS05B «Роковые яйца»
    'Fallout4.esm|148BAC': 'side.nonfaction.other',         # MS10MiscTrinityTower -> MS10b «Выход на поклон»
    'Fallout4.esm|1437DB': 'side.misc.diamondcity',         # FFDiamondCityWantedPosterMisc -> FFDiamondCityWanted*
    'DLCCoast.esm|04B1B9': 'dlc.farharbor.side.atom',       # DLC03CoA_AtomM02_Misc -> DLC03AtomM02 «Еретичка»
    'DLCCoast.esm|036761': 'dlc.farharbor.side.other',      # DLC03_V118_MiscObjectives -> DLC03_V118_Quest «Смерть мозга»
    'DLCNukaWorld.esm|01E34B': 'dlc.nukaworld.main',        # DLC04GZMiscObjective -> DLC04GZMainQuest «Звёздный диспетчер»
    'DLCNukaWorld.esm|051012': 'dlc.nukaworld.main',        # DLC04ShankMiscObjective -> выдаёт DLC04MQ05 «Демонстрация силы»
}

USER_COMMENT = [
    'Nearest Quest Tracker: your own quest categories (for quests from other mods, or to fix a vanilla one).',
    'Copy this file to QuestCategories-user.json in this folder. The example file itself is never read.',
    '"quests": one object per quest, {"quest": "Plugin.esp|ID", "category": "key"}; ID is the FormID as xEdit',
    '  shows it: 0F12, 00000F12, 01000F12 or FE001F12 (ESL) all work. An entry here replaces the built-in one.',
    '"category" is a key from QuestCategories.json ("side.misc.diamondcity") or a new one from "categories".',
    '"categories": optional new categories, {"key": "parent.child", "ru": "...", "en": "..."}; the parent key',
    '  must exist (built-in or listed earlier in this file). New ones go after the built-in ones of the same parent.',
    'Quests without a category go to "Other quests", grouped by plugin.',
    'Example (the plugin is made up):',
    '  {"categories": [{"key": "side.mymod", "ru": "Мой мод", "en": "My mod"}],',
    '   "quests": [{"quest": "MyQuestMod.esp|000F12", "category": "side.mymod"}]}',
]


def fetch(lang, refresh):
    path = os.path.join(CACHE, lang + '.json')
    if refresh or not os.path.exists(path):
        api, page = PAGES[lang]
        url = api + '?action=parse&prop=wikitext&format=json&formatversion=2&page=' + urllib.parse.quote(page)
        os.makedirs(CACHE, exist_ok=True)
        # WebFetch/urllib get 402/403 from fandom; curl with a browser UA works.
        subprocess.run(['curl', '-s', '-f', '-A', 'Mozilla/5.0', url, '-o', path], check=True)
    with open(path, encoding='utf-8') as f:
        return json.load(f)['parse']['wikitext']


def clean(s):
    s = re.sub(r'<ref[^>]*>.*?</ref>|<ref[^>]*/>', '', s, flags=re.S)
    s = re.sub(r'\[\[(?:[^|\]]*\|)?([^\]]*)\]\]', r'\1', s)
    s = re.sub(r'<br\s*/?>', ' ', s)
    s = re.sub(r"'''?|<[^>]+>", '', s)
    return re.sub(r'\s+', ' ', s).strip()


ID_RE = re.compile(r'\{\{(ID|DLC[ _]ID)\|\s*([0-9A-Fa-f]+)\s*\}\}')


def table_rows(text):
    """(заголовки разделов, ячейки строки) для каждой строки каждой таблицы."""
    heads, cells = {}, None
    for line in text.split('\n'):
        m = re.match(r'^(=+)\s*(.*?)\s*\1\s*$', line)
        if m:
            if cells:
                yield [heads[k] for k in sorted(heads)], cells
            cells = None
            lvl = len(m.group(1))
            heads = {k: v for k, v in heads.items() if k < lvl}
            heads[lvl] = clean(m.group(2))
            continue
        if line.startswith('|-') or line.startswith('|}'):
            if cells:
                yield [heads[k] for k in sorted(heads)], cells
            cells = [] if line.startswith('|-') else None
            continue
        if cells is not None and line.startswith('|') and not line.startswith('|+'):
            body = re.sub(r'^\s*data-row-id="\d+"\s*\|', '', line[1:])
            # Several cells on one line: "| a || b".
            cells.extend(c.strip() for c in body.split('||'))
        elif cells is not None and cells and not line.startswith('!'):
            cells[-1] += '\n' + line


def quest_key(index, plugin, raw):
    """Ключ QUST в индексе по ID из шаблона, или None."""
    local = int(raw, 16) & 0xFFFFFF
    key = '%s|%06X' % (plugin, local)
    q = index['quests'].get(key)
    return key if q else None


def resolve_ids(index, row_text, dlc_plugin):
    """[(шаблон, ID, ключ квеста или None)] для всех ID-шаблонов строки."""
    out = []
    for tmpl, raw in ID_RE.findall(row_text):
        if tmpl == 'ID':
            plugin = 'Fallout4.esm' if len(raw) < 8 or raw[:2] == '00' else None
            key = quest_key(index, plugin, raw) if plugin else None
        elif dlc_plugin:
            key = quest_key(index, dlc_plugin, raw)
        else:
            # {{DLC ID}} вне раздела дополнения — ищем во всех дополнениях, годится единственный QUST.
            found = [k for p in ALL_PLUGINS[1:] if (k := quest_key(index, p, raw))]
            key = found[0] if len(found) == 1 else None
        out.append((tmpl, raw, key))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--refresh', action='store_true', help='download the wiki pages again')
    ap.add_argument('--index', default=DEFAULT_INDEX)
    ap.add_argument('--ae-index', default=DEFAULT_AE_INDEX)
    a = ap.parse_args()

    with open(a.index, 'rb') as f:
        index = pickle.load(f)
    report = []

    # --- ru: дерево и квесты ---
    categories = {}  # ключ -> {key, ru, en}; порядок вставки = порядок на странице
    assigned = {}    # ключ квеста -> (категория, название)

    def category_of(heads):
        keys = []
        for h in heads:
            if h not in TITLES:
                raise SystemExit('unknown ru section title %r (add it to TITLES)' % h)
            k, ru, en = TITLES[h]
            keys.append(k)
            path = '.'.join(keys)
            categories.setdefault(path, {'key': path, 'ru': ru, 'en': en})
        return '.'.join(keys)

    for heads, cells in table_rows(fetch('ru', a.refresh)):
        if not heads or heads[0] in SKIP_SECTIONS or not cells:
            continue
        cat = category_of(heads)
        name = clean(cells[0])
        dlc = next((p for k, p in DLC_PLUGINS.items() if cat == k or cat.startswith(k + '.')), None)
        ids = resolve_ids(index, '\n'.join(cells[1:]), dlc)
        if not ids:
            report.append('ru: no ID: %s / %s' % (cat, name))
        for tmpl, raw, key in ids:
            if not key:
                report.append('ru: not a quest: %s / %s {{%s|%s}}' % (cat, name, tmpl, raw))
            elif key in assigned:
                report.append('ru: duplicate: %s / %s %s (already in %s)' % (cat, name, key, assigned[key][0]))
            else:
                assigned[key] = (cat, name)
    ru_count = len(assigned)

    # --- en: квесты, которых нет в ru-таблицах ---
    for heads, cells in table_rows(fetch('en', a.refresh)):
        if not heads or not cells:
            continue
        section = ' / '.join(heads)
        name = clean(cells[1] if cells[0].startswith('[[File:') else cells[0])
        dlc = EN_DLC_PLUGINS.get(heads[1]) if heads[0] == 'Add-on quests' and len(heads) > 1 else None
        for tmpl, raw, key in resolve_ids(index, '\n'.join(cells), dlc):
            if not key or key in assigned:
                continue
            cat = EN_SECTIONS.get(section)
            if cat and cat in categories:
                assigned[key] = (cat, name)
                report.append('en: added %s %s -> %s' % (key, name, cat))
            else:
                report.append('en: not in ru, section %r not mapped: %s %s' % (section, key, name))

    for key, cat in MANUAL.items():
        assigned[key] = (cat, index['quests'][key].get('edid', ''))

    # --- Creation Club (AE): квесты с целями из cc-плагинов ---
    if os.path.exists(a.ae_index):
        from ba2_strings import StringTables
        with open(a.ae_index, 'rb') as f:
            ae = pickle.load(f)
        strings = StringTables(AE_DATA)
        categories.setdefault(CC_CATEGORY['key'], dict(CC_CATEGORY))
        cc = 0
        for key, q in sorted(ae['quests'].items()):
            plugin = key.split('|')[0]
            if not plugin.lower().startswith('cc') or not q['objectives'] or key in CC_SKIP:
                continue
            name = (q.get('full') is not None and (strings.text(plugin, q['full'], 'ru')
                                                   or strings.text(plugin, q['full'], 'en'))) or q.get('edid', '')
            assigned[key] = (CC_CATEGORY['key'], name)
            index['quests'][key] = q
            cc += 1
        report.append('cc: %d quests from %s' % (cc, a.ae_index))
    else:
        report.append('cc: no AE index %s - Creation Club category not built' % a.ae_index)

    # Квесты-указатели («Разное» в Pip-Boy: FFDiamondCity01Misc, BoSM01Pointer, MS07cPostQuest…): на вики их нет,
    # editor ID начинается с editor ID квеста с вики -> его категория. Только квесты с целями (без целей в Pip-Boy
    # не попасть) и не вырезанные (_OLD).
    by_edid = {index['quests'][k].get('edid', ''): k for k in assigned}
    for key, q in sorted(index['quests'].items()):
        edid = q.get('edid') or ''
        if key in assigned or not q['objectives'] or edid.endswith('_OLD'):
            continue
        parent = max((e for e in by_edid if e and edid.startswith(e) and e != edid), key=len, default=None)
        if parent:
            cat = assigned[by_edid[parent]][0]
            assigned[key] = (cat, edid)
            report.append('pointer: %s %s -> %s (%s)' % (key, edid, cat, parent))

    # Пустые разделы (например «Третий акт» — на вики список ссылок на квесты фракций) — не пишем.
    used = set()
    for cat, _ in assigned.values():
        parts = cat.split('.')
        used.update('.'.join(parts[:i]) for i in range(1, len(parts) + 1))
    tree = [c for k, c in categories.items() if k in used]
    for k in categories:
        if k not in used:
            report.append('empty category dropped: %s' % k)

    quests = [{'quest': key, 'category': cat, 'edid': index['quests'][key].get('edid', ''), 'name': name}
              for key, (cat, name) in assigned.items()]
    os.makedirs(OUT_DIR, exist_ok=True)
    with open(os.path.join(OUT_DIR, 'QuestCategories.json'), 'w', encoding='utf-8') as f:
        json.dump({'_comment': ['Generated by tools/gen_quest_categories.py from the Fallout wiki (fallout.fandom.com) '
                                'quest tables. Do not edit: use QuestCategories-user.json (see the example file).'],
                   'categories': tree, 'quests': quests}, f, ensure_ascii=False, indent=1)
    with open(os.path.join(OUT_DIR, 'QuestCategories-user.example.json'), 'w', encoding='utf-8') as f:
        json.dump({'_comment': USER_COMMENT, 'categories': [], 'quests': []}, f, ensure_ascii=False, indent=1)

    # Квесты мастеров с типом из DNAM (1-5, 7: настоящие квесты Pip-Boy), которых нет на вики.
    missing = sorted(k for k, q in index['quests'].items() if q.get('type') in (1, 2, 3, 4, 5, 7) and k not in assigned)
    for k in missing:
        q = index['quests'][k]
        report.append('not on wiki: %s %s type %d' % (k, q.get('edid', ''), q['type']))

    with open(os.path.join(CACHE, 'report.txt'), 'w', encoding='utf-8') as f:
        f.write('\n'.join(report) + '\n')
    print('categories %d, quests %d (ru %d, en/manual %d); report: %d lines in tools/cache/report.txt'
          % (len(tree), len(quests), ru_count, len(quests) - ru_count, len(report)))


if __name__ == '__main__':
    main()
