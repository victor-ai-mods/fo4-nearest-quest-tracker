"""
AE-4: черновой отчёт по стартам квестов Creation Club — для ручного выбора строк в quest_starts.txt.

Входы (не в репозитории, ..\\_research\\nqt-quests):
  esm_index_ae.pickle      — python tools/esm_index.py --ae (в cc-плагинах — все REFR)
  cc\\cc_infobox_en.json    — выжимка en-вики по CC-квестам (given by / location / начало прохождения)
Квесты — категория dlc.cc из mod/.../QuestCategories.json. Имена и тексты целей — строки плагинов (ba2_strings.py).
Выход: cc\\cc_starts_report.txt — по квесту: флаги, стадии, цели, алиасы с принудительными ref (что это и где лежит),
уникальные актёры и строка вики.

python tools/cc_starts_research.py
"""

import json
import os
import pickle
import re

from ba2_strings import StringTables

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
RES = os.path.join(ROOT, '..', '_research', 'nqt-quests')
AE_DATA = r'D:\Games\F4_AE\Data'
ALIAS_FLAGS = {0x1: 'reserves', 0x2: 'optional', 0x4: 'quest object', 0x200: 'essential', 0x10000: 'initially disabled'}


def main():
    with open(os.path.join(RES, 'esm_index_ae.pickle'), 'rb') as f:
        ix = pickle.load(f)
    with open(os.path.join(ROOT, 'mod', 'F4SE', 'Plugins', 'NearestQuestTracker', 'QuestCategories.json'),
              encoding='utf-8') as f:
        keys = [q['quest'] for q in json.load(f)['quests'] if q['category'] == 'dlc.cc']
    with open(os.path.join(RES, 'cc', 'cc_infobox_en.json'), encoding='utf-8') as f:
        wiki = {re.sub(r'\s*\((quest|Creation Club)\)$', '', w['title']).lower(): w for w in json.load(f)}
    st = StringTables(AE_DATA)

    def text(key, sid):
        if sid is None:
            return ''
        plugin = key.split('|')[0]
        return st.text(plugin, sid, 'en') or ''

    def describe_ref(rk):
        r = ix['refs'].get(rk)
        if not r:
            return '%s (not indexed)' % rk
        b = ix['bases'].get(r.get('base'), {})
        cell = ix['cells'].get(r.get('cell'), {}).get('edid', r.get('cell'))
        world = ix['worlds'].get(r.get('world'), r.get('world'))
        pos = ' (%.0f, %.0f, %.0f)' % r['pos'] if 'pos' in r else ''
        persistent = 'P' if r['flags'] & 0x400 else 'T'
        return '%s %s %s %s "%s" in %s / %s%s' % (rk, persistent, b.get('sig', '?'), b.get('edid', ''),
                                                  text(r.get('base', ''), b.get('full')), cell, world, pos)

    out = []
    for k in keys:
        q = ix['quests'][k]
        name = text(k, q.get('full'))
        w = wiki.get(name.lower(), {})
        out.append('== %s %s "%s" type %s flags %04X stages %s' % (k, q.get('edid', ''), name, q.get('type'),
                                                                   q.get('flags', 0), [s[0] for s in q['stages']]))
        if w:
            out.append('   wiki: given by "%s"; location "%s"' % (w['given_by'], w['location']))
            if w['start']:
                out.append('   wiki: %s' % w['start'][:260])
        for o in q['objectives']:
            out.append('   obj %3d "%s" targets %s' % (o['index'], text(k, o.get('text')), o['targets']))
        for a in q['aliases']:
            flags = ', '.join(n for bit, n in ALIAS_FLAGS.items() if a.get('flags', 0) & bit)
            line = '   alias %2d %s %-28s %s' % (a['id'], a['kind'], a.get('name', ''), flags)
            if a.get('forced'):
                line += '\n            forced: ' + describe_ref(a['forced'])
            if a.get('unique'):
                b = ix['bases'].get(a['unique'], {})
                line += '\n            unique: %s %s "%s"' % (a['unique'], b.get('edid', ''), text(a['unique'], b.get('full')))
                placed = [rk for rk, r in ix['refs'].items() if r.get('base') == a['unique']]
                for rk in placed[:3]:
                    line += '\n               at: ' + describe_ref(rk)
            if a.get('location'):
                line += '\n            location: %s' % a['location']
            out.append(line)
        out.append('')
    path = os.path.join(RES, 'cc', 'cc_starts_report.txt')
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(out))
    print('%d quests -> %s' % (len(keys), path))


if __name__ == '__main__':
    main()
