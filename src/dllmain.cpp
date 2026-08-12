// Entry point of the KeyRebind mod. The loader calls mod_open() while the game
// window is still being created, which is early enough to hook input and (in a
// later step) to write the icon atlas the loader is about to scan for.

#include "config.h"
#include "input_hook.h"
#include "keys.h"
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
    if (!ConfigLoad(path, g_cfg)) {
        Log("no keybind.toml yet, writing defaults");
        ConfigSave(path, g_cfg);
    }

    unsigned char clash = 0;
    if (!ConfigValidate(g_cfg, &clash)) {
        const char* name = NameFromKey(clash);
        Log("config has %s bound twice, falling back to defaults",
            name ? name : "a key");
        ConfigDefaults(g_cfg);
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
}
