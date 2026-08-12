#include "keys.h"

#include <string.h>

const KeyName kBindableKeys[] = {
    { "ESC", 0x01 },
    { "N1", 0x02 }, { "N2", 0x03 }, { "N3", 0x04 }, { "N4", 0x05 }, { "N5", 0x06 },
    { "N6", 0x07 }, { "N7", 0x08 }, { "N8", 0x09 }, { "N9", 0x0A }, { "N0", 0x0B },
    { "MINUS", 0x0C }, { "EQUALS", 0x0D }, { "TAB", 0x0F },
    { "Q", 0x10 }, { "W", 0x11 }, { "E", 0x12 }, { "R", 0x13 }, { "T", 0x14 },
    { "Y", 0x15 }, { "U", 0x16 }, { "I", 0x17 }, { "O", 0x18 }, { "P", 0x19 },
    { "LBRACKET", 0x1A }, { "RBRACKET", 0x1B }, { "ENTER", 0x1C }, { "LCTRL", 0x1D },
    { "A", 0x1E }, { "S", 0x1F }, { "D", 0x20 }, { "F", 0x21 }, { "G", 0x22 },
    { "H", 0x23 }, { "J", 0x24 }, { "K", 0x25 }, { "L", 0x26 },
    { "SEMICOLON", 0x27 }, { "APOSTROPHE", 0x28 }, { "GRAVE", 0x29 },
    { "LSHIFT", 0x2A }, { "BACKSLASH", 0x2B },
    { "Z", 0x2C }, { "X", 0x2D }, { "C", 0x2E }, { "V", 0x2F }, { "B", 0x30 },
    { "N", 0x31 }, { "M", 0x32 },
    { "COMMA", 0x33 }, { "PERIOD", 0x34 }, { "SLASH", 0x35 },
    { "RSHIFT", 0x36 }, { "LALT", 0x38 }, { "SPACE", 0x39 }, { "CAPSLOCK", 0x3A },
    { "RCTRL", 0x9D }, { "RALT", 0xB8 },
    { "UP", 0xC8 }, { "LEFT", 0xCB }, { "RIGHT", 0xCD }, { "DOWN", 0xD0 },
};
const int kBindableKeyCount = int(sizeof(kBindableKeys) / sizeof(kBindableKeys[0]));

// Layout A, as data/actions.json describes it. Zero and ZX put different
// actions on the same key, but the set of keys the game reads is the same.
const unsigned char kGameKeys[] = {
    0xC8, 0xD0, 0xCB, 0xCD,     // UP DOWN LEFT RIGHT
    0x39,                       // SPACE
    0x1E, 0x20, 0x2E, 0x2D,     // A D C X
    0x1F, 0x21,                 // S F
    0x2C, 0x1C,                 // Z ENTER
    0x25, 0x32, 0x33, 0x34,     // K M COMMA PERIOD
    0x0F, 0x01,                 // TAB ESC
};
const int kGameKeyCount = int(sizeof(kGameKeys) / sizeof(kGameKeys[0]));

unsigned char KeyFromName(const char* name) {
    if (!name) return 0;
    for (int i = 0; i < kBindableKeyCount; ++i) {
        if (_stricmp(name, kBindableKeys[i].name) == 0)
            return kBindableKeys[i].dik;
    }
    return 0;
}

const char* NameFromKey(unsigned char dik) {
    for (int i = 0; i < kBindableKeyCount; ++i) {
        if (kBindableKeys[i].dik == dik)
            return kBindableKeys[i].name;
    }
    return nullptr;
}

bool IsGameKey(unsigned char dik) {
    for (int i = 0; i < kGameKeyCount; ++i) {
        if (kGameKeys[i] == dik)
            return true;
    }
    return false;
}
