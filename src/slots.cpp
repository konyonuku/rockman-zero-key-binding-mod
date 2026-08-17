#include "slots.h"

const MiniBox kMiniBox[kMiniKindCount] = {
    { 10, 11 },   // cluster4
    { 20, 19 },   // pair_v
    { 14, 15 },   // pair_h
};

// The 19 keys Layout A uses. W, I, J, U, O and 7-9 also have cells, but they
// belong to Layout B and are left alone.
const SingleCell kSingleCells[] = {
    { 0x1E, 1, 0 },   // A
    { 0x1F, 2, 0 },   // S
    { 0x20, 3, 0 },   // D
    { 0xC8, 0, 1 },   // UP
    { 0xCB, 1, 1 },   // LEFT
    { 0xCD, 2, 1 },   // RIGHT
    { 0xD0, 3, 1 },   // DOWN
    { 0x25, 0, 2 },   // K
    { 0x32, 1, 2 },   // M
    { 0x33, 2, 2 },   // COMMA
    { 0x34, 3, 2 },   // PERIOD
    { 0x2C, 4, 3 },   // Z
    { 0x2D, 5, 3 },   // X
    { 0x2E, 6, 3 },   // C
    { 0x21, 7, 3 },   // F
    { 0x01, 3, 4 },   // ESC
    { 0x39, 4, 4 },   // SPACE
    { 0x0F, 5, 4 },   // TAB
    { 0x1C, 6, 4 },   // ENTER
};
const int kSingleCellCount = sizeof(kSingleCells) / sizeof(kSingleCells[0]);

// Movement groups: the arrows in both games, and the sub-display cursor in ZX.
// The WASD cluster and its pairs in row 0 are Layout B artwork and stay as they
// are.
const DerivedCell kDerivedCells[] = {
    { 4, 1, kMiniCluster4, 4, {
        { 0xC8, 27, 19 },   // UP
        { 0xCB, 13, 33 },   // LEFT
        { 0xD0, 27, 33 },   // DOWN
        { 0xCD, 41, 33 },   // RIGHT
    } },
    { 5, 1, kMiniPairV, 2, {
        { 0xC8, 22,  8 },   // UP
        { 0xD0, 22, 36 },   // DOWN
    } },
    { 6, 1, kMiniPairH, 2, {
        { 0xCB, 15, 24 },   // LEFT
        { 0xCD, 36, 24 },   // RIGHT
    } },
    { 4, 2, kMiniCluster4, 4, {
        { 0x25, 27, 19 },   // K
        { 0x32, 13, 33 },   // M
        { 0x33, 27, 33 },   // COMMA
        { 0x34, 41, 33 },   // PERIOD
    } },
    { 5, 2, kMiniPairV, 2, {
        { 0x25, 22,  8 },   // K
        { 0x33, 22, 36 },   // COMMA
    } },
    { 6, 2, kMiniPairH, 2, {
        { 0x32, 15, 24 },   // M
        { 0x34, 36, 24 },   // PERIOD
    } },
};
const int kDerivedCellCount = sizeof(kDerivedCells) / sizeof(kDerivedCells[0]);
