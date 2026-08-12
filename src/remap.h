#pragma once

// The key-substitution rules, kept free of Windows and DirectInput so they can
// be exercised by tests/test_remap.cpp without a game attached.

struct Config;

struct RemapTables {
    unsigned char bind[256];      // game key -> physical key the player presses
    unsigned char reverse[256];   // physical key -> game key it stands in for
    bool suppress[256];           // physical keys the game must never see raw
};

void RemapBuild(RemapTables& tab, const Config& cfg);

// Rewrites a 256-byte DirectInput key array in place.
void RemapState(const RemapTables& tab, unsigned char* state);

// Rewrites buffered events in place; returns how many survive. Each event is
// `stride` bytes and starts with a DWORD offset (the scancode, for keyboards).
unsigned RemapBuffer(const RemapTables& tab, unsigned char* base,
                     unsigned stride, unsigned count);
