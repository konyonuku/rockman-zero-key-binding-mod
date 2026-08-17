#pragma once

// What the mod loader can tell us about the previous launch.
//
// stage0 of the loader clears enabled_mods, then walks the enabled mods -
// loading each DLL and calling its mod_open - and only writes modloader.toml
// back once that whole loop is done. So while mod_open is running, the file on
// disk still describes the launch before this one. That is what lets us notice
// that this mod has just been switched on, which is otherwise invisible: a mod
// that is off does not run and cannot record anything.

// <game>\mods\<name>\ -> <game>\ ; false when the path is not shaped like that.
bool LoaderGameRoot(const wchar_t* modDir, wchar_t* out);

enum LoaderEnabledState {
    kLoaderUnknown = -1,    // modloader.toml is missing or unreadable
    kLoaderWasOff = 0,      // this mod was not enabled on the previous launch
    kLoaderWasOn = 1,
};

int LoaderEnabledLastLaunch(const wchar_t* modDir);
