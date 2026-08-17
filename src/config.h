#pragma once

// keybind.toml, kept next to the DLL. Written by the binding GUI, read by the
// input hook and the atlas builder.

struct Config {
    // bind[gameKey] = the physical key the player presses for it.
    // Entries for keys the game does not use stay 0.
    unsigned char bind[256];
    bool showGui;           // show the binding window on the next launch
    bool diag;              // log every key the game actually receives
    int lang;               // Lang, the language the binding window speaks
};

void ConfigDefaults(Config& cfg);               // every game key bound to itself
bool ConfigLoad(const wchar_t* path, Config& cfg);
bool ConfigSave(const wchar_t* path, const Config& cfg);

// True when no two game keys share the same physical key.
bool ConfigValidate(const Config& cfg, unsigned char* clashOut = nullptr);
