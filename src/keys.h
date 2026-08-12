#pragma once

// DirectInput scancodes (DIK_*) are the ids used everywhere in this mod: the
// config file, the baked keycap library, and the input hook all speak them.

struct KeyName {
    const char* name;
    unsigned char dik;
};

// Keys a player may bind an action to (62 of them, matching the keycap library).
extern const KeyName kBindableKeys[];
extern const int kBindableKeyCount;

// Stock Layout A keys the game reads; a binding replaces one of these.
extern const unsigned char kGameKeys[];
extern const int kGameKeyCount;

unsigned char KeyFromName(const char* name);    // 0 when unknown
const char* NameFromKey(unsigned char dik);     // nullptr when unknown
bool IsGameKey(unsigned char dik);
