#include "atlas.h"

#include "keycaps.h"
#include "slots.h"

#include <string.h>

#define STB_DXT_IMPLEMENTATION
#include "third_party/stb_dxt.h"

namespace {

const int kBlocksPerRow = kAtlasSide / 4;
const int kCellBlocks = kCellSize / 4;
const unsigned char kRB = 123;      // R and B are constant across this atlas

// One cell of the working image: G and A interleaved, the same layout the
// keycap library stores its tiles in.
void EncodeCell(unsigned char* bc3, int col, int row, const unsigned char* ga) {
    for (int by = 0; by < kCellBlocks; ++by) {
        for (int bx = 0; bx < kCellBlocks; ++bx) {
            unsigned char rgba[64];
            for (int py = 0; py < 4; ++py) {
                for (int px = 0; px < 4; ++px) {
                    const unsigned char* s =
                        ga + (((by * 4 + py) * kCellSize) + bx * 4 + px) * 2;
                    unsigned char* d = rgba + (py * 4 + px) * 4;
                    d[0] = kRB;
                    d[1] = s[0];
                    d[2] = kRB;
                    d[3] = s[1];
                }
            }
            int block = (row * kCellBlocks + by) * kBlocksPerRow + col * kCellBlocks + bx;
            stb_compress_dxt_block(bc3 + block * 16, rgba, 1, STB_DXT_HIGHQUAL);
        }
    }
}

void PasteTile(unsigned char* cell, const KeycapTile& t) {
    for (int y = 0; y < t.h; ++y) {
        for (int x = 0; x < t.w; ++x) {
            const unsigned char* s = t.pixels + (y * t.w + x) * 2;
            unsigned char* d = cell + ((t.y + y) * kCellSize + t.x + x) * 2;
            d[0] = s[0];
            d[1] = s[1];
        }
    }
}

// Lettering is a coverage mask: it lifts the backdrop towards opaque white,
// which is how the original small keycaps are drawn.
void PaintMask(unsigned char* cell, const KeycapTile& t, int ox, int oy) {
    for (int y = 0; y < t.h; ++y) {
        for (int x = 0; x < t.w; ++x) {
            unsigned m = t.pixels[y * t.w + x];
            if (!m)
                continue;
            unsigned char* d = cell + ((oy + t.y + y) * kCellSize + ox + t.x + x) * 2;
            d[0] = (unsigned char)(d[0] + (255 - d[0]) * m / 255);
            d[1] = (unsigned char)(d[1] + (255 - d[1]) * m / 255);
        }
    }
}

}  // namespace

int AtlasCompose(const Keycaps& kc, const unsigned char* bind,
                 unsigned char* bc3, unsigned bc3Size) {
    if (bc3Size < (unsigned)(kBlocksPerRow * kBlocksPerRow * 16))
        return 0;

    unsigned char cell[kCellSize * kCellSize * 2];
    int written = 0;

    for (int i = 0; i < kSingleCellCount; ++i) {
        const SingleCell& sc = kSingleCells[i];
        KeycapTile t;
        if (!bind[sc.dik] || !KeycapsTile(kc, 0, bind[sc.dik], t))
            continue;
        memset(cell, 0, sizeof(cell));
        PasteTile(cell, t);
        EncodeCell(bc3, sc.col, sc.row, cell);
        ++written;
    }

    for (int i = 0; i < kDerivedCellCount; ++i) {
        const DerivedCell& dc = kDerivedCells[i];
        const unsigned char* backdrop = KeycapsBackdrop(kc, dc.kind);
        if (!backdrop)
            continue;
        memcpy(cell, backdrop, sizeof(cell));
        for (int s = 0; s < dc.slotCount; ++s) {
            KeycapTile t;
            unsigned char phys = bind[dc.slot[s].dik];
            if (!phys || !KeycapsTile(kc, 1 + dc.kind, phys, t))
                continue;
            PaintMask(cell, t, dc.slot[s].x, dc.slot[s].y);
        }
        EncodeCell(bc3, dc.col, dc.row, cell);
        ++written;
    }
    return written;
}
