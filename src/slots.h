#pragma once

// Where the mod writes keycap art inside the button atlas
// (btn_icon_pcb_00_ID_HQ, 512x512, laid out in 64px cells). The numbers were
// measured off the original artwork, and tools/make_keycaps.py bakes the keycap
// library against the same ones, so the two have to change together.

const int kAtlasSide = 512;
const int kCellSize = 64;

// Kinds of small keycap the cluster and pair cells are built from. The order
// matches the tile groups in keycaps.bin.
enum MiniKind {
    kMiniCluster4 = 0,
    kMiniPairV = 1,
    kMiniPairH = 2,
    kMiniKindCount = 3,
};

// Size of the lettering box on a small keycap, one entry per kind.
struct MiniBox {
    unsigned char w, h;
};
extern const MiniBox kMiniBox[kMiniKindCount];

// One small keycap: which Layout A key it stands for, and where its lettering
// box sits inside the cell.
struct MiniSlot {
    unsigned char dik;
    unsigned char x, y;
};

// A cell holding one keycap. Its art is copied in whole.
struct SingleCell {
    unsigned char dik;
    unsigned char col, row;
};

// A cell holding two or four small keycaps on a shared backdrop.
struct DerivedCell {
    unsigned char col, row;
    unsigned char kind;
    unsigned char slotCount;
    MiniSlot slot[4];
};

extern const SingleCell kSingleCells[];
extern const int kSingleCellCount;

extern const DerivedCell kDerivedCells[];
extern const int kDerivedCellCount;
