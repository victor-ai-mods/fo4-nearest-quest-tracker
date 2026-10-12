"""
Этап 3a: tools/quest_starts.txt + индекс мастеров -> mod/F4SE/Plugins/NearestQuestTracker/QuestStarts.json.

Для каждой записи, кроме квеста и ref квестодателя, считается статичная точка для расстояния,
когда ref не загружен (временные ref'ы NPC есть в памяти, только пока загружена их ячейка):
  world  — основной мир (Содружество / Фар-Харбор / Нюка-Мир), в котором лежит точка;
  pos    — координаты точки в этом мире;
  anchor — persistent-ref в этой точке (для маркера в 3b):
           NPC в интерьере или под-мире (Даймонд-Сити, Добрососедство, …) -> наружная дверь, до
           которой ведёт цепочка дверей из его ячейки; NPC снаружи -> сам ref, если он persistent,
           иначе ближайший маркер карты (или ближайший persistent-ref);
  marker — persistent-ref в ячейке/мире самого NPC, на который DLL ставит маркер, пока NPC не загружен
           (этап 3b): для NPC в интерьере или под-мире — дверь, через которую цепочка выходит наружу
           (движок строит к ней путь через все двери, и маркер не остаётся позади, когда игрок уже
           внутри цепочки); для NPC снаружи поле не пишется — берётся anchor.

python tools/gen_quest_starts.py [--index <pickle>]
"""

import argparse
import json
import math
import os
import pickle
import re
from collections import deque

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(HERE, 'quest_starts.txt')
OUT_DIR = os.path.join(ROOT, 'mod', 'F4SE', 'Plugins', 'NearestQuestTracker')
DEFAULT_INDEX = os.path.join(ROOT, '..', '_research', 'nqt-quests', 'esm_index.pickle')

ROOT_WORLDS = {'Fallout4.esm|00003C', 'DLCCoast.esm|000B0F', 'DLCNukaWorld.esm|00290F'}
MAP_MARKER = 'Fallout4.esm|000010'
PERSISTENT, DISABLED = 0x400, 0x800

USER_EXAMPLE = {
    '_comment': [
        'Nearest Quest Tracker: your own quest start points (for quests from other mods, or to fix a vanilla one).',
        'Copy this file to QuestStarts-user.json in this folder. The example file itself is never read.',
        'One object per quest start:',
        '  "quest"  - the quest, "Plugin.esp|ID"; ID is the FormID as xEdit shows it: 0F12, 00000F12, 01000F12 or FE001F12 (ESL) all work.',
        '  "ref"    - the placed reference (NPC, activator, door, ...) where the quest starts, "Plugin.esp|ID".',
        '             Best is a persistent reference: a temporary one is only known to the game while its cell is loaded.',
        '  "after"  - optional: quests that must be completed first, ["Plugin.esp|ID", ...]; an element can also be',
        '             a list of quests of which any one will do: ["Plugin.esp|A", ["Plugin.esp|B", "Plugin.esp|C"]].',
        '  "objective" - optional: the quest counts as started once THIS objective has been shown (several entries',
        '             for one quest, e.g. one per note that starts one objective).',
        '  "stage"  - optional: the quest counts as started once this stage is done (for quests that show no objective).',
        '  "radio"  - optional: "ref" is a radio transmitter and the quest starts when its signal is picked up;',
        '             the value is the reception radius in game units (distances are measured to its edge).',
        '  "running" - optional, true: the giver offers the quest only while the quest is already running (its dialogue',
        '             belongs to the quest itself; the game starts it in the background, e.g. on entering a location).',
        '             Near such a giver a quest that is not running is shown as "not offered now" and skipped.',
        'The quest counts as started (and its start point is no longer shown) once it is completed or any of',
        'its objectives has been shown in the Pip-Boy (unless "objective" or "stage" says otherwise).',
        'A quest can have several entries; entries for a quest in this file replace all QuestStarts.json entries for it.',
        '"world", "pos", "anchor" (a persistent ref) can be given for a place on the map while the ref is not loaded;',
        'without them the start is offered only when the ref is loaded near the player.',
        'Example (the plugin is made up):',
        '  {"quest": "MyQuestMod.esp|000F12", "ref": "MyQuestMod.esp|000F99"}',
    ],
    'starts': [],
}


def parse_src(path):
    out = []
    with open(path, encoding='utf-8') as f:
        for n, line in enumerate(f, 1):
            body, _, comment = line.partition('#')
            parts = body.split()
            if not parts:
                continue
            e = {'quest': parts[0], 'ref': parts[1], 'after': [], 'radio': None, 'objective': None, 'stage': None,
                 'inside': False, 'running': False, 'comment': comment.strip(), 'line': n}
            for p in parts[2:]:
                k, _, v = p.partition('=')
                if k == 'after':
                    e['after'] = [g.split('|') for g in v.split(',')]
                elif k == 'radio':
                    e['radio'] = float(v)
                elif k in ('objective', 'stage'):
                    e[k] = int(v)
                elif p == 'inside':
                    e['inside'] = True
                elif p == 'running':
                    e['running'] = True
                else:
                    raise SystemExit('%s:%d: неизвестное поле %r' % (path, n, p))
            out.append(e)
    return out


class Data:
    def __init__(self, ix):
        self.ix = ix
        self.refs, self.bases, self.cells, self.worlds = ix['refs'], ix['bases'], ix['cells'], ix['worlds']
        self.quest_by_edid = {q['edid'].lower(): k for k, q in ix['quests'].items() if q.get('edid')}
        self.ref_by_edid = {r['edid'].lower(): k for k, r in self.refs.items() if r.get('edid')}
        self.doors = {}
        for k, r in self.refs.items():
            if r.get('door'):
                self.doors.setdefault(self.space(r), []).append(k)

    def space(self, r):
        return r['world'] or r['cell']

    def space_name(self, s):
        return self.worlds.get(s) or self.cells.get(s, {}).get('edid') or s

    def quest(self, edid):
        k = self.quest_by_edid.get(edid.lower())
        if not k:
            raise SystemExit('квест %s не найден' % edid)
        return k

    def ref(self, s):
        if '|' in s:
            p, i = s.split('|')
            k = '%s|%06X' % (p, int(i, 16))
        else:
            k = self.ref_by_edid.get(s.lower())
        if not k or k not in self.refs:
            raise SystemExit('ref %s не найден' % s)
        return k

    def exterior(self, rk):
        """(world, pos, anchor, path, marker) — точка в основном мире для ref; marker — дверь в его ячейке."""
        r = self.refs[rk]
        start = self.space(r)
        if start in ROOT_WORLDS:
            return start, r['pos'], self.anchor_near(rk), [], None
        seen = {start}
        q = deque([(start, [], None)])
        while q:
            s, path, first = q.popleft()
            for dk in self.doors.get(s, ()):
                dest = self.refs.get(self.refs[dk]['door'])
                if not dest:
                    continue
                ns = self.space(dest)
                np = path + [self.refs[dk]['door']]
                nf = first or dk
                if ns in ROOT_WORLDS:
                    return ns, dest['pos'], self.refs[dk]['door'], np, nf
                if ns not in seen and len(np) < 8:
                    seen.add(ns)
                    q.append((ns, np, nf))
        raise SystemExit('нет пути наружу из %s (%s)' % (self.space_name(start), rk))

    def anchor_near(self, rk):
        r = self.refs[rk]
        if r['flags'] & PERSISTENT and not r['flags'] & DISABLED:
            return rk
        best = {}
        for k, o in self.refs.items():
            if (o['world'] != r['world'] or o['flags'] & DISABLED or not o['flags'] & PERSISTENT
                    or k == rk or 'pos' not in o):
                continue
            d = math.dist(o['pos'][:2], r['pos'][:2])
            kind = 'marker' if o.get('base') == MAP_MARKER else 'ref'
            if d < best.get(kind, (1e18,))[0]:
                best[kind] = (d, k)
        if 'marker' in best and best['marker'][0] < 6000:
            return best['marker'][1]
        return best['ref'][1]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--index', default=DEFAULT_INDEX)
    a = ap.parse_args()
    with open(a.index, 'rb') as f:
        data = Data(pickle.load(f))
    src = parse_src(SRC)

    starts, report, seen = [], [], set()
    for e in src:
        qk = data.quest(e['quest'])
        rk = data.ref(e['ref'])
        if (qk, rk, e['objective']) in seen:
            raise SystemExit('%s: квест %s с тем же ref указан дважды' % (e['line'], e['quest']))
        seen.add((qk, rk, e['objective']))
        after = [[data.quest(x) for x in g] for g in e['after']]
        r = data.refs[rk]
        entry = {'quest': qk, 'ref': rk}
        if after:
            entry['after'] = [g[0] if len(g) == 1 else g for g in after]
        for k in ('objective', 'stage'):
            if e[k] is not None:
                entry[k] = e[k]
        if e['radio'] is not None:
            entry['radio'] = round(e['radio'])
        if e['running']:
            entry['running'] = True
        where = 'только рядом (inside)'
        if not e['inside']:
            # Квестодатель, к которому нет пути через двери (Институт — только телепорт), — ошибка; такие
            # помечаются inside: точки на карте нет, старт предлагается, когда квестодатель загружен рядом.
            world, pos, anchor, path, marker = data.exterior(rk)
            ar = data.refs[anchor]
            dist = math.dist(ar['pos'][:2], pos[:2]) if anchor != rk and not path else 0
            entry.update({'world': world, 'pos': [round(c) for c in pos], 'anchor': anchor})
            if marker:
                mr = data.refs[marker]
                if not mr['flags'] & PERSISTENT or mr['flags'] & DISABLED:
                    raise SystemExit('дверь %s у %s не persistent или выключена' % (marker, e['quest']))
                entry['marker'] = marker
            where = '%-12s %s%s' % (data.space_name(world),
                                    ' -> '.join(data.space_name(data.space(data.refs[p])) for p in path) or 'снаружи',
                                    (' (якорь %s в %d ед.)' % (data.bases.get(ar.get('base'), {}).get('edid'), dist)) if dist else '')
        entry.update({'name': '%s %s' % (e['quest'], re.sub(r'\s+—.*', '', e['comment']))})
        starts.append(entry)
        report.append('%-26s %-24s %s' % (e['quest'], data.bases.get(r.get('base'), {}).get('edid'), where))

    os.makedirs(OUT_DIR, exist_ok=True)
    doc = {
        '_comment': [
            'Nearest Quest Tracker: start points of quests not started yet (vanilla + DLC). Generated by',
            'tools/gen_quest_starts.py from tools/quest_starts.txt - edit those, not this file.',
            'Your own entries go to QuestStarts-user.json (see QuestStarts-user.example.json).',
        ],
        'starts': starts,
    }
    with open(os.path.join(OUT_DIR, 'QuestStarts.json'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('{\n    "_comment": [\n%s\n    ],\n' % ',\n'.join('        ' + json.dumps(c) for c in doc['_comment']))
        f.write('    "starts": [\n%s\n    ]\n}\n' % ',\n'.join('        ' + json.dumps(s, ensure_ascii=False)
                                                               for s in starts))
    with open(os.path.join(OUT_DIR, 'QuestStarts-user.example.json'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('{\n    "_comment": [\n%s\n    ],\n    "starts": [\n    ]\n}\n' % ',\n'.join(
            '        ' + json.dumps(c) for c in USER_EXAMPLE['_comment']))
    print('\n'.join(report))
    print('%d точек старта -> %s' % (len(starts), OUT_DIR))


if __name__ == '__main__':
    main()
