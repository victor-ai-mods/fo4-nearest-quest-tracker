"""
Этап 3a: черновой отчёт «квест → точка старта» для ручной проверки.

Входы (не в репозитории, кэш в ..\\_research\\nqt-quests):
  infobox.json, givers.json   — выжимка из вики fallout.fandom.com (инфобоксы квестов и квестодателей)
  esm_index.pickle            — tools/esm_index.py
Выход: quest_starts_report.md там же — по строке на квест: тип, EDID/FormID, квестодатели с их ref
в ESM (ячейка/мир), реплики этого NPC с условиями на квест и стадии, которые из них видны.

python tools/quest_starts_research.py
"""

import json
import os
import pickle
import re
from collections import defaultdict

HERE = os.path.dirname(os.path.abspath(__file__))
RES = os.path.join(HERE, '..', '..', '_research', 'nqt-quests')

DLC_BY_PREFIX = {'DLC01': 'DLCRobot.esm', 'DLC02': 'DLCworkshop01.esm', 'DLC03': 'DLCCoast.esm',
                 'DLC05': 'DLCworkshop02.esm', 'DLC06': 'DLCworkshop03.esm', 'DLC04': 'DLCNukaWorld.esm'}
PLUGINS = ['Fallout4.esm', 'DLCRobot.esm', 'DLCworkshop01.esm', 'DLCCoast.esm',
           'DLCworkshop02.esm', 'DLCworkshop03.esm', 'DLCNukaWorld.esm']
OPS = ['==', '!=', '>', '>=', '<', '<=']
FN = {56: 'GetQuestRunning', 58: 'GetStage', 59: 'GetStageDone', 543: 'GetQuestCompleted'}


def load():
    with open(os.path.join(RES, 'esm_index.pickle'), 'rb') as f:
        ix = pickle.load(f)
    with open(os.path.join(RES, 'infobox.json'), encoding='utf-8') as f:
        rows = json.load(f)
    with open(os.path.join(RES, 'givers.json'), encoding='utf-8') as f:
        givers = json.load(f)
    with open(os.path.join(RES, 'givers_wikitext.json'), encoding='utf-8') as f:
        redirects = {k[len('#redirect:'):]: v for k, v in json.load(f).items() if k.startswith('#redirect:')}
    return ix, rows, givers, redirects


def wiki_ids(s):
    """[(dlc?, local_hex)] из {{ID|...}} / {{DLC ID|...}}."""
    return [(m.group(1) is not None, m.group(2).upper()[-6:].rjust(6, '0'))
            for m in re.finditer(r'\{\{(DLC )?ID\|([0-9a-fA-F]+)\}\}', s)]


def find_form(table, local, prefer=None):
    order = ([prefer] if prefer else []) + [p for p in PLUGINS if p != prefer]
    for p in order:
        k = '%s|%s' % (p, local)
        if k in table:
            return k
    return None


def main():
    ix, rows, givers, redirects = load()
    quests, refs, bases, cells, worlds = ix['quests'], ix['refs'], ix['bases'], ix['cells'], ix['worlds']
    by_edid = {q.get('edid', '').lower(): k for k, q in quests.items()}

    # реплики: квест -> [(info, speaker_base)]
    owner = ix['dials']
    info_by_quest = defaultdict(list)
    for i in ix['infos']:
        speakers = set()
        if 'speaker' in i:
            speakers.add(i['speaker'])
        for c in i['conds']:
            if c['func'] == 72 and c['runon'] == 0 and c['op'] == 0:
                speakers.add(c['p1'])
        qs = set()
        oq = owner.get(i['dial'])
        if oq:
            qs.add(oq)
        for c in i['conds']:
            if c['func'] in FN:
                qs.add(c['p1'])
        for q in qs:
            info_by_quest[q].append((i, speakers, oq))

    def place(rk):
        r = refs.get(rk)
        if not r:
            return 'нет в ESM'
        b = bases.get(r.get('base'), {})
        c = cells.get(r['cell'], {})
        where = worlds.get(r['world']) if r['world'] else (c.get('edid') or r['cell'])
        return '%s %s [%s] %s%s' % (r['sig'], r.get('edid') or '', b.get('edid'), where,
                                   '' if r['flags'] & 0x400 else ' (не persistent)')

    out = ['# Точки старта: черновой отчёт', '',
           'Столбцы: тип вики | EDID (FormID ESM) | квестодатель → ref | реплики с условиями на квест.', '']
    for title, typ, edid, fid, given, loc in rows:
        if typ == 'NOBOX':
            continue
        e = re.sub(r'<.*', '', edid).split('(')[0].strip()
        qk = by_edid.get(e.lower())
        if not qk:
            ids = wiki_ids(fid)
            if ids:
                qk = find_form(quests, ids[0][1], DLC_BY_PREFIX.get(e[:5]))
        q = quests.get(qk, {})
        out.append('## %s — %s' % (title, typ))
        out.append('- квест: %s %s type=%s flags=0x%X, стадии %s' % (
            qk, q.get('edid'), q.get('type'), q.get('flags', 0), [s for s, _ in q.get('stages', [])][:14]))
        out.append('- вики: дал «%s», место «%s»' % (re.sub(r'\s+', ' ', given)[:150], re.sub(r'\s+', ' ', loc)[:100]))
        giver_bases = {}
        for m in re.finditer(r'\[\[([^|\]#]+)', given):
            name = m.group(1).strip()
            name = redirects.get(name, name)
            g = givers.get(name)
            if not g:
                out.append('  - %s: нет страницы' % name)
                continue
            for dlc, local in wiki_ids(' '.join(g['refid'])):
                rk = find_form(refs, local, (q.get('plugin') if dlc else 'Fallout4.esm'))
                out.append('  - %s: ref %s → %s' % (name, rk, place(rk) if rk else '—'))
                if rk and refs[rk].get('base'):
                    giver_bases[refs[rk]['base']] = name
        # реплики квестодателей с условиями на этот квест
        stages = defaultdict(int)
        n = 0
        for i, speakers, oq in info_by_quest.get(qk, ()):
            if not speakers & set(giver_bases):
                continue
            n += 1
            for c in i['conds']:
                if c['func'] in (58, 59) and c['p1'] == qk:
                    v = c['p2'] if c['func'] == 59 else int(c['val'])
                    stages['%s %s %s%s' % (FN[c['func']], OPS[c['op']], v,
                                           (' =%g' % c['val']) if c['func'] == 59 else '')] += 1
            if 'tiqs' in i and oq == qk:
                stages['TIQS %s' % (i['tiqs'],)] += 1
        if n:
            out.append('  - реплик квестодателя про квест: %d; %s' % (n, dict(sorted(stages.items()))))
        out.append('')
    path = os.path.join(RES, 'quest_starts_report.md')
    with open(path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(out))
    print(path)


if __name__ == '__main__':
    main()
