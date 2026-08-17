#include "loader.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>

namespace {

// The loader names a mod after its folder, and that is what it writes into
// enabled_mods.
bool ModName(const wchar_t* modDir, char* out, int cap) {
    wchar_t tmp[MAX_PATH];
    wcscpy_s(tmp, MAX_PATH, modDir);
    size_t n = wcslen(tmp);
    if (n && tmp[n - 1] == L'\\')
        tmp[n - 1] = 0;
    const wchar_t* leaf = wcsrchr(tmp, L'\\');
    leaf = leaf ? leaf + 1 : tmp;
    if (!*leaf)
        return false;
    return WideCharToMultiByte(CP_UTF8, 0, leaf, -1, out, cap, nullptr, nullptr) > 0;
}

char* ReadText(const wchar_t* path, unsigned& sizeOut) {
    HANDLE fh = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fh == INVALID_HANDLE_VALUE)
        return nullptr;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(fh, &sz) || sz.QuadPart > 1024 * 1024) {
        CloseHandle(fh);
        return nullptr;
    }
    unsigned size = (unsigned)sz.QuadPart;
    char* buf = (char*)malloc(size + 1);
    DWORD got = 0;
    if (!buf || !ReadFile(fh, buf, size, &got, nullptr) || got != size) {
        CloseHandle(fh);
        free(buf);
        return nullptr;
    }
    CloseHandle(fh);
    buf[size] = 0;
    sizeOut = size;
    return buf;
}

// enabled_mods is a TOML array of quoted names. It is written on one line for a
// short list but may wrap, so scan from the opening bracket to its match rather
// than reading a single line.
bool NameInArray(const char* text, const char* name) {
    const char* key = strstr(text, "enabled_mods");
    if (!key)
        return false;
    const char* open = strchr(key, '[');
    if (!open)
        return false;
    const char* end = strchr(open, ']');
    if (!end)
        end = text + strlen(text);

    size_t len = strlen(name);
    for (const char* p = open; p < end; ++p) {
        if (*p != '\'' && *p != '"')
            continue;
        char quote = *p;
        const char* start = p + 1;
        const char* stop = (const char*)memchr(start, quote, (size_t)(end - start));
        if (!stop)
            break;
        if ((size_t)(stop - start) == len && _strnicmp(start, name, len) == 0)
            return true;
        p = stop;
    }
    return false;
}

}  // namespace

bool LoaderGameRoot(const wchar_t* modDir, wchar_t* out) {
    wcscpy_s(out, MAX_PATH, modDir);
    size_t n = wcslen(out);
    if (n && out[n - 1] == L'\\')
        out[n - 1] = 0;
    for (int i = 0; i < 2; ++i) {          // <game>\mods\<name> -> <game>
        wchar_t* slash = wcsrchr(out, L'\\');
        if (!slash)
            return false;
        *slash = 0;
    }
    wcscat_s(out, MAX_PATH, L"\\");
    return true;
}

int LoaderEnabledLastLaunch(const wchar_t* modDir) {
    char name[128];
    wchar_t path[MAX_PATH];
    if (!ModName(modDir, name, sizeof(name)) || !LoaderGameRoot(modDir, path))
        return kLoaderUnknown;
    wcscat_s(path, MAX_PATH, L"modloader.toml");

    unsigned size = 0;
    char* text = ReadText(path, size);
    if (!text)
        return kLoaderUnknown;
    int state = NameInArray(text, name) ? kLoaderWasOn : kLoaderWasOff;
    free(text);
    return state;
}
