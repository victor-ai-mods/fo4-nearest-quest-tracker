"""
Индекс ванильных мастеров для сбора точек старта квестов (этап 3a).

Читает Fallout4.esm + DLC и сохраняет в pickle то, что нужно quest_starts.py:
  quests  : QUST — EDID, тип, флаги, стадии, цели (QSTA-алиасы), алиасы (ALFR/ALUA)
  infos   : INFO — квест-владелец (через DIAL QNAM), говорящий (ANAM), TIQS, условия, есть ли фрагмент
  refs    : ACHR (все) и REFR (только persistent) — база, ячейка, мир, позиция, флаги
  bases   : EDID базовых форм (NPC_, ACTI, DOOR, STAT, ...), у NPC_ — флаги ACBS
  cells   : CELL — EDID, мир (None для интерьера); worlds: WRLD — EDID
Ключ формы везде — строка "Plugin.esm|XXXXXX" (локальный ID без байта индекса).

python tools/esm_index.py [--out <pickle>]
python tools/esm_index.py --ae [--out <pickle>]   — AE 1.11.x: мастера + все cc-плагины из Fallout4.ccc
                                                   (D:\\Games\\F4_AE, по умолчанию esm_index_ae.pickle)
"""

import argparse
import os
import pickle
import struct
import zlib

GAME = os.environ.get('FO4_PATH', r'D:\Games\Fallout 4')
MASTERS = ['Fallout4.esm', 'DLCRobot.esm', 'DLCworkshop01.esm', 'DLCCoast.esm',
           'DLCworkshop02.esm', 'DLCworkshop03.esm', 'DLCNukaWorld.esm']
DEFAULT_OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', '_research',
                           'nqt-quests', 'esm_index.pickle')
AE_GAME = r'D:\Games\F4_AE'
AE_OUT = os.path.join(os.path.dirname(DEFAULT_OUT), 'esm_index_ae.pickle')


def cc_plugins(game):
    """Creation Club plugins in load order (Fallout4.ccc), those present in Data."""
    with open(os.path.join(game, 'Fallout4.ccc'), encoding='utf-8') as f:
        names = [ln.strip() for ln in f if ln.strip()]
    present = {n.lower(): n for n in os.listdir(os.path.join(game, 'Data'))}
    return [present[n.lower()] for n in names if n.lower() in present]

COMPRESSED = 0x00040000
REC = struct.Struct('<4sIIIIHH')
GRP = struct.Struct('<4sIIIQ')
SUB = struct.Struct('<4sH')
CTDA = struct.Struct('<B3sIHHIIHHIi')   # сравнение читается как u32: float или GLOB

WANT_BASE = {b'NPC_', b'ACTI', b'DOOR', b'STAT', b'CONT', b'FURN', b'TERM', b'MISC', b'BOOK', b'NOTE',
             b'TACT', b'LIGH', b'FLOR', b'IDLM', b'MSTT', b'ALCH', b'WEAP', b'ARMO', b'KEYM', b'SCOL'}
FORM_PARAM_FUNCS = (56, 58, 59, 72, 543)   # GetQuestRunning, GetStage, GetStageDone, GetIsID, GetQuestCompleted


def subrecords(d):
    pos, n, big = 0, len(d), None
    while pos + 6 <= n:
        tag, size = SUB.unpack_from(d, pos)
        pos += 6
        if tag == b'XXXX':
            big = struct.unpack_from('<I', d, pos)[0]
            pos += size
            continue
        if big is not None:
            size, big = big, None
        yield tag, d[pos:pos + size]
        pos += size


def zstr(p):
    return p.rstrip(b'\x00').decode('cp1252', 'replace') if p else None


def u32(p):
    return struct.unpack_from('<I', p, 0)[0]


class Indexer:
    def __init__(self):
        self.quests, self.infos, self.refs, self.bases = {}, [], {}, {}
        self.cells, self.worlds, self.dials = {}, {}, {}

    def load(self, path):
        name = os.path.basename(path)
        with open(path, 'rb') as f:
            buf = f.read()
        sig, size, flags, fid, *_ = REC.unpack_from(buf, 0)
        masters = [zstr(p) for t, p in subrecords(buf[24:24 + size]) if t == b'MAST']
        self.name = name

        def key(fid):
            if fid == 0:
                return None
            idx = fid >> 24
            return '%s|%06X' % (masters[idx] if idx < len(masters) else name, fid & 0xFFFFFF)
        self.key = key
        self.walk(buf, 24 + size, len(buf), None, None, None)

    def walk(self, buf, pos, end, world, cell, dial):
        while pos + 24 <= end:
            sig, size, flags, fid, *_ = REC.unpack_from(buf, pos)
            if sig == b'GRUP':
                _, gsize, label, gtype, _ = GRP.unpack_from(buf, pos)
                w, c, dl = world, cell, dial
                if gtype == 1:
                    w = self.key(label)
                elif gtype in (6, 8, 9, 10):
                    c = self.key(label)
                elif gtype == 7:
                    dl = self.key(label)
                elif gtype == 0:
                    w = None
                self.walk(buf, pos + 24, pos + gsize, w, c, dl)
                pos += gsize
                continue
            body = buf[pos + 24:pos + 24 + size]
            pos += 24 + size
            if sig in (b'REFR', b'ACHR', b'QUST', b'INFO', b'DIAL', b'CELL', b'WRLD') or sig in WANT_BASE:
                if flags & COMPRESSED:
                    body = zlib.decompress(body[4:])
                # REFR: persistent ones, and placed books/notes (a quest can start from reading one).
                if sig == b'REFR' and not flags & 0x400:
                    name = next((p for t, p in subrecords(body) if t == b'NAME'), None)
                    if not name or self.bases.get(self.key(u32(name)), {}).get('sig') != 'BOOK':
                        continue
                self.record(sig, flags, self.key(fid), body, world, cell, dial)

    def record(self, sig, flags, k, body, world, cell, dial):
        if sig in (b'REFR', b'ACHR'):
            r = {'sig': sig.decode(), 'flags': flags, 'cell': cell, 'world': world, 'plugin': self.name}
            for t, p in subrecords(body):
                if t == b'NAME':
                    r['base'] = self.key(u32(p))
                elif t == b'DATA' and len(p) >= 24:
                    r['pos'] = struct.unpack_from('<3f', p, 0)
                elif t == b'EDID':
                    r['edid'] = zstr(p)
                elif t == b'XTEL' and len(p) >= 4:
                    r['door'] = self.key(u32(p))
            self.refs[k] = r
        elif sig == b'CELL':
            c = {'world': world, 'plugin': self.name}
            for t, p in subrecords(body):
                if t == b'EDID':
                    c['edid'] = zstr(p)
                elif t == b'FULL' and len(p) == 4:
                    c['full'] = u32(p)
            self.cells.setdefault(k, c)
        elif sig == b'WRLD':
            for t, p in subrecords(body):
                if t == b'EDID':
                    self.worlds[k] = zstr(p)
        elif sig == b'DIAL':
            for t, p in subrecords(body):
                if t == b'QNAM':
                    self.dials[k] = self.key(u32(p))
        elif sig == b'INFO':
            self.info(k, body, dial)
        elif sig == b'QUST':
            self.quest(k, body)
        else:
            b = {'sig': sig.decode(), 'plugin': self.name}
            for t, p in subrecords(body):
                if t == b'EDID':
                    b['edid'] = zstr(p)
                elif t == b'FULL' and len(p) == 4:
                    b['full'] = u32(p)
                elif t == b'ACBS' and sig == b'NPC_':
                    b['npcflags'] = u32(p)
            self.bases.setdefault(k, b)

    def conds(self, subs):
        out = []
        for t, p in subs:
            if t == b'CTDA' and len(p) >= 32:
                op, _, val, func, _, p1, p2, runon, _, ref, p3 = CTDA.unpack_from(p)
                c = {'op': op >> 5, 'or': bool(op & 1), 'glob': bool(op & 4),
                     'val': val if op & 4 else struct.unpack('<f', struct.pack('<I', val))[0],
                     'func': func, 'p1': p1, 'p2': p2, 'runon': runon, 'ref': ref}
                if func in FORM_PARAM_FUNCS:
                    c['p1'] = self.key(p1)
                out.append(c)
        return out

    def info(self, k, body, dial):
        subs = list(subrecords(body))
        i = {'id': k, 'dial': dial, 'plugin': self.name, 'frag': False}
        for t, p in subs:
            if t == b'EDID':
                i['edid'] = zstr(p)
            elif t == b'ANAM':
                i['speaker'] = self.key(u32(p))
            elif t == b'TIQS':
                i['tiqs'] = struct.unpack('<hh', p)
            elif t == b'VMAD':
                i['frag'] = True
                i['vmad'] = p
            elif t == b'DNAM':
                i['shared'] = self.key(u32(p))
            elif t == b'TSCE':
                i['scene'] = self.key(u32(p))
        i['conds'] = self.conds(subs)
        self.infos.append(i)

    def quest(self, k, body):
        q = {'id': k, 'plugin': self.name, 'stages': [], 'objectives': [], 'aliases': []}
        cur_obj = cur_alias = None
        for t, p in subrecords(body):
            if t == b'EDID':
                q['edid'] = zstr(p)
            elif t == b'FULL' and len(p) == 4:
                q['full'] = u32(p)
            elif t == b'DNAM':
                fl, prio, _, delay, typ = struct.unpack_from('<HBBfI', p)
                q['flags'], q['priority'], q['type'] = fl, prio, typ
            elif t == b'INDX':
                idx, fl = struct.unpack_from('<HB', p)
                q['stages'].append((idx, fl))
            elif t == b'QOBJ':
                cur_obj = {'index': struct.unpack('<H', p)[0], 'targets': []}
                q['objectives'].append(cur_obj)
            elif t == b'QSTA' and cur_obj is not None:
                cur_obj['targets'].append(struct.unpack_from('<i', p)[0])
            elif t in (b'ALST', b'ALLS', b'ALCS'):
                cur_obj = None
                cur_alias = {'kind': t.decode(), 'id': u32(p)}
                q['aliases'].append(cur_alias)
            elif cur_alias is not None:
                if t == b'ALID':
                    cur_alias['name'] = zstr(p)
                elif t == b'FNAM':
                    cur_alias['flags'] = u32(p)
                elif t == b'ALFR':
                    cur_alias['forced'] = self.key(u32(p))
                elif t == b'ALUA':
                    cur_alias['unique'] = self.key(u32(p))
                elif t == b'ALFL':
                    cur_alias['location'] = self.key(u32(p))
                elif t == b'ALEQ':
                    cur_alias['extquest'] = self.key(u32(p))
                elif t == b'ALEA':
                    cur_alias['extalias'] = struct.unpack('<i', p)[0]
        self.quests[k] = q


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out')
    ap.add_argument('--ae', action='store_true')
    a = ap.parse_args()
    game = AE_GAME if a.ae else GAME
    plugins = MASTERS + (cc_plugins(game) if a.ae else [])
    a.out = a.out or (AE_OUT if a.ae else DEFAULT_OUT)
    ix = Indexer()
    for m in plugins:
        print('reading', m, flush=True)
        ix.load(os.path.join(game, 'Data', m))
    data = {k: getattr(ix, k) for k in ('quests', 'infos', 'refs', 'bases', 'cells', 'worlds', 'dials')}
    os.makedirs(os.path.dirname(os.path.abspath(a.out)), exist_ok=True)
    with open(a.out, 'wb') as f:
        pickle.dump(data, f, protocol=pickle.HIGHEST_PROTOCOL)
    print({k: len(v) for k, v in data.items()})


if __name__ == '__main__':
    main()
