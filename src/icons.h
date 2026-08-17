#pragma once

struct Config;

// Writes the mod copy of IconFont.arc so the button icons on screen match the
// current bindings. The loader scans this mod folder right after mod_open
// returns, so a file written here is picked up on the same launch.
//
// Rebuilding costs a BC3 encode and a zlib pass, so a stamp next to the DLL
// records what the last build was made from and the work is skipped when
// nothing that feeds it has changed.
bool IconsBuild(const wchar_t* modDir, const Config& cfg);
