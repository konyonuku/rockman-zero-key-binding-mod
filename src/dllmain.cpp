// Entry point of the KeyRebind mod. The loader calls mod_open() while the game
// window is still being created, which is early enough to show the binding
// window, hook the keyboard, and write the icon atlas the loader is about to
// scan for.

#include "config.h"
#include "gui.h"
#include "icons.h"
#include "input_hook.h"
#include "keys.h"
#include "loader.h"
#include "log.h"

#include <windows.h>

namespace {

HMODULE g_self = nullptr;
Config g_cfg;
wchar_t g_modDir[MAX_PATH];

void BuildModPath(wchar_t* out, const wchar_t* leaf) {
    wcscpy_s(out, MAX_PATH, g_modDir);
    wcscat_s(out, MAX_PATH, leaf);
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

extern "C" __declspec(dllexport) void mod_open() {
    GetModuleFileNameW(g_self, g_modDir, MAX_PATH);
    wchar_t* slash = wcsrchr(g_modDir, L'\\');
    if (slash)
        slash[1] = 0;

    wchar_t path[MAX_PATH];
    BuildModPath(path, L"KeyRebind.log");
    LogInit(path);

    BuildModPath(path, L"keybind.toml");
    bool hadConfig = ConfigLoad(path, g_cfg);
    if (!hadConfig) {
        Log("no keybind.toml yet, writing defaults");
        ConfigSave(path, g_cfg);
    }

    // Whether the player has just switched this mod on, which is one of the
    // moments the binding window should appear.
    int wasOn = LoaderEnabledLastLaunch(g_modDir);
    Log("previous launch had this mod %s",
        wasOn == kLoaderWasOn ? "enabled" :
        wasOn == kLoaderWasOff ? "disabled (switched on since)" : "unknown");

    unsigned char clash = 0;
    if (!ConfigValidate(g_cfg, &clash)) {
        const char* name = NameFromKey(clash);
        Log("config has %s bound twice, falling back to defaults",
            name ? name : "a key");
        ConfigDefaults(g_cfg);
    }

    // Three moments call for the binding window: the mod has never been set up,
    // the player asked to see it again, or the mod has just been switched on.
    bool justEnabled = wasOn == kLoaderWasOff;
    if (!hadConfig || g_cfg.showGui || justEnabled) {
        Log("opening the binding window (%s)",
            !hadConfig ? "first run" : justEnabled ? "just switched on" : "asked for");
        if (GuiRun(g_cfg)) {
            ConfigSave(path, g_cfg);
            Log("settings applied");
        } else {
            Log("window closed without applying, keeping the old settings");
        }
    }

    int changed = 0;
    for (int i = 0; i < kGameKeyCount; ++i) {
        unsigned char game = kGameKeys[i];
        if (g_cfg.bind[game] != game) {
            const char* from = NameFromKey(game);
            const char* to = NameFromKey(g_cfg.bind[game]);
            Log("  %s -> %s", from ? from : "?", to ? to : "(unbound)");
            ++changed;
        }
    }
    Log("%d of %d keys rebound", changed, kGameKeyCount);

    if (!InputHookInstall(&g_cfg))
        Log("input hook failed to install");
    else
        Log("input hook installed");

    // The loader scans this mod folder for asset overrides as soon as we
    // return, so the atlas has to be on disk before then.
    IconsBuild(g_modDir, g_cfg);
}
