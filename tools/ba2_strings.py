"""
String tables of localized plugins (FULL and other lstring fields hold a string id) from the game's BA2 archives.

    from ba2_strings import StringTables
    st = StringTables(r'D:\\Games\\F4_AE\\Data')
    st.text('ccOTMFO4001-Remnants.esl', 0x1234, 'ru')   # -> str or None

Looks for Strings\\<plugin stem>_<lang>.STRINGS (then .DLSTRINGS, .ILSTRINGS) in every general (GNRL) BA2 of the
Data folder and in loose Data\\Strings. BA2 versions 1, 7 and 8 (Fallout 4 OG/NG/AE) share the 24-byte header and
36-byte file records. A string file: count u32, data size u32, count x (id u32, offset u32), then the strings
(.STRINGS: zero-terminated; .DLSTRINGS/.ILSTRINGS: u32 length + text). Text is UTF-8 (1.10.163 and later).

python tools/ba2_strings.py <Data> <plugin> <hex id> [lang]
"""

import os
import struct
import sys
import zlib


class StringTables:
    def __init__(self, data_dir):
        self.data_dir = data_dir
        self._index = None    # lower-case archive path -> (ba2 file, record offset)
        self._tables = {}     # (plugin lower, lang) -> {id: text}

    def _build_index(self):
        self._index = {}
        for name in sorted(os.listdir(self.data_dir)):
            if not name.lower().endswith('.ba2'):
                continue
            path = os.path.join(self.data_dir, name)
            with open(path, 'rb') as f:
                magic, version, kind, count, names_at = struct.unpack('<4sI4sIQ', f.read(24))
                if magic != b'BTDX' or kind != b'GNRL':
                    continue
                f.seek(names_at)
                names = f.read()
            pos = 0
            for i in range(count):
                n, = struct.unpack_from('<H', names, pos)
                entry = names[pos + 2:pos + 2 + n].decode('latin1').replace('/', '\\').lower()
                pos += 2 + n
                if entry.startswith('strings\\'):
                    self._index.setdefault(entry, (path, 24 + 36 * i))

    def _read(self, rel):
        loose = os.path.join(self.data_dir, rel)
        if os.path.exists(loose):
            with open(loose, 'rb') as f:
                return f.read()
        if self._index is None:
            self._build_index()
        hit = self._index.get(rel.lower())
        if not hit:
            return None
        path, rec = hit
        with open(path, 'rb') as f:
            f.seek(rec)
            _, _, _, _, offset, packed, size, _ = struct.unpack('<I4sIIQIII', f.read(36))
            f.seek(offset)
            raw = f.read(packed or size)
        return zlib.decompress(raw) if packed else raw

    def table(self, plugin, lang):
        key = (plugin.lower(), lang)
        if key not in self._tables:
            stem = os.path.splitext(plugin)[0]
            out = {}
            for ext, counted in (('STRINGS', False), ('DLSTRINGS', True), ('ILSTRINGS', True)):
                data = self._read('Strings\\%s_%s.%s' % (stem, lang, ext))
                if not data:
                    continue
                count, _ = struct.unpack_from('<II', data, 0)
                base = 8 + 8 * count
                for k in range(count):
                    sid, off = struct.unpack_from('<II', data, 8 + 8 * k)
                    p = base + off
                    if counted:
                        n, = struct.unpack_from('<I', data, p)
                        text = data[p + 4:p + 4 + n].rstrip(b'\0')
                    else:
                        text = data[p:data.index(b'\0', p)]
                    out.setdefault(sid, text.decode('utf-8', 'replace'))
            self._tables[key] = out
        return self._tables[key]

    def text(self, plugin, sid, lang='en'):
        return self.table(plugin, lang).get(sid)


if __name__ == '__main__':
    st = StringTables(sys.argv[1])
    print(st.text(sys.argv[2], int(sys.argv[3], 16), sys.argv[4] if len(sys.argv) > 4 else 'en'))
