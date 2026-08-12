// The game reads the keyboard through DirectInput8. Rather than patching game
// code, we swap two entries in the IDirectInputDevice8 vtable: dinput8.dll
// keeps one shared vtable per interface, so creating a throwaway keyboard
// device is enough to reach the table the game's own device will use - it does
// not matter whether the game made its device before or after we load.
//
// Inside the hook the raw key array is rewritten so the game still sees its
// stock Layout A keys. No game code has to be reversed.

#define DIRECTINPUT_VERSION 0x0800

#include "input_hook.h"
#include "config.h"
#include "keys.h"
#include "log.h"
#include "remap.h"

#include <windows.h>
#include <dinput.h>
#include <string.h>

namespace {

// vtable slots of IDirectInputDevice8, counting IUnknown's three.
const int kSlotGetDeviceState = 9;
const int kSlotGetDeviceData = 10;

typedef HRESULT(WINAPI* PFN_DirectInput8Create)(HINSTANCE, DWORD, REFIID, LPVOID*, LPUNKNOWN);
typedef HRESULT(WINAPI* PFN_GetDeviceState)(void*, DWORD, LPVOID);
typedef HRESULT(WINAPI* PFN_GetDeviceData)(void*, DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD);

const Config* g_cfg = nullptr;
RemapTables g_tab;

PFN_GetDeviceState g_origStateA = nullptr;
PFN_GetDeviceState g_origStateW = nullptr;
PFN_GetDeviceData g_origDataA = nullptr;
PFN_GetDeviceData g_origDataW = nullptr;

// Which device pointers are keyboards. Buffered reads carry no size we could
// test, and a mouse's dwOfs values overlap keyboard scancodes, so the device
// type has to be checked - once per device, then remembered.
struct DevKind { void* dev; bool keyboard; };
DevKind g_devKind[16];
int g_devKindCount = 0;

// Logged once each, so the log shows which path the game actually uses.
bool g_sawState = false;
bool g_sawBuffer = false;

// The vtable we already patched, so a second interface variant sharing it is
// left alone.
void* g_patchedVtable = nullptr;

void NoteFirstUse(bool& flag, const char* what) {
    if (flag)
        return;
    flag = true;
    Log("hook is live: game reads the keyboard via %s", what);
}

// Troubleshooting: report the array indices the game really receives, so a key
// that "does nothing" can be traced to the scancode it actually arrives on.
int g_diagBudget = 400;

// slot 0 = the array the game handed us, 1 = what we hand back
void DiagEdges(const BYTE* arr, const char* stage, int slot) {
    static BYTE prev[2][256];
    for (int i = 0; i < 256 && g_diagBudget > 0; ++i) {
        bool now = (arr[i] & 0x80) != 0;
        bool was = (prev[slot][i] & 0x80) != 0;
        if (now != was) {
            const char* n = NameFromKey((unsigned char)i);
            Log("  %s %s 0x%02X (%s)", stage, now ? "down" : "up  ",
                i, n ? n : "unnamed");
            --g_diagBudget;
        }
    }
    memcpy(prev[slot], arr, 256);
}

// Everything the game ever asks for, so an unexpected buffer size or a second
// device shows up in the log instead of silently doing nothing.
void DiagCall(void* dev, DWORD cb, HRESULT hr, bool keyboard) {
    static void* seen[8];
    static DWORD seenCb[8];
    static int count = 0;
    for (int i = 0; i < count; ++i) {
        if (seen[i] == dev && seenCb[i] == cb)
            return;
    }
    if (count < 8) {
        seen[count] = dev;
        seenCb[count] = cb;
        ++count;
    }
    Log("GetDeviceState: device %p cb=%lu hr=0x%08lX keyboard=%d",
        dev, (unsigned long)cb, (unsigned long)hr, keyboard ? 1 : 0);
}

template <typename DevT, typename InstT>
bool IsKeyboard(DevT* dev, HRESULT(WINAPI* getInfo)(DevT*, InstT*)) {
    for (int i = 0; i < g_devKindCount; ++i) {
        if (g_devKind[i].dev == dev)
            return g_devKind[i].keyboard;
    }
    InstT inst;
    memset(&inst, 0, sizeof(inst));
    inst.dwSize = sizeof(inst);
    bool kb = SUCCEEDED(getInfo(dev, &inst)) &&
              GET_DIDEVICE_TYPE(inst.dwDevType) == DI8DEVTYPE_KEYBOARD;
    if (g_devKindCount < int(sizeof(g_devKind) / sizeof(g_devKind[0]))) {
        g_devKind[g_devKindCount].dev = dev;
        g_devKind[g_devKindCount].keyboard = kb;
        ++g_devKindCount;
    }
    return kb;
}

HRESULT WINAPI GetInfoA(IDirectInputDevice8A* d, DIDEVICEINSTANCEA* i) {
    return d->GetDeviceInfo(i);
}
HRESULT WINAPI GetInfoW(IDirectInputDevice8W* d, DIDEVICEINSTANCEW* i) {
    return d->GetDeviceInfo(i);
}

// Shared tail of both state hooks.
void HandleState(void* dev, DWORD cb, LPVOID data, HRESULT hr, bool keyboard) {
    bool diag = g_cfg && g_cfg->diag;
    if (diag)
        DiagCall(dev, cb, hr, keyboard);
    if (FAILED(hr) || cb != 256 || !data || !keyboard)
        return;
    NoteFirstUse(g_sawState, "GetDeviceState");
    if (diag)
        DiagEdges((const BYTE*)data, "in ", 0);
    RemapState(g_tab, (BYTE*)data);
    if (diag)
        DiagEdges((const BYTE*)data, "out", 1);
}

HRESULT WINAPI HookStateA(void* dev, DWORD cb, LPVOID data) {
    HRESULT hr = g_origStateA(dev, cb, data);
    bool kb = IsKeyboard<IDirectInputDevice8A, DIDEVICEINSTANCEA>(
        (IDirectInputDevice8A*)dev, GetInfoA);
    HandleState(dev, cb, data, hr, kb);
    return hr;
}

HRESULT WINAPI HookStateW(void* dev, DWORD cb, LPVOID data) {
    HRESULT hr = g_origStateW(dev, cb, data);
    bool kb = IsKeyboard<IDirectInputDevice8W, DIDEVICEINSTANCEW>(
        (IDirectInputDevice8W*)dev, GetInfoW);
    HandleState(dev, cb, data, hr, kb);
    return hr;
}

HRESULT WINAPI HookDataA(void* dev, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod,
                         LPDWORD pdwInOut, DWORD dwFlags) {
    HRESULT hr = g_origDataA(dev, cbObjectData, rgdod, pdwInOut, dwFlags);
    if (SUCCEEDED(hr) && rgdod && pdwInOut && *pdwInOut && cbObjectData &&
        IsKeyboard<IDirectInputDevice8A, DIDEVICEINSTANCEA>((IDirectInputDevice8A*)dev, GetInfoA)) {
        NoteFirstUse(g_sawBuffer, "GetDeviceData");
        *pdwInOut = RemapBuffer(g_tab, (BYTE*)rgdod, cbObjectData, *pdwInOut);
    }
    return hr;
}

HRESULT WINAPI HookDataW(void* dev, DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod,
                         LPDWORD pdwInOut, DWORD dwFlags) {
    HRESULT hr = g_origDataW(dev, cbObjectData, rgdod, pdwInOut, dwFlags);
    if (SUCCEEDED(hr) && rgdod && pdwInOut && *pdwInOut && cbObjectData &&
        IsKeyboard<IDirectInputDevice8W, DIDEVICEINSTANCEW>((IDirectInputDevice8W*)dev, GetInfoW)) {
        NoteFirstUse(g_sawBuffer, "GetDeviceData");
        *pdwInOut = RemapBuffer(g_tab, (BYTE*)rgdod, cbObjectData, *pdwInOut);
    }
    return hr;
}

bool PatchSlot(void* obj, int slot, void* hook, void** orig) {
    void** vtbl = *reinterpret_cast<void***>(obj);
    DWORD prot = 0;
    if (!VirtualProtect(&vtbl[slot], sizeof(void*), PAGE_READWRITE, &prot))
        return false;
    *orig = vtbl[slot];
    vtbl[slot] = hook;
    VirtualProtect(&vtbl[slot], sizeof(void*), prot, &prot);
    return true;
}

// Creates a throwaway keyboard device purely to reach the shared vtable.
bool PatchVariant(PFN_DirectInput8Create create, HINSTANCE self, REFIID iid,
                  void* hookState, void** origState,
                  void* hookData, void** origData, const char* what) {
    void* di = nullptr;
    HRESULT hr = create(self, DIRECTINPUT_VERSION, iid, &di, nullptr);
    if (FAILED(hr) || !di) {
        Log("  %s: DirectInput8Create failed (0x%08lX)", what, (unsigned long)hr);
        return false;
    }
    // IDirectInput8A and W share this vtable layout; CreateDevice is slot 3.
    typedef HRESULT(WINAPI* PFN_CreateDevice)(void*, REFGUID, void**, LPUNKNOWN);
    void** divt = *reinterpret_cast<void***>(di);
    PFN_CreateDevice createDevice = reinterpret_cast<PFN_CreateDevice>(divt[3]);
    typedef ULONG(WINAPI* PFN_Release)(void*);
    PFN_Release release = reinterpret_cast<PFN_Release>(divt[2]);

    void* dev = nullptr;
    hr = createDevice(di, GUID_SysKeyboard, &dev, nullptr);
    if (FAILED(hr) || !dev) {
        Log("  %s: CreateDevice failed (0x%08lX)", what, (unsigned long)hr);
        release(di);
        return false;
    }

    // dinput8 hands the ANSI and Unicode interfaces the same vtable. Patching
    // it twice would chain our hooks, so the remap would run twice per read and
    // cancel itself out - the bug that made both the old and new key go dead.
    void** devvt = *reinterpret_cast<void***>(dev);
    bool ok;
    if (devvt == g_patchedVtable) {
        Log("  %s: same vtable as the variant already patched (%p), skipping",
            what, (void*)devvt);
        ok = true;
    } else {
        ok = PatchSlot(dev, kSlotGetDeviceState, hookState, origState) &&
             PatchSlot(dev, kSlotGetDeviceData, hookData, origData);
        Log("  %s: vtable %p patch %s", what, (void*)devvt, ok ? "ok" : "FAILED");
        if (ok)
            g_patchedVtable = devvt;
    }

    reinterpret_cast<PFN_Release>(devvt[2])(dev);
    release(di);
    return ok;
}

}  // namespace

bool InputHookInstall(const Config* cfg) {
    g_cfg = cfg;
    RemapBuild(g_tab, *cfg);

    HMODULE dinput = LoadLibraryA("dinput8.dll");
    if (!dinput) {
        Log("input hook: dinput8.dll not available");
        return false;
    }
    PFN_DirectInput8Create create =
        (PFN_DirectInput8Create)GetProcAddress(dinput, "DirectInput8Create");
    if (!create) {
        Log("input hook: DirectInput8Create not exported");
        return false;
    }

    HINSTANCE self = GetModuleHandleA(nullptr);
    Log("input hook: patching IDirectInputDevice8 vtables");
    bool a = PatchVariant(create, self, IID_IDirectInput8A,
                          (void*)HookStateA, (void**)&g_origStateA,
                          (void*)HookDataA, (void**)&g_origDataA, "ANSI");
    bool w = PatchVariant(create, self, IID_IDirectInput8W,
                          (void*)HookStateW, (void**)&g_origStateW,
                          (void*)HookDataW, (void**)&g_origDataW, "Unicode");
    return a || w;
}

void InputHookRefresh() {
    if (g_cfg)
        RemapBuild(g_tab, *g_cfg);
}
