#pragma once

// Reader for keycaps.bin, the keycap library baked by tools/make_keycaps.py.
//
// KRKC v2 holds four groups of tiles, one record per key in each: group 0 is
// the full-size cell artwork (G and A interleaved), groups 1..3 are lettering
// masks for the three kinds of small keycap. It also holds one 64x64 backdrop
// per mini kind, which the cluster and pair cells are drawn on top of.

struct KeycapTile {
    const unsigned char* pixels;
    unsigned char x, y, w, h;
};

struct Keycaps {
    unsigned char* data;
    unsigned size;
    unsigned keyCount;
    unsigned groupCount;
    unsigned templateCount;
    const unsigned char* templateOffsets;   // one u32 per kind, into the blob
    const unsigned char* records;
    const unsigned char* blob;
};

bool KeycapsLoad(const wchar_t* path, Keycaps& kc);
void KeycapsFree(Keycaps& kc);

// group 0 = full-size art, 1 + MiniKind = lettering mask. False when the key
// has no tile in that group.
bool KeycapsTile(const Keycaps& kc, int group, unsigned char dik, KeycapTile& out);

// 64x64 G/A backdrop for one mini kind, or null.
const unsigned char* KeycapsBackdrop(const Keycaps& kc, int kind);
