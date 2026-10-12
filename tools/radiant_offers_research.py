"""
Реплики, которыми квестодатель предлагает радиантный (повторяемый) квест, — кандидаты для полей
`radiant info=...` в quest_starts.txt.

Реплика-кандидат для квеста Q:
  - есть условие «говорит он»: GetIsID(NPC) или GetIsAliasRef(алиас квеста-владельца реплики);
  - все условия на Q — «ещё не взят»: GetStageDone(Q, s) == 0 (или != 1), GetQuestRunning(Q) == 0,
    GetStage(Q) == v при v <= 10, GetStage(Q) < / <= v при v <= 20 (== 40 и т.п. — сдача задания);
  - реплика в топиках самого Q или диалогового квеста (EDID с «Dialogue»), не вступление к другому квесту;
  - не реплика игрока (GetIsID(Player)).
Квестодатель: NPC из GetIsID, или уникальный актёр / forced ref алиаса, и его размещённый ref.
Выбор — вручную (отчёт для глаз).

python tools/radiant_offers_research.py [EDID ...] [--emit]
  --emit: строки для quest_starts.txt (по строке на квест и квестодателя-NPC; алиасы без NPC — пропуск).
"""

import argparse
import os
import pickle
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from gen_quest_starts import DEFAULT_INDEX  # noqa: E402

GET_STAGE, GET_STAGE_DONE, GET_QUEST_RUNNING, GET_IS_ID, GET_IS_ALIAS_REF = 58, 59, 56, 72, 566
PLAYER = 'Fallout4.esm|000007'
DEFAULT = ['MinRecruit01', 'MinRecruit02', 'MinRecruit03', 'MinRecruit04', 'MinRecruit05', 'MinRecruit06', 'MinRecruit07',
           'MinRecruit08', 'MinRecruit09', 'MinRadiantOwned01', 'MinRadiantOwned02', 'MinRadiantOwned03ChangeLocOnly',
           'MinRadiantOwned04_BOS', 'MinRadiantOwned05', 'MinRadiantOwned06ChangeLocOnly', 'MinRadiantOwned07ChangeLocOnly',
           'MinRadiantOwned08', 'MinRadiantOwned09ChangeLocOnly', 'MinRadiantOwned10ChangeLocOnly', 'MinRadiantOwned11',
           'BoSR01', 'BosR02', 'BoSR03', 'BoSR04', 'BoSR05', 'RRR02a', 'RRR03', 'RRR05', 'RRR07', 'RRR09', 'RRR10', 'RRR11',
           'InstR01', 'InstR02', 'InstR03NEW', 'InstR04', 'InstR05']


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('quests', nargs='*')
    ap.add_argument('--index', default=DEFAULT_INDEX)
    ap.add_argument('--emit', action='store_true')
    a = ap.parse_args()
    with open(a.index, 'rb') as f:
        ix = pickle.load(f)
    quests, infos, dials, bases, refs = ix['quests'], ix['infos'], ix['dials'], ix['bases'], ix['refs']
    by_edid = {q.get('edid'): k for k, q in quests.items()}
    refs_by_base = {}
    for k, r in refs.items():
        if r['sig'] == 'ACHR':
            refs_by_base.setdefault(r.get('base'), []).append(k)

    def npc_name(k):
        return bases.get(k, {}).get('edid') or k

    def giver_of(info, owner):
        for c in info['conds']:
            if c['func'] == GET_IS_ID and c['op'] == 0 and c['val'] == 1 and c['p1'] != PLAYER:
                return c['p1'], refs_by_base.get(c['p1'], [])
            if c['func'] == GET_IS_ALIAS_REF and c['op'] == 0 and c['val'] == 1:
                alias = next((x for x in quests.get(owner, {}).get('aliases', ()) if x['id'] == c['p1']), None)
                if alias:
                    if alias.get('unique'):
                        return alias['unique'], refs_by_base.get(alias['unique'], [])
                    if alias.get('forced'):
                        r = refs.get(alias['forced'], {})
                        return r.get('base'), [alias['forced']]
                    return 'alias %s %s' % (c['p1'], alias.get('name')), []
        return None, []

    for edid in a.quests or DEFAULT:
        qk = by_edid.get(edid)
        if not qk:
            print('?', edid)
            continue
        if not a.emit:
            print('=== %s %s flags %X' % (edid, qk, quests[qk].get('flags', 0)))
        groups = {}
        for i in infos:
            mine = [c for c in i['conds'] if c['p1'] == qk and c['func'] in (GET_STAGE, GET_STAGE_DONE, GET_QUEST_RUNNING)]
            if not mine:
                continue
            ops = ['==', '!=', '>', '>=', '<', '<=']

            def is_before(c):
                op = ops[c['op']]
                if c['func'] in (GET_STAGE_DONE, GET_QUEST_RUNNING):
                    return (op == '==' and c['val'] == 0) or (c['func'] == GET_STAGE_DONE and op == '!=' and c['val'] == 1)
                # GetStage: только ранние стадии; == 40 / 80 и т.п. — сдача задания, а не предложение
                return (op == '==' and c['val'] <= 10) or (op in ('<', '<=') and c['val'] <= 20)

            # все условия на Q — «ещё не взят» (одно «уже взят / сдаёт» — не предложение)
            if not all(is_before(c) for c in mine):
                continue
            if any(c['func'] == GET_IS_ID and c['p1'] == PLAYER for c in i['conds']):
                continue
            owner = dials.get(i['dial'])
            # реплика из топиков самого Q или диалогового квеста фракции, а не вступление к другому сюжетному
            if owner != qk and 'dialogue' not in (quests.get(owner, {}).get('edid') or '').lower():
                continue
            npc, grefs = giver_of(i, owner)
            if not npc:
                continue
            if '|' in str(npc) and grefs:
                groups.setdefault(npc, (grefs, []))[1].append(i['id'])
            if a.emit:
                continue
            print('  INFO %s in %-26s giver %-28s refs %s tiqs %s frag %s scene %s' % (
                i['id'], quests.get(owner, {}).get('edid'), npc_name(npc) if '|' in str(npc) else npc,
                ','.join('%s%s' % (r, '(P)' if refs[r]['flags'] & 0x400 else '') for r in grefs[:3]),
                i.get('tiqs'), i['frag'], i.get('scene')))
        if a.emit:
            for npc, (grefs, ids) in groups.items():
                ref = next((r for r in grefs if refs[r]['flags'] & 0x400), grefs[0])
                print('%-24s %s  radiant info=%s  # %s' % (edid, ref, ','.join(sorted({x.split('|')[1] for x in ids})),
                                                           bases.get(npc, {}).get('edid')))


if __name__ == '__main__':
    main()
