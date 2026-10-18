"""Which plugins in a Data folder contain a WRLD record for Fallout4.esm 0x3C, and its FULL (string id if localized)."""
import os, struct, sys, zlib

DATA = sys.argv[1]
TARGET = 0x3C


def records(buf, off, end):
    while off < end:
        typ = buf[off:off + 4]
        size, = struct.unpack_from('<I', buf, off + 4)
        if typ == b'GRUP':
            yield from records(buf, off + 24, off + size)
            off += size
        else:
            flags, fid = struct.unpack_from('<II', buf, off + 8)
            yield typ, flags, fid, buf[off + 24:off + 24 + size]
            off += 24 + size


def subrecords(data):
    off = 0
    while off + 6 <= len(data):
        typ = data[off:off + 4]
        size, = struct.unpack_from('<H', data, off + 4)
        yield typ, data[off + 6:off + 6 + size]
        off += 6 + size


for name in sorted(os.listdir(DATA)):
    if not name.lower().endswith(('.esm', '.esl', '.esp')):
        continue
    buf = open(os.path.join(DATA, name), 'rb').read()
    hsize, = struct.unpack_from('<I', buf, 4)
    hflags, = struct.unpack_from('<I', buf, 8)
    masters = [d.rstrip(b'\0').decode('latin1') for t, d in subrecords(buf[24:24 + hsize]) if t == b'MAST']
    if 'Fallout4.esm' not in masters and name != 'Fallout4.esm':
        continue
    want = (masters.index('Fallout4.esm') << 24 | TARGET) if name != 'Fallout4.esm' else TARGET
    # top-level WRLD group only (fast path)
    off = 24 + hsize
    while off < len(buf):
        size, = struct.unpack_from('<I', buf, off + 4)
        if buf[off + 8:off + 12] == b'WRLD':
            for typ, flags, fid, data in records(buf, off + 24, off + size):
                if typ == b'WRLD' and fid == want:
                    if flags & 0x40000:
                        data = zlib.decompress(data[4:])
                    full = [d for t, d in subrecords(data) if t == b'FULL']
                    val = (('id %08X' % struct.unpack('<I', full[0])[0]) if (hflags & 0x80) and full and len(full[0]) == 4
                           else repr(full[0]) if full else 'no FULL')
                    print('%-55s localized=%d  FULL %s' % (name, bool(hflags & 0x80), val))
            break
        off += size
