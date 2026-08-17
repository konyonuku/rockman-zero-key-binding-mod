// KeyRebindConfig.exe - the binding window on its own, for when the player has
// turned off the in-game prompt and wants to change keys later. It reads and
// writes the same keybind.toml the mod does, so the next launch picks the new
// bindings up and redraws the icons to match.

#include "config.h"
#include "gui.h"
#include "strings.h"

#include <windows.h>
#include <commctrl.h>

// Ask for the version 6 common controls, so the buttons and the language box
// look like the rest of the system rather than like Windows 95.
#pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#pragma comment(lib, "comctl32.lib")

namespace {

void ExeFolder(wchar_t* out) {
    GetModuleFileNameW(nullptr, out, MAX_PATH);
    wchar_t* slash = wcsrchr(out, L'\\');
    if (slash)
        slash[1] = 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // Sharp text and controls on a scaled display. Safe here in a way it would
    // not be inside the game, whose own awareness we must not touch.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    wchar_t path[MAX_PATH];
    ExeFolder(path);
    wcscat_s(path, MAX_PATH, L"keybind.toml");

    Config cfg;
    if (!ConfigLoad(path, cfg))
        ConfigDefaults(cfg);

    unsigned char clash = 0;
    if (!ConfigValidate(cfg, &clash))
        ConfigDefaults(cfg);

    if (!GuiRun(cfg))
        return 0;

    if (!ConfigSave(path, cfg)) {
        MessageBoxW(nullptr, L"Could not write keybind.toml next to this program.",
                    L"KeyRebind", MB_OK | MB_ICONERROR);
        return 1;
    }
    return 0;
}
