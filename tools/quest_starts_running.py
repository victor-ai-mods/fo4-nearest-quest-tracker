"""
Какие точки старта требуют, чтобы квест УЖЕ работал (поле `running` в quest_starts.txt).

У таких квестов реплики квестодателя лежат в топиках самого квеста (DIAL QNAM = квест), а сам
квест в фоне запускает Story Manager (SMQN), например при входе в локацию. Если запуск не прошёл
(обязательный алиас не заполнился — чаще всего локацию зарезервировал другой квест), квестодатель
квест не предлагает. Пример: FFDiamondCity01 «Старый город в новом цвете» — Эббот.

Отчёт по каждому квесту из quest_starts.txt (только NPC-квестодатели, не радио):
  sm     — узлы Story Manager, которые запускают квест;
  own    — реплики квестодателя в топиках самого квеста;
  other  — реплики квестодателя в чужих топиках, VMAD которых ссылается на квест (могут его запустить);
  SGE    — флаг Start Game Enabled.
Кандидат в `running`: есть sm, есть own, нет other, нет SGE. Решение о поле — вручную.

python tools/quest_starts_running.py [--index <pickle>]
"""

import argparse
import os
import pickle
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

from esm import Plugin  # noqa: E402
from esm_index import GAME, MASTERS  # noqa: E402
from gen_quest_starts import DEFAULT_INDEX, SRC, Data, parse_src  # noqa: E402


def sm_nodes():
    """quest key -> [EDID узла SMQN] по всем мастерам."""
    out = {}
    for name in MASTERS:
        p = Plugin(os.path.join(GAME, 'Data', name))
        for r in p.records(b'SMQN'):
            for t, d in r.subrecords():
                if t == b'NNAM':
                    plugin, local = p.resolve(struct.unpack('<I', d)[0])
                    out.setdefault('%s|%06X' % (plugin, local), []).append(r.editor_id() or '%06X' % r.local_id)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--index', default=DEFAULT_INDEX)
    a = ap.parse_args()
    with open(a.index, 'rb') as f:
        ix = pickle.load(f)
    data = Data(ix)
    sm = sm_nodes()

    by_npc = {}
    for i in ix['infos']:
        npcs = {i.get('speaker')} | {c['p1'] for c in i['conds'] if c['func'] == 72}
        for n in npcs:
            if n:
                by_npc.setdefault(n, []).append(i)

    for e in parse_src(SRC):
        if e['radio'] is not None:
            continue
        qk = data.quest(e['quest'])
        base = data.refs[data.ref(e['ref'])].get('base')
        if data.bases.get(base, {}).get('sig') != 'NPC_':
            continue
        q = ix['quests'][qk]
        local = bytes.fromhex(qk.split('|')[1])[::-1]
        own = other = 0
        for i in by_npc.get(base, ()):
            owner = ix['dials'].get(i['dial'])
            if owner == qk:
                own += 1
            elif local in (i.get('vmad') or b''):
                other += 1
        sge = q.get('flags', 0) & 1
        nodes = sm.get(qk, [])
        mark = 'RUNNING?' if nodes and own and not other and not sge else ''
        print('%-9s %-24s own=%-3d other=%-2d SGE=%d sm=%s' % (mark, e['quest'], own, other, sge, ','.join(nodes)))


if __name__ == '__main__':
    main()
