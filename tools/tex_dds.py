#!/usr/bin/env python3
"""
TEX <-> DDS/PNG converter for MZZXLC button textures.

TEX format: magic "TEX\0", version 0xA2 (late MT Framework),
24-byte header, then raw BC3 blocks. Button atlas is 512x512, mip 1, DXT5.
The original TEX header is preserved and reattached when converting back.

Raw conversion (lossless, for backup/verification):
  python tex_dds.py tex2dds in.tex out.dds
  python tex_dds.py dds2tex edited.dds orig.tex out.tex

Editable conversion:
  python tex_dds.py tex2png in.tex out.png
  python tex_dds.py png2tex edited.png orig.tex out.tex

Channel layout: only G and A carry information. R/B are constant 123 and
ignored by the game, which renders these as greyscale + alpha (grey keycap,
white glyph). tex2png expands G into grey RGB and keeps A, so the PNG looks
the way the game draws it; png2tex folds luminance back into G and alpha into A.
"""
import sys, struct, os

try:
    sys.stdout.reconfigure(encoding='utf-8')
    sys.stderr.reconfigure(encoding='utf-8')
except Exception:
    pass

# dword@16 holds the data offset and is always 0x18 here.
TEX_HEADER_SIZE = 24


def make_dds_header(width, height, fourcc=b'DXT5', mipcount=1):
    # DDS_HEADER (124 bytes) + magic "DDS " (4) = 128
    DDSD_CAPS = 0x1
    DDSD_HEIGHT = 0x2
    DDSD_WIDTH = 0x4
    DDSD_PIXELFORMAT = 0x1000
    DDSD_LINEARSIZE = 0x80000
    DDSD_MIPMAPCOUNT = 0x20000
    flags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT | DDSD_LINEARSIZE
    if mipcount > 1:
        flags |= DDSD_MIPMAPCOUNT

    # BC3: 16 bytes per 4x4 block
    block_bytes = 16
    pitch = max(1, (width + 3) // 4) * max(1, (height + 3) // 4) * block_bytes

    DDPF_FOURCC = 0x4
    DDSCAPS_TEXTURE = 0x1000

    h = bytearray(128)
    struct.pack_into('<4s', h, 0, b'DDS ')
    struct.pack_into('<I', h, 4, 124)            # dwSize
    struct.pack_into('<I', h, 8, flags)          # dwFlags
    struct.pack_into('<I', h, 12, height)        # dwHeight
    struct.pack_into('<I', h, 16, width)         # dwWidth
    struct.pack_into('<I', h, 20, pitch)         # dwPitchOrLinearSize
    struct.pack_into('<I', h, 24, 0)             # dwDepth
    struct.pack_into('<I', h, 28, mipcount)      # dwMipMapCount
    # pixel format @ 76
    struct.pack_into('<I', h, 76, 32)            # dwSize of pixelformat
    struct.pack_into('<I', h, 80, DDPF_FOURCC)   # dwFlags
    struct.pack_into('<4s', h, 84, fourcc)       # dwFourCC
    # caps @ 108
    struct.pack_into('<I', h, 108, DDSCAPS_TEXTURE)
    return bytes(h)


def parse_tex(data):
    assert data[0:4] == b'TEX\x00', "not a TEX file"
    dw2 = struct.unpack('<I', data[8:12])[0]
    mip = dw2 & 0x3F
    width = (dw2 >> 6) & 0x1FFF
    height = (dw2 >> 19) & 0x1FFF
    return width, height, mip


def to_dds(tex_path, dds_path):
    data = open(tex_path, 'rb').read()
    w, h, mip = parse_tex(data)
    print(f"TEX: {w}x{h} mip={mip} datasize={len(data)-TEX_HEADER_SIZE}")
    payload = data[TEX_HEADER_SIZE:]
    dds = make_dds_header(w, h, b'DXT5', max(1, mip)) + payload
    open(dds_path, 'wb').write(dds)
    print(f"-> wrote {dds_path} ({len(dds)} bytes)")


def to_tex(dds_path, orig_tex_path, out_tex_path):
    dds = open(dds_path, 'rb').read()
    assert dds[0:4] == b'DDS ', "not a DDS file"
    orig = open(orig_tex_path, 'rb').read()
    tex_header = orig[:TEX_HEADER_SIZE]
    payload = dds[128:]
    out = tex_header + payload
    open(out_tex_path, 'wb').write(out)
    print(f"-> wrote {out_tex_path} ({len(out)} bytes)  "
          f"[orig payload {len(orig)-TEX_HEADER_SIZE}, new payload {len(payload)}]")


# Constant the game ignores; kept so output matches the original bytes.
RB_CONST = 123


def tex_to_png(tex_path, png_path):
    """TEX -> editable PNG, expanded to what the game actually draws."""
    from PIL import Image
    data = open(tex_path, 'rb').read()
    w, h, mip = parse_tex(data)
    dds = make_dds_header(w, h, b'DXT5', max(1, mip)) + data[TEX_HEADER_SIZE:]
    tmp = png_path + '.tmp.dds'
    open(tmp, 'wb').write(dds)
    im = Image.open(tmp).convert('RGBA')
    os.remove(tmp)
    r, g, b, a = im.split()
    out = Image.merge('RGBA', (g, g, g, a))
    out.save(png_path)
    print(f"-> wrote {png_path} ({w}x{h}) [G->gray, A->alpha]")


def png_to_tex(png_path, orig_tex_path, out_tex_path):
    """Editable PNG -> TEX: luminance back into G, alpha into A, encoded as DXT5."""
    from PIL import Image
    orig = open(orig_tex_path, 'rb').read()
    w, h, mip = parse_tex(orig)
    im = Image.open(png_path).convert('RGBA')
    if im.size != (w, h):
        print(f"   (resize {im.size} -> {(w, h)})")
        im = im.resize((w, h), Image.NEAREST)
    pr, pg, pb, pa = im.split()
    lum = im.convert('L')
    R = Image.new('L', (w, h), RB_CONST)
    B = Image.new('L', (w, h), RB_CONST)
    G = lum
    A = pa
    rebuilt = Image.merge('RGBA', (R, G, B, A))
    tmp = out_tex_path + '.tmp.dds'
    rebuilt.save(tmp, pixel_format='DXT5')
    dds = open(tmp, 'rb').read()
    os.remove(tmp)
    payload = dds[128:]
    tex = orig[:TEX_HEADER_SIZE] + payload
    open(out_tex_path, 'wb').write(tex)
    print(f"-> wrote {out_tex_path} ({len(tex)} bytes) "
          f"[lum->G, alpha->A, R/B={RB_CONST}]")


if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'tex2dds' and len(sys.argv) == 4:
        to_dds(sys.argv[2], sys.argv[3])
    elif cmd == 'dds2tex' and len(sys.argv) == 5:
        to_tex(sys.argv[2], sys.argv[3], sys.argv[4])
    elif cmd == 'tex2png' and len(sys.argv) == 4:
        tex_to_png(sys.argv[2], sys.argv[3])
    elif cmd == 'png2tex' and len(sys.argv) == 5:
        png_to_tex(sys.argv[2], sys.argv[3], sys.argv[4])
    else:
        print(__doc__)
        sys.exit(1)
