#include "remap.h"
#include "config.h"
#include "keys.h"

#include <string.h>

void RemapBuild(RemapTables& tab, const Config& cfg) {
    memset(&tab, 0, sizeof(tab));
    memcpy(tab.bind, cfg.bind, sizeof(tab.bind));
    for (int i = 0; i < kGameKeyCount; ++i) {
        unsigned char game = kGameKeys[i];
        unsigned char phys = tab.bind[game];
        if (!phys)
            continue;
        if (!tab.reverse[phys])
            tab.reverse[phys] = game;
        // A key the player pressed only to trigger a rebound action must not
        // also reach whatever the game natively does with that key.
        if (!IsGameKey(phys))
            tab.suppress[phys] = true;
    }
}

void RemapState(const RemapTables& tab, unsigned char* state) {
    unsigned char raw[256];
    memcpy(raw, state, sizeof(raw));
    for (int i = 0; i < kGameKeyCount; ++i) {
        unsigned char game = kGameKeys[i];
        unsigned char phys = tab.bind[game];
        state[game] = phys ? raw[phys] : 0;
    }
    for (int p = 0; p < 256; ++p) {
        if (tab.suppress[p])
            state[p] = 0;
    }
}

unsigned RemapBuffer(const RemapTables& tab, unsigned char* base,
                     unsigned stride, unsigned count) {
    unsigned kept = 0;
    for (unsigned i = 0; i < count; ++i) {
        unsigned char* src = base + i * stride;
        unsigned long ofs = *reinterpret_cast<unsigned long*>(src);
        if (ofs < 256) {
            unsigned char game = tab.reverse[ofs];
            if (game) {
                *reinterpret_cast<unsigned long*>(src) = game;
            } else if (IsGameKey((unsigned char)ofs)) {
                continue;           // this key no longer drives its old action
            }
        }
        unsigned char* dst = base + kept * stride;
        if (dst != src)
            memmove(dst, src, stride);
        ++kept;
    }
    return kept;
}
