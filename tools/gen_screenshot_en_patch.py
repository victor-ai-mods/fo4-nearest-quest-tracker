"""
Заплатка ТОЛЬКО для английских скриншотов (в мод не входит): русские названия квестов и тексты целей, которые
русские версии модов (Unofficial Fallout 4 Patch, Community Fixes Merged, DLC Timing, Oxy-Perevod-RU) пишут прямо в
плагин, заменяются английскими из строк игры.

Для каждого ванильного/DLC квеста берётся запись, которая побеждает в порядке загрузки профиля; если в её FULL или
в тексте цели (NNAM после QOBJ) есть кириллица, создаётся копия записи, где эти поля заменены английским текстом
(по ID строки из записи мастера: Strings\\<мастер>_en.STRINGS в архивах игры). Остальные байты записи — без изменений.
Чтобы FormID внутри записи не надо было пересчитывать, у заплатки тот же список мастеров, что у плагина-победителя,
плюс он сам — поэтому заплаток несколько, по одной на победителя (ESL, грузятся последними).

    python tools/gen_screenshot_en_patch.py <Data> <loadorder.txt> <plugins.txt> <папка мода MO2>
"""

import os
import re
import struct
import sys
import zlib

OFFICIAL = ['Fallout4.esm', 'DLCRobot.esm', 'DLCworkshop01.esm', 'DLCCoast.esm', 'DLCworkshop02.esm',
            'DLCworkshop03.esm', 'DLCNukaWorld.esm']
BA2_OF = {'fallout4.esm': 'Fallout4 - Interface.ba2', 'dlcrobot.esm': 'DLCRobot - Main.ba2',
          'dlcworkshop01.esm': 'DLCworkshop01 - Main.ba2', 'dlccoast.esm': 'DLCCoast - Main.ba2',
          'dlcworkshop02.esm': 'DLCworkshop02 - Main.ba2', 'dlcworkshop03.esm': 'DLCworkshop03 - Main.ba2',
          'dlcnukaworld.esm': 'DLCNukaWorld - Main.ba2'}
COMPRESSED = 0x00040000
LOCALIZED = 0x80
ESL = 0x200
CYRILLIC = re.compile('[Ѐ-ӿ]')
REC = struct.Struct('<4sIIIIHH')
GRUP = struct.Struct('<4sIIIQ')


# ------------------------------------------------------------------ plugins

class Plugin:
    def __init__(self, path):
        self.name = os.path.basename(path)
        self.buf = open(path, 'rb').read()
        sig, size, self.flags, _, _, _, _ = REC.unpack_from(self.buf, 0)
        assert sig == b'TES4', path
        self.masters = [p.rstrip(b'\0').decode('utf-8', 'replace') for t, p in subrecords(self.buf[24:24 + size])
                        if t == b'MAST']
        self.groups = []
        pos = 24 + size
        while pos + 24 <= len(self.buf):
            _, gsize, label, gtype, _ = GRUP.unpack_from(self.buf, pos)
            if gtype == 0:
                self.groups.append((label, pos + 24, pos + gsize))
            pos += gsize

    def quests(self):
        """(header fields, data) of every QUST record, data decompressed."""
        want = struct.unpack('<I', b'QUST')[0]
        for label, start, end in self.groups:
            if label == want:
                yield from self._walk(start, end)

    def _walk(self, pos, end):
        while pos + 24 <= end:
            sig, size, flags, form_id, rev, ver, unk = REC.unpack_from(self.buf, pos)
            if sig == b'GRUP':
                yield from self._walk(pos + 24, pos + size)
                pos += size
                continue
            data = self.buf[pos + 24:pos + 24 + size]
            if sig == b'QUST':
                if flags & COMPRESSED:
                    data = zlib.decompress(data[4:])
                yield (flags & ~COMPRESSED, form_id, rev, ver, unk), data
            pos += 24 + size

    def owner(self, form_id):
        """Plugin file that defines form_id (as written in this plugin)."""
        idx = form_id >> 24
        return self.masters[idx] if idx < len(self.masters) else self.name


def subrecords(data):
    pos, pending = 0, None
    while pos + 6 <= len(data):
        tag, size = struct.unpack_from('<4sH', data, pos)
        pos += 6
        if tag == b'XXXX':
            pending = struct.unpack_from('<I', data, pos)[0]
            pos += size
            continue
        if pending is not None:
            size, pending = pending, None
        yield tag, data[pos:pos + size]
        pos += size


def pack_sub(tag, payload):
    if len(payload) > 0xFFFF:
        return struct.pack('<4sHI', b'XXXX', 4, len(payload)) + struct.pack('<4sH', tag, 0) + payload
    return struct.pack('<4sH', tag, len(payload)) + payload


def text_ids(data):
    """FULL string id and {objective index: NNAM string id} of a master (localized) QUST record."""
    full, objectives, current = None, {}, None
    for tag, payload in subrecords(data):
        if tag == b'FULL' and full is None and len(payload) == 4:
            full = struct.unpack('<I', payload)[0]
        elif tag == b'QOBJ':
            current = struct.unpack('<H', payload[:2])[0]
        elif tag in (b'ANAM', b'ALST', b'ALLS', b'ALCS'):
            current = None
        elif tag == b'NNAM' and current is not None and len(payload) == 4:
            objectives[current] = struct.unpack('<I', payload)[0]
    return full, objectives


def has_cyrillic(payload):
    return bool(CYRILLIC.search(payload.rstrip(b'\0').decode('utf-8', 'replace')) or
                CYRILLIC.search(payload.rstrip(b'\0').decode('cp1251', 'replace')) and any(b >= 0xC0 for b in payload))


# ------------------------------------------------------------------ strings from BA2

def ba2_file(path, name):
    with open(path, 'rb') as f:
        magic, version, kind, count, names_offset = struct.unpack('<4sI4sIQ', f.read(24))
        assert magic == b'BTDX' and kind == b'GNRL', path
        entries = [struct.unpack('<I4sIIQIII', f.read(36)) for _ in range(count)]
        f.seek(names_offset)
        rest = f.read()
        pos = 0
        for e in entries:
            (n,) = struct.unpack_from('<H', rest, pos)
            fname = rest[pos + 2:pos + 2 + n].decode('utf-8', 'replace')
            pos += 2 + n
            if fname.lower() == name.lower():
                _, _, _, _, offset, packed, unpacked, _ = e
                f.seek(offset)
                raw = f.read(packed or unpacked)
                return zlib.decompress(raw) if packed else raw
    raise KeyError('%s not in %s' % (name, path))


def load_strings(data_dir, master):
    """{id: raw bytes} of Strings\\<master>_en.STRINGS (zero-terminated strings)."""
    stem = os.path.splitext(master)[0]
    blob = ba2_file(os.path.join(data_dir, BA2_OF[master.lower()]), 'Strings\\%s_en.STRINGS' % stem)
    count, size = struct.unpack_from('<II', blob, 0)
    base = 8 + count * 8
    out = {}
    for i in range(count):
        sid, off = struct.unpack_from('<II', blob, 8 + i * 8)
        start = base + off
        out[sid] = blob[start:blob.index(b'\0', start)]
    return out


# ------------------------------------------------------------------ patch

def patched(data, english_full, english_objectives):
    """Record data with FULL / objective NNAM replaced; None if nothing changed."""
    out, current, changed = bytearray(), None, False
    for tag, payload in subrecords(data):
        if tag == b'QOBJ':
            current = struct.unpack('<H', payload[:2])[0]
        elif tag in (b'ANAM', b'ALST', b'ALLS', b'ALCS'):
            current = None
        if tag == b'FULL' and english_full is not None and has_cyrillic(payload):
            payload, changed = english_full + b'\0', True
        elif tag == b'NNAM' and current in english_objectives and has_cyrillic(payload):
            payload, changed = english_objectives[current] + b'\0', True
        out += pack_sub(tag, payload)
    return bytes(out) if changed else None


def write_plugin(path, masters, records):
    body = bytearray()
    for (flags, form_id, rev, ver, unk), data in records:
        body += REC.pack(b'QUST', len(data), flags, form_id, rev, ver, unk) + data
    group = GRUP.pack(b'GRUP', 24 + len(body), struct.unpack('<I', b'QUST')[0], 0, 0) + body
    hdr = pack_sub(b'HEDR', struct.pack('<fII', 1.0, len(records), 0x800))
    hdr += pack_sub(b'CNAM', b'Nearest Quest Tracker screenshot patch\0')
    for m in masters:
        hdr += pack_sub(b'MAST', m.encode('utf-8') + b'\0') + pack_sub(b'DATA', bytes(8))
    tes4 = REC.pack(b'TES4', len(hdr), ESL, 0, 0, 131, 0) + hdr
    with open(path, 'wb') as f:
        f.write(tes4 + group)


def main():
    data_dir, loadorder, plugins_txt, out_dir = sys.argv[1:5]
    enabled = {l.strip()[1:].lower() for l in open(plugins_txt, encoding='utf-8') if l.startswith('*')}
    order = [l.strip() for l in open(loadorder, encoding='utf-8') if l.strip() and not l.startswith('#')]
    order = [p for p in order if p in OFFICIAL or p.lower() in enabled]

    masters_text = {}          # (master lower, local id) -> (full id, {objective: id})
    winners = {}               # (master lower, local id) -> (plugin, header, data)
    for name in order:
        path = os.path.join(data_dir, name)
        if not os.path.exists(path):
            continue
        plugin = Plugin(path)
        for header, data in plugin.quests():
            owner = plugin.owner(header[1]).lower()
            if owner not in BA2_OF:
                continue
            key = (owner, header[1] & 0xFFFFFF)
            if plugin.name.lower() == owner:
                masters_text[key] = text_ids(data)
            winners[key] = (plugin, header, data)

    strings = {}
    groups = {}                # winner plugin name -> (plugin, [records])
    replaced = 0
    for key, (plugin, header, data) in winners.items():
        if plugin.name.lower() == key[0] or plugin.flags & LOCALIZED or key not in masters_text:
            continue
        if key[0] not in strings:
            strings[key[0]] = load_strings(data_dir, next(m for m in OFFICIAL if m.lower() == key[0]))
        table = strings[key[0]]
        full_id, objective_ids = masters_text[key]
        english_full = table.get(full_id) if full_id else None
        english_objectives = {i: table[s] for i, s in objective_ids.items() if s in table}
        new = patched(data, english_full, english_objectives)
        if new is not None:
            groups.setdefault(plugin.name, (plugin, []))[1].append((header, new))
            replaced += 1

    os.makedirs(out_dir, exist_ok=True)
    names = []
    for winner, (plugin, records) in sorted(groups.items()):
        out_name = 'NQTScreenshotEN - %s.esp' % os.path.splitext(winner)[0]
        write_plugin(os.path.join(out_dir, out_name), plugin.masters + [plugin.name], records)
        names.append(out_name)
        print('%s: %d quests (masters: %s)' % (out_name, len(records), ', '.join(plugin.masters + [plugin.name])))
    print('quests patched:', replaced)
    open(os.path.join(out_dir, 'plugins.list'), 'w', encoding='utf-8').write('\n'.join(names) + '\n')


if __name__ == '__main__':
    main()
