"""Does a plugin's <name>_<lang>.STRINGS (inside a GNRL BA2) contain a string id? Prints the text if yes."""
import struct, sys, zlib

ba2, want_name, sid = sys.argv[1], sys.argv[2].lower(), int(sys.argv[3], 16)
b = open(ba2, 'rb').read()
magic, ver, typ, count, nameoff = struct.unpack_from('<4sI4sIQ', b, 0)
hdr = 24  # FO4 v1/v7/v8
o, names = nameoff, []
for i in range(count):
    n, = struct.unpack_from('<H', b, o)
    names.append(b[o + 2:o + 2 + n].decode('latin1'))
    o += 2 + n
for i, name in enumerate(names):
    if name.lower().endswith(want_name):
        _, _, _, _, off, packed, size, _ = struct.unpack_from('<I4sIIQIII', b, hdr + 36 * i)
        data = zlib.decompress(b[off:off + packed]) if packed else b[off:off + size]
        cnt, dsize = struct.unpack_from('<II', data, 0)
        base = 8 + 8 * cnt
        table = dict(struct.unpack_from('<II', data, 8 + 8 * k) for k in range(cnt))
        print(name, 'entries', cnt, 'has %X:' % sid, sid in table)
        if sid in table:
            s = data[base + table[sid]:data.index(b'\0', base + table[sid])]
            print('  text:', s.decode('utf-8', 'replace'))
