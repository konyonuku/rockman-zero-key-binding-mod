#!/usr/bin/env python3
"""Bake the keycap image library the mod DLL draws button icons from.

Reads the game's button atlas (btn_icon_pcb_00_ID_HQ.tex), reuses the artwork
already there for the special keys, and renders every other key onto a blank
keycap extracted from that same atlas. Output is a raw G+A blob the DLL can
copy into atlas cells, plus a JSON manifest and a preview sheet.

  python tools/make_keycaps.py <atlas.tex> <outdir>

Only the mod author runs this; end users get the baked output inside the mod.
"""
import json, os, struct, sys, tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import tex_dds

from PIL import Image, ImageChops, ImageDraw, ImageFont

CELL = 64
ART_OFF, ART = 10, 44          # normal keycap art rect inside a cell
CAP_H, BASELINE = 18, 29       # single-glyph metrics inside that 44x44 art

# Space-shaped cap, reused as the base for the modifier keys.
SPACE_CAP = (10, 18, 54, 46)   # art rect inside the cell (44 x 28)
SPACE_FACE = (13, 21, 51, 43)  # inner face, excluding the outline ring
SPACE_TEXT = (16, 24, 48, 40)  # box the original "Space" lettering sits in
MOD_CAP_W = 60                 # modifier cap width; 44 leaves Space untouched
MOD_LABEL_H = 12

FONT = r"C:\Windows\Fonts\NotoSansJP-VF.ttf"   # Noto Sans JP (OFL), Latin subset
FONT_WEIGHT = 700

# Cells whose original artwork is reused verbatim.
REUSED = ["ESC", "SPACE", "TAB", "ENTER", "UP", "DOWN", "LEFT", "RIGHT"]

# DirectInput scancodes (DIK_*) - the ids the input hook works in.
DIK = {
    "ESC": 0x01, "N1": 0x02, "N2": 0x03, "N3": 0x04, "N4": 0x05, "N5": 0x06,
    "N6": 0x07, "N7": 0x08, "N8": 0x09, "N9": 0x0A, "N0": 0x0B,
    "MINUS": 0x0C, "EQUALS": 0x0D, "TAB": 0x0F,
    "Q": 0x10, "W": 0x11, "E": 0x12, "R": 0x13, "T": 0x14, "Y": 0x15,
    "U": 0x16, "I": 0x17, "O": 0x18, "P": 0x19,
    "LBRACKET": 0x1A, "RBRACKET": 0x1B, "ENTER": 0x1C, "LCTRL": 0x1D,
    "A": 0x1E, "S": 0x1F, "D": 0x20, "F": 0x21, "G": 0x22, "H": 0x23,
    "J": 0x24, "K": 0x25, "L": 0x26,
    "SEMICOLON": 0x27, "APOSTROPHE": 0x28, "GRAVE": 0x29,
    "LSHIFT": 0x2A, "BACKSLASH": 0x2B,
    "Z": 0x2C, "X": 0x2D, "C": 0x2E, "V": 0x2F, "B": 0x30, "N": 0x31,
    "M": 0x32, "COMMA": 0x33, "PERIOD": 0x34, "SLASH": 0x35,
    "RSHIFT": 0x36, "LALT": 0x38, "SPACE": 0x39, "CAPSLOCK": 0x3A,
    "RCTRL": 0x9D, "RALT": 0xB8,
    "UP": 0xC8, "LEFT": 0xCB, "RIGHT": 0xCD, "DOWN": 0xD0,
}

# Keys drawn as a single glyph on the normal keycap.
GLYPH_KEYS = {k: k for k in "ABCDEFGHIJKLMNOPQRSTUVWXYZ"}
GLYPH_KEYS.update({"N%d" % d: str(d) for d in range(10)})
GLYPH_KEYS.update({
    "GRAVE": "`", "MINUS": "-", "EQUALS": "=", "LBRACKET": "[",
    "RBRACKET": "]", "BACKSLASH": "\\", "SEMICOLON": ";",
    "APOSTROPHE": "'", "COMMA": ",", "PERIOD": ".", "SLASH": "/",
})

# Keys drawn as a word on the Space-shaped cap.
LABEL_KEYS = {
    "LSHIFT": "LSHIFT", "RSHIFT": "RSHIFT", "LCTRL": "LCTRL",
    "RCTRL": "RCTRL", "LALT": "LALT", "RALT": "RALT", "CAPSLOCK": "CAPS",
}

# Hand-tuned vertical trim, in pixels, positive moves the lettering up.
# Font metrics alone put some marks too high or too low on the cap face.
NUDGE = {
    "LBRACKET": 3, "RBRACKET": 3, "SEMICOLON": 3, "COMMA": 3,
    "SLASH": 3, "BACKSLASH": 3, "APOSTROPHE": -3, "GRAVE": -4,
    "LSHIFT": 2, "RSHIFT": 2, "LCTRL": 2, "RCTRL": 2,
    "LALT": 2, "RALT": 2, "CAPSLOCK": 2,
}


def load_atlas(tex_path):
    tmp = os.path.join(tempfile.gettempdir(), "_keycap_atlas.png")
    tex_dds.tex_to_png(tex_path, tmp)
    im = Image.open(tmp).convert("RGBA")
    im.load()
    os.remove(tmp)
    return im


def cell_of(atlas, slots, label):
    c = slots["cells"][label]
    x, y = c["col"] * CELL, c["row"] * CELL
    return atlas.crop((x, y, x + CELL, y + CELL))


def blank_keycap(atlas, slots):
    """Min over every single-glyph cell: the lettering differs, the cap does not."""
    mn_a = mn_g = None
    for c in slots["cells"].values():
        if c["kind"] != "single":
            continue
        x, y = c["col"] * CELL + ART_OFF, c["row"] * CELL + ART_OFF
        _, g, _, a = atlas.crop((x, y, x + ART, y + ART)).split()
        mn_a = a if mn_a is None else ImageChops.darker(mn_a, a)
        mn_g = g if mn_g is None else ImageChops.darker(mn_g, g)
    return Image.merge("RGBA", (mn_g, mn_g, mn_g, mn_a))


def blank_space_cap(atlas, slots):
    """Erase the word "Space": refill each row with the median of its clean pixels."""
    cell = cell_of(atlas, slots, "SPACE")
    g, a = cell.split()[1], cell.split()[3].copy()
    px = a.load()
    for y in range(SPACE_TEXT[1], SPACE_TEXT[3]):
        clean = sorted(px[x, y] for x in range(SPACE_FACE[0], SPACE_FACE[2])
                       if not SPACE_TEXT[0] <= x < SPACE_TEXT[2])
        if clean:
            med = clean[len(clean) // 2]
            for x in range(SPACE_TEXT[0], SPACE_TEXT[2]):
                px[x, y] = med
    return Image.merge("RGBA", (g, g, g, a))


def widen_cap(cap_cell, new_w):
    """Stretch the Space cap horizontally, leaving its rounded ends untouched."""
    cap = cap_cell.crop(SPACE_CAP)
    w, h = cap.size
    if new_w == w:
        return cap_cell
    edge = 12
    mid = cap.crop((edge, 0, w - edge, h)).resize((new_w - 2 * edge, h), Image.BILINEAR)
    out = Image.new("RGBA", (new_w, h), (0, 0, 0, 0))
    out.paste(cap.crop((0, 0, edge, h)), (0, 0))
    out.paste(mid, (edge, 0))
    out.paste(cap.crop((w - edge, 0, w, h)), (new_w - edge, 0))
    cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    cell.paste(out, ((CELL - new_w) // 2, SPACE_CAP[1]))
    return cell


def font_at(size):
    f = ImageFont.truetype(FONT, size)
    try:
        f.set_variation_by_axes([FONT_WEIGHT])
    except Exception:
        pass
    return f


def ink_box(size, text, thr=40):
    p = Image.new("L", (size * 14 + 240, size * 8 + 120), 0)
    ImageDraw.Draw(p).text((size + 30, size + 30), text, fill=255, font=font_at(size))
    return p.point(lambda v: 255 if v > thr else 0).getbbox()


def size_for_cap_height(target):
    """Smallest point size reaching the target cap height (sizes skip values)."""
    for size in range(6, 60):
        bb = ink_box(size, "H", thr=96)
        if bb[3] - bb[1] >= target:
            return size
    return 24


def size_for_box(text, max_w, max_h):
    """Largest point size whose ink still fits the label box."""
    best = 6
    for size in range(6, 40):
        bb = ink_box(size, text)
        if bb[2] - bb[0] <= max_w and bb[3] - bb[1] <= max_h:
            best = size
        else:
            break
    return best


def paint(base, mask):
    """Raise alpha to opaque where the mask is set; G already reads 255 there."""
    g, a = base.split()[1], base.split()[3].copy()
    pa, pm = a.load(), mask.load()
    w, h = a.size
    for y in range(h):
        for x in range(w):
            m = pm[x, y]
            if m:
                pa[x, y] += (255 - pa[x, y]) * m // 255
    return Image.merge("RGBA", (g, g, g, a))


def glyph_mask(text, font, box, cx, baseline=None, cy=None):
    """Rasterise text into a box-sized mask, centred on cx, sitting on baseline."""
    w, h = box
    tmp = Image.new("L", (w * 4 + 80, h * 4 + 80), 0)
    ImageDraw.Draw(tmp).text((w + 40, h + 40), text, fill=255, font=font)
    bb = tmp.point(lambda v: 255 if v > 40 else 0).getbbox()
    m = Image.new("L", box, 0)
    if not bb:
        return m
    ink = tmp.crop(bb)
    x = round(cx - ink.size[0] / 2)
    if baseline is not None:
        probe = Image.new("L", (400, 400), 0)
        ImageDraw.Draw(probe).text((100, 100), "H", fill=255, font=font)
        pb = probe.point(lambda v: 255 if v > 96 else 0).getbbox()
        base_dy = pb[3] - 100                     # draw origin -> baseline
        y = baseline + (bb[1] - (h + 40 + base_dy))
    else:
        y = round(cy - ink.size[1] / 2)
    m.paste(ink, (x, y), ink)
    return m


def build(tex_path, outdir):
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    slots = json.load(open(os.path.join(root, "data", "slots.json"), encoding="utf-8"))
    atlas = load_atlas(tex_path)
    os.makedirs(outdir, exist_ok=True)

    blank_art = blank_keycap(atlas, slots)
    mod_cap = widen_cap(blank_space_cap(atlas, slots), MOD_CAP_W)
    big = font_at(size_for_cap_height(CAP_H))

    tiles = {}
    for key in REUSED:
        tiles[key] = (cell_of(atlas, slots, key), "reused")

    for key, ch in GLYPH_KEYS.items():
        mask = glyph_mask(ch, big, (ART, ART), ART / 2,
                          baseline=BASELINE - NUDGE.get(key, 0))
        cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
        cell.paste(paint(blank_art, mask), (ART_OFF, ART_OFF))
        tiles[key] = (cell, "glyph '%s'" % ch)

    label_cy = (SPACE_CAP[1] + SPACE_CAP[3]) / 2 + 1
    for key, text in LABEL_KEYS.items():
        f = font_at(size_for_box(text, MOD_CAP_W - 12, MOD_LABEL_H))
        mask = glyph_mask(text, f, (CELL, CELL), CELL / 2,
                          cy=label_cy - NUDGE.get(key, 0))
        tiles[key] = (paint(mod_cap, mask), "label '%s'" % text)

    # ---- pack: header, fixed-size records, then interleaved G/A pixels ----
    entries, blob = [], bytearray()
    for key in sorted(tiles, key=lambda k: DIK[k]):
        img, note = tiles[key]
        bb = img.split()[3].point(lambda v: 255 if v > 0 else 0).getbbox()
        x0, y0, x1, y1 = bb
        w, h = x1 - x0, y1 - y0
        g, a = img.crop(bb).split()[1], img.crop(bb).split()[3]
        gp, ap = g.load(), a.load()
        off = len(blob)
        for y in range(h):
            for x in range(w):
                blob += bytes((gp[x, y], ap[x, y]))
        entries.append((key, DIK[key], x0, y0, w, h, off, w * h * 2, note))

    with open(os.path.join(outdir, "keycaps.bin"), "wb") as fp:
        fp.write(struct.pack("<4sIII", b"KRKC", 1, len(entries), len(blob)))
        for _, dik, x, y, w, h, off, ln, _ in entries:
            fp.write(struct.pack("<HBBBBHII", dik, x, y, w, h, 0, off, ln))
        fp.write(bytes(blob))

    manifest = {
        "format": "KRKC v1: header <4sIII>, then <HBBBBHII> per key "
                  "(dik, x, y, w, h, pad, offset, length), then G/A interleaved pixels",
        "cell": CELL,
        "modifier_cap_width": MOD_CAP_W,
        "font": os.path.basename(FONT),
        "font_weight": FONT_WEIGHT,
        "keys": [{"key": k, "dik": d, "x": x, "y": y, "w": w, "h": h,
                  "offset": o, "length": l, "note": n}
                 for k, d, x, y, w, h, o, l, n in entries],
    }
    with open(os.path.join(outdir, "keycaps.json"), "w", encoding="utf-8") as fp:
        json.dump(manifest, fp, indent=1)

    # ---- preview sheet, composited on black so the white art is visible ----
    cols, zoom = 10, 3
    rows = (len(entries) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * CELL * zoom, rows * (CELL + 12) * zoom), (0, 0, 0))
    d = ImageDraw.Draw(sheet)
    try:
        lf = ImageFont.truetype(r"C:\Windows\Fonts\consola.ttf", 12)
    except Exception:
        lf = ImageFont.load_default()
    for i, entry in enumerate(entries):
        key = entry[0]
        b = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 255))
        b.alpha_composite(tiles[key][0])
        cx, cy = i % cols, i // cols
        sheet.paste(b.convert("RGB").resize((CELL * zoom, CELL * zoom), Image.NEAREST),
                    (cx * CELL * zoom, cy * (CELL + 12) * zoom))
        d.text((cx * CELL * zoom + 4, cy * (CELL + 12) * zoom + CELL * zoom + 2),
               key + (" *" if tiles[key][1] == "reused" else ""),
               fill=(120, 200, 255), font=lf)
    sheet.save(os.path.join(outdir, "preview.png"))

    print("%d keys -> %s" % (len(entries), outdir))
    print("  keycaps.bin  %d bytes (%d bytes of pixels)"
          % (16 + len(entries) * 16 + len(blob), len(blob)))
    print("  reused from the original atlas: %s" % ", ".join(REUSED))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    build(sys.argv[1], sys.argv[2])
