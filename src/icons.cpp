#include "icons.h"

#include "arc.h"
#include "atlas.h"
#include "config.h"
#include "keycaps.h"
#include "log.h"
#include "slots.h"

#include <windows.h>
#include <string.h>

namespace {

const char* kAtlasEntry = "Font\\IconFont\\btn_icon_pcb_00_ID_HQ";
const wchar_t* kArcTail = L"nativePCx64\\RZZC\\romPC\\IconFont.arc";
const wchar_t* kArcDirTail = L"nativePCx64\\RZZC\\romPC";

// TEX is a 24-byte header followed by raw BC3 blocks.
const unsigned kTexHeaderSize = 24;

// Bumped whenever the composer changes shape, so old stamps stop matching.
const unsigned kStampVersion = 1;
const unsigned kStampMagic = 0x4B524B53;   // KRKS

struct Stamp {
    unsigned magic;
    unsigned version;
    unsigned hash;
};

void Join(wchar_t* out, const wchar_t* dir, const wchar_t* leaf) {
    wcscpy_s(out, MAX_PATH, dir);
    wcscat_s(out, MAX_PATH, leaf);
}

// The mod lives in <game>\mods\<name>, so the game root is two levels up.
bool GameRoot(const wchar_t* modDir, wchar_t* out) {
    wcscpy_s(out, MAX_PATH, modDir);
    size_t n = wcslen(out);
    if (n && out[n - 1] == L'\\')
        out[n - 1] = 0;
    for (int i = 0; i < 2; ++i) {
        wchar_t* slash = wcsrchr(out, L'\\');
        if (!slash)
            return false;
        *slash = 0;
    }
    wcscat_s(out, MAX_PATH, L"\\");
    return true;
}

void EnsureDir(const wchar_t* path) {
    wchar_t tmp[MAX_PATH];
    wcscpy_s(tmp, MAX_PATH, path);
    for (wchar_t* p = tmp + 1; *p; ++p) {
        if (*p != L'\\')
            continue;
        *p = 0;
        CreateDirectoryW(tmp, nullptr);
        *p = L'\\';
    }
    CreateDirectoryW(tmp, nullptr);
}

unsigned FileSizeOf(const wchar_t* path) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path, GetFileExInfoStandard, &fad))
        return 0;
    return fad.nFileSizeLow;
}

unsigned Fnv(unsigned h, const void* data, unsigned len) {
    const unsigned char* p = (const unsigned char*)data;
    for (unsigned i = 0; i < len; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    return h;
}

bool StampMatches(const wchar_t* path, unsigned hash) {
    HANDLE fh = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fh == INVALID_HANDLE_VALUE)
        return false;
    Stamp s = {};
    DWORD got = 0;
    bool ok = ReadFile(fh, &s, sizeof(s), &got, nullptr) && got == sizeof(s);
    CloseHandle(fh);
    return ok && s.magic == kStampMagic && s.version == kStampVersion &&
           s.hash == hash;
}

void StampWrite(const wchar_t* path, unsigned hash) {
    HANDLE fh = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fh == INVALID_HANDLE_VALUE)
        return;
    Stamp s = { kStampMagic, kStampVersion, hash };
    DWORD put = 0;
    WriteFile(fh, &s, sizeof(s), &put, nullptr);
    CloseHandle(fh);
}

}  // namespace

bool IconsBuild(const wchar_t* modDir, const Config& cfg) {
    wchar_t libPath[MAX_PATH], stampPath[MAX_PATH], outDir[MAX_PATH];
    wchar_t outPath[MAX_PATH], gameRoot[MAX_PATH], srcPath[MAX_PATH];

    Join(libPath, modDir, L"assets\\keycaps.bin");
    Join(stampPath, modDir, L"atlas.stamp");
    Join(outDir, modDir, kArcDirTail);
    Join(outPath, modDir, kArcTail);
    if (!GameRoot(modDir, gameRoot)) {
        Log("icons: cannot locate the game folder from %S", modDir);
        return false;
    }
    Join(srcPath, gameRoot, kArcTail);

    unsigned libSize = FileSizeOf(libPath);
    unsigned srcSize = FileSizeOf(srcPath);
    if (!libSize) {
        Log("icons: assets keycaps.bin is missing, icons left alone");
        return false;
    }
    if (!srcSize) {
        Log("icons: cannot read %S", srcPath);
        return false;
    }

    unsigned hash = Fnv(2166136261u, cfg.bind, sizeof(cfg.bind));
    hash = Fnv(hash, &libSize, sizeof(libSize));
    hash = Fnv(hash, &srcSize, sizeof(srcSize));
    if (StampMatches(stampPath, hash) && FileSizeOf(outPath)) {
        Log("icons: already match the current bindings");
        return true;
    }

    Keycaps kc;
    if (!KeycapsLoad(libPath, kc)) {
        Log("icons: keycaps.bin is not a keycap library");
        return false;
    }

    Archive arc;
    if (!ArcLoad(srcPath, arc)) {
        KeycapsFree(kc);
        Log("icons: %S is not readable as an ARC", srcPath);
        return false;
    }

    bool ok = false;
    int index = ArcFind(arc, kAtlasEntry);
    if (index < 0) {
        Log("icons: %s is not in the archive", kAtlasEntry);
    } else {
        ArcEntry& tex = arc.entries[index];
        unsigned expect = kTexHeaderSize + kAtlasSide * kAtlasSide;
        if (tex.size != expect || memcmp(tex.data, "TEX", 4) != 0) {
            Log("icons: unexpected atlas texture (%u bytes, wanted %u)",
                tex.size, expect);
        } else {
            int cells = AtlasCompose(kc, cfg.bind, tex.data + kTexHeaderSize,
                                     tex.size - kTexHeaderSize);
            EnsureDir(outDir);
            ok = cells > 0 && ArcSave(outPath, arc);
            if (ok) {
                Log("icons: redrew %d cells, wrote %S", cells, outPath);
                StampWrite(stampPath, hash);
            } else {
                Log("icons: failed to write %S", outPath);
            }
        }
    }

    ArcFree(arc);
    KeycapsFree(kc);
    return ok;
}
