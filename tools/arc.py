#!/usr/bin/env python3
"""
ARC archive extract/repack for MZZXLC (MT Framework v7).

Format:
  magic "ARC\0", u16 version (7), u16 file count,
  then 80-byte entries:
    name[64]     null-terminated path, backslash separated, no extension
    u32 ext_hash jamcrc of the extension (.tex/.gmd/...)
    u32 comp_size
    u32 raw3     low 29 bits = original size, high 3 bits = flags (2 = zlib)
    u32 offset
  First data starts at 0x8000 alignment; files are packed back to back.

Usage:
  python arc.py extract in.arc outdir     # writes _arcmeta.json alongside
  python arc.py repack  outdir out.arc    # reads _arcmeta.json
"""
import sys, os, struct, zlib, json

try:
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')
except Exception:
    pass

ENTRY = 80
HEADER = 8
FIRST_DATA_ALIGN = 0x8000
SEP = chr(92)  # backslash


def extract(arc_path, outdir):
    d = open(arc_path, 'rb').read()
    assert d[0:4] == b'ARC\x00', "not an ARC file"
    version, count = struct.unpack('<HH', d[4:8])
    os.makedirs(outdir, exist_ok=True)
    meta = {"version": version, "first_data_align": FIRST_DATA_ALIGN, "files": []}
    off = HEADER
    for i in range(count):
        e = d[off:off+ENTRY]; off += ENTRY
        name = e[0:64].split(b'\x00')[0].decode('latin1')
        ext_hash, comp, raw3, foff = struct.unpack('<IIII', e[64:80])
        uncomp = raw3 & 0x1FFFFFFF
        flags = raw3 >> 29
        blob = d[foff:foff+comp]
        if flags == 2:
            data = zlib.decompress(blob)
        else:
            data = blob
        assert len(data) == uncomp, f"size mismatch {name}: {len(data)} != {uncomp}"
        safe = name.replace(SEP, '__') + ".bin"
        open(os.path.join(outdir, safe), 'wb').write(data)
        meta["files"].append({
            "name": name, "ext_hash": ext_hash, "flags": flags,
            "uncomp": uncomp, "disk": safe,
        })
        print(f"[{i}] {name}  ({len(data)} bytes, flags={flags})")
    json.dump(meta, open(os.path.join(outdir, "_arcmeta.json"), 'w'), indent=2)
    print(f"-> extracted {count} files + _arcmeta.json")


def repack(srcdir, out_path):
    meta = json.load(open(os.path.join(srcdir, "_arcmeta.json")))
    files = meta["files"]
    count = len(files)
    align = meta.get("first_data_align", FIRST_DATA_ALIGN)

    blobs = []
    for f in files:
        raw = open(os.path.join(srcdir, f["disk"]), 'rb').read()
        if f["flags"] == 2:
            comp = zlib.compress(raw, 9)
        else:
            comp = raw
        blobs.append((f, raw, comp))

    data_start = align
    cur = data_start
    offsets = []
    for f, raw, comp in blobs:
        offsets.append(cur)
        cur += len(comp)
    total = cur

    out = bytearray(total)
    struct.pack_into('<4sHH', out, 0, b'ARC\x00', meta["version"], count)
    eoff = HEADER
    for (f, raw, comp), foff in zip(blobs, offsets):
        name_b = f["name"].encode('latin1')[:63]
        struct.pack_into('<64s', out, eoff, name_b)
        raw3 = (len(raw) & 0x1FFFFFFF) | (f["flags"] << 29)
        struct.pack_into('<IIII', out, eoff+64, f["ext_hash"], len(comp), raw3, foff)
        eoff += ENTRY
    for (f, raw, comp), foff in zip(blobs, offsets):
        out[foff:foff+len(comp)] = comp

    open(out_path, 'wb').write(out)
    print(f"-> repacked {count} files into {out_path} ({len(out)} bytes)")


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'extract' and len(sys.argv) == 4:
        extract(sys.argv[2], sys.argv[3])
    elif cmd == 'repack' and len(sys.argv) == 4:
        repack(sys.argv[2], sys.argv[3])
    else:
        print(__doc__); sys.exit(1)
