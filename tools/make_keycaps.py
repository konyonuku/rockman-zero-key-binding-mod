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


# ---- mini keycaps -----------------------------------------------------------
# Cluster and pair cells hold small keycaps the game draws for movement groups.
# Their artwork is recovered from the cells that share a cap shape, and each key
# gets a small label rendered at that size; scaling the full-size cap down turns
# the wide keys (Space, LSHIFT) into mush.
MINI_KINDS = ["cluster4", "pair_v", "pair_h"]

# src   = cells sharing this cap shape, min over them recovers the blank caps
# face  = top-left of every slot's lettering box, all the same size per kind
# size  = that box, w x h
# cap_h = lettering cap height inside it
MINI_GEOM = {
    "cluster4": {
        "src": [(4, 1), (4, 2)],
        "size": (10, 11), "cap_h": 7,
        "face": {"up": (27, 19), "left": (13, 33), "down": (27, 33), "right": (41, 33)},
    },
    "pair_v": {
        "src": [(5, 0), (5, 1), (5, 2)],
        "size": (20, 19), "cap_h": 13,
        "face": {"up": (22, 8), "down": (22, 36)},
    },
    "pair_h": {
        "src": [(6, 0), (6, 1), (6, 2)],
        "size": (14, 15), "cap_h": 10,
        "face": {"left": (15, 24), "right": (36, 24)},
    },
}

# Which original cell each kind takes its arrow artwork from.
MINI_ARROW_CELL = {"cluster4": (4, 1), "pair_v": (5, 1), "pair_h": (6, 1)}
ARROW_SLOT = {"UP": "up", "DOWN": "down", "LEFT": "left", "RIGHT": "right"}

# Two letters is all a mini cap fits.
MINI_LABEL = dict(GLYPH_KEYS)
MINI_LABEL.update({
    "SPACE": "SP", "ENTER": "EN", "ESC": "ES", "TAB": "TB", "CAPSLOCK": "CL",
    "LSHIFT": "LS", "RSHIFT": "RS", "LCTRL": "LC", "RCTRL": "RC",
    "LALT": "LA", "RALT": "RA",
})

# Alpha at or below this is cap face, above it is lettering.
FACE_MAX = 100

# At mini sizes antialiasing thins the lettering to almost nothing - a backtick
# nearly vanishes. This fattens the coverage the way the original small art is
# drawn, without touching the pixels the glyph already covers fully.
MINI_GAMMA = [min(255, round(255 * (v / 255.0) ** 0.62)) for v in range(256)]

# Lettering shorter than this fraction of the cap face is grown until it reads.
MINI_INK_MIN = 0.45


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


def blank_mini(atlas, kind):
    """Blank mini caps: min over the sibling cells, then repair the leftovers.

    The cells share a cap shape and differ only in lettering, so the minimum
    clears most of it; where two cells ink the same pixel a ghost survives, and
    that is refilled from the median of the row's remaining face pixels.
    """
    spec = MINI_GEOM[kind]
    cells = [atlas.crop((c * CELL, r * CELL, c * CELL + CELL, r * CELL + CELL))
             for c, r in spec["src"]]
    g = cells[0].split()[1]
    a = cells[0].split()[3]
    for c in cells[1:]:
        a = ImageChops.darker(a, c.split()[3])
    a = a.copy()
    px = a.load()
    fw, fh = spec["size"]
    for fx, fy in spec["face"].values():
        for y in range(fy, fy + fh):
            clean = sorted(px[x, y] for x in range(fx, fx + fw) if px[x, y] <= FACE_MAX)
            if not clean:
                continue
            med = clean[len(clean) // 2]
            for x in range(fx, fx + fw):
                if px[x, y] > FACE_MAX:
                    px[x, y] = med
    return Image.merge("RGBA", (g, g, g, a))


def ink_of(cell, blank, box):
    """Lettering inside a box as a soft coverage mask, antialiasing kept."""
    diff = ImageChops.subtract(cell.split()[3], blank.split()[3])
    x, y, w, h = box
    m = diff.crop((x, y, x + w, y + h)).point(lambda v: min(255, v * 255 // 170))
    bb = m.point(lambda v: 255 if v > 24 else 0).getbbox()
    return m.crop(bb) if bb else None


def arrow_ink(atlas, kind, key):
    """The arrow for one direction, at this kind's size where the atlas has it."""
    slot = ARROW_SLOT[key]
    spec = MINI_GEOM[kind]
    if slot in spec["face"]:
        c, r = MINI_ARROW_CELL[kind]
        cell = atlas.crop((c * CELL, r * CELL, c * CELL + CELL, r * CELL + CELL))
        fx, fy = spec["face"][slot]
        return ink_of(cell, blank_mini(atlas, kind), (fx, fy) + spec["size"])
    return None


def mini_mask(atlas, kind, key, blank_art, arrow_full):
    """One key's lettering for one kind of mini cap, in its slot-sized box.

    The hand-tuned vertical trims are reused, scaled to the smaller cap height.
    """
    fw, fh = MINI_GEOM[kind]["size"]
    box = Image.new("L", (fw, fh), 0)
    if key in ARROW_SLOT:
        ink = arrow_ink(atlas, kind, key)
        if ink is None:                       # no arrow this size: shrink the big one
            ink = arrow_full[key]
            sc = min(fw / ink.size[0], (fh - 2) / ink.size[1])
            ink = ink.resize((max(1, round(ink.size[0] * sc)),
                              max(1, round(ink.size[1] * sc))), Image.LANCZOS)
        box.paste(ink, ((fw - ink.size[0]) // 2, (fh - ink.size[1]) // 2))
        return box
    text = MINI_LABEL[key]
    cap_h = MINI_GEOM[kind]["cap_h"]
    size = size_for_cap_height(cap_h)
    while size > 6 and ink_box(size, text)[2] - ink_box(size, text)[0] > fw:
        size -= 1
    # A comma or a backtick inks so little at this size that it disappears. The
    # original small artwork draws those oversized, so grow them the same way
    # until they read, stopping before they touch the edges of the cap face.
    while True:
        bb = ink_box(size + 1, text)
        if bb[3] - bb[1] > MINI_INK_MIN * fh or bb[2] - bb[0] > fw:
            break
        size += 1
    nudge = round(NUDGE.get(key, 0) * cap_h / CAP_H)
    m = glyph_mask(text, font_at(size), (fw, fh), fw / 2,
                   baseline=(fh + cap_h) // 2 - nudge)
    return m.point(MINI_GAMMA)


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


def mini_sheet(outdir, atlas, templates, minis, order):
    """Every key drawn into every slot of every mini cap kind, for eyeballing."""
    zoom, cols = 3, 8
    rows = (len(order) + cols - 1) // cols
    tiles_per = len(MINI_KINDS)
    sheet = Image.new("RGB", (cols * CELL * zoom,
                              tiles_per * rows * (CELL + 12) * zoom), (0, 0, 0))
    d = ImageDraw.Draw(sheet)
    try:
        lf = ImageFont.truetype(r"C:\Windows\Fonts\consola.ttf", 12)
    except Exception:
        lf = ImageFont.load_default()
    y0 = 0
    for ki, kind in enumerate(MINI_KINDS):
        tmpl = templates[ki]
        for i, key in enumerate(order):
            cell = tmpl.copy()
            for slot, (fx, fy) in MINI_GEOM[kind]["face"].items():
                fw, fh = MINI_GEOM[kind]["size"]
                mask = Image.new("L", (CELL, CELL), 0)
                mask.paste(minis[kind][key], (fx, fy))
                cell = paint(cell, mask)
            b = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 255))
            b.alpha_composite(cell)
            cx, cy = i % cols, i // cols
            sheet.paste(b.convert("RGB").resize((CELL * zoom, CELL * zoom), Image.NEAREST),
                        (cx * CELL * zoom, y0 + cy * (CELL + 12) * zoom))
            d.text((cx * CELL * zoom + 4,
                    y0 + cy * (CELL + 12) * zoom + CELL * zoom + 2),
                   "%s %s" % (kind[:4], key), fill=(120, 200, 255), font=lf)
        y0 += rows * (CELL + 12) * zoom
    sheet.save(os.path.join(outdir, "preview_mini.png"))


def build(tex_path, outdir):
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    slots = json.load(open(os.path.join(root, "data", "slots.json"), encoding="utf-8"))
    atlas = load_atlas(tex_path)
    os.makedirs(outdir, exist_ok=True)

    blank_art = blank_keycap(atlas, slots)
    blank_art_cell = Image.new("RGBA", (CELL, CELL), (0, 0, 0, 0))
    blank_art_cell.paste(blank_art, (ART_OFF, ART_OFF))
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

    # ---- mini caps: one lettering mask per key, per kind of mini cap ----
    arrow_full = {}
    for key in ARROW_SLOT:
        ink = ink_of(cell_of(atlas, slots, key), blank_art_cell,
                     (ART_OFF, ART_OFF, ART, ART))
        arrow_full[key] = ink
    minis = {k: {key: mini_mask(atlas, k, key, blank_art, arrow_full)
                 for key in tiles} for k in MINI_KINDS}
    templates = [blank_mini(atlas, k) for k in MINI_KINDS]

    # ---- pack: header, template offsets, per-group records, then pixels ----
    order = sorted(tiles, key=lambda k: DIK[k])
    blob = bytearray()

    tmpl_off = []
    for t in templates:
        gp, ap = t.split()[1].load(), t.split()[3].load()
        tmpl_off.append(len(blob))
        for y in range(CELL):
            for x in range(CELL):
                blob += bytes((gp[x, y], ap[x, y]))

    groups = []
    entries = []
    for key in order:                      # group 0: full-size cell art, G then A
        img, note = tiles[key]
        bb = img.split()[3].point(lambda v: 255 if v > 0 else 0).getbbox()
        x0, y0, x1, y1 = bb
        w, h = x1 - x0, y1 - y0
        gp = img.crop(bb).split()[1].load()
        ap = img.crop(bb).split()[3].load()
        off = len(blob)
        for y in range(h):
            for x in range(w):
                blob += bytes((gp[x, y], ap[x, y]))
        entries.append((key, DIK[key], x0, y0, w, h, off, w * h * 2, note))
    groups.append(entries)

    for kind in MINI_KINDS:                # groups 1..3: coverage masks
        recs = []
        for key in order:
            m = minis[kind][key]
            bb = m.getbbox()
            if bb is None:
                recs.append((key, DIK[key], 0, 0, 0, 0, 0, 0, "empty"))
                continue
            x0, y0, x1, y1 = bb
            w, h = x1 - x0, y1 - y0
            mp = m.crop(bb).load()
            off = len(blob)
            for y in range(h):
                for x in range(w):
                    blob += bytes((mp[x, y],))
            recs.append((key, DIK[key], x0, y0, w, h, off, w * h, kind))
        groups.append(recs)

    with open(os.path.join(outdir, "keycaps.bin"), "wb") as fp:
        fp.write(struct.pack("<4sIIIII", b"KRKC", 2, len(order),
                             len(groups), len(templates), len(blob)))
        for o in tmpl_off:
            fp.write(struct.pack("<I", o))
        for recs in groups:
            for _, dik, x, y, w, h, off, ln, _ in recs:
                fp.write(struct.pack("<HBBBBHII", dik, x, y, w, h, 0, off, ln))
        fp.write(bytes(blob))

    manifest = {
        "format": "KRKC v2: header <4sIIIII> (magic, version, key_count, "
                  "group_count, template_count, blob_size), then one u32 blob "
                  "offset per 64x64 G/A template, then group_count x key_count "
                  "records <HBBBBHII> (dik, x, y, w, h, pad, offset, length), "
                  "then the pixel blob",
        "groups": ["full"] + MINI_KINDS,
        "group_pixels": {"full": "G/A interleaved, x/y are cell-relative",
                         "mini": "coverage mask, x/y are relative to the slot face box"},
        "templates": MINI_KINDS,
        "cell": CELL,
        "modifier_cap_width": MOD_CAP_W,
        "mini_geometry": {k: {"size": MINI_GEOM[k]["size"],
                              "cap_h": MINI_GEOM[k]["cap_h"],
                              "face": MINI_GEOM[k]["face"]} for k in MINI_KINDS},
        "font": os.path.basename(FONT),
        "font_weight": FONT_WEIGHT,
        "keys": [{"key": k, "dik": d, "x": x, "y": y, "w": w, "h": h,
                  "offset": o, "length": l, "note": n}
                 for k, d, x, y, w, h, o, l, n in entries],
        "mini_keys": {kind: [{"key": r[0], "x": r[2], "y": r[3], "w": r[4],
                              "h": r[5], "offset": r[6], "length": r[7]}
                             for r in groups[1 + i]]
                      for i, kind in enumerate(MINI_KINDS)},
    }
    with open(os.path.join(outdir, "keycaps.json"), "w", encoding="utf-8") as fp:
        json.dump(manifest, fp, indent=1)

    mini_sheet(outdir, atlas, templates, minis, order)

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
    print("  mini caps: %s" % ", ".join(MINI_KINDS))
    print("  reused from the original atlas: %s" % ", ".join(REUSED))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(1)
    build(sys.argv[1], sys.argv[2])
