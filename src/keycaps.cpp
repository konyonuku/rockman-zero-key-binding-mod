#include "keycaps.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>

namespace {

const unsigned kHeaderSize = 24;
const unsigned kRecordSize = 16;
const unsigned kBackdropSize = 64 * 64 * 2;

unsigned Read32(const unsigned char* p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

unsigned short Read16(const unsigned char* p) {
    return (unsigned short)(p[0] | (p[1] << 8));
}

}  // namespace

bool KeycapsLoad(const wchar_t* path, Keycaps& kc) {
    memset(&kc, 0, sizeof(kc));

    HANDLE fh = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fh == INVALID_HANDLE_VALUE)
        return false;

    LARGE_INTEGER sz;
    if (!GetFileSizeEx(fh, &sz) || sz.QuadPart < kHeaderSize ||
        sz.QuadPart > 64 * 1024 * 1024) {
        CloseHandle(fh);
        return false;
    }

    unsigned size = (unsigned)sz.QuadPart;
    unsigned char* buf = (unsigned char*)malloc(size);
    DWORD got = 0;
    if (!buf || !ReadFile(fh, buf, size, &got, nullptr) || got != size) {
        CloseHandle(fh);
        free(buf);
        return false;
    }
    CloseHandle(fh);

    if (memcmp(buf, "KRKC", 4) != 0 || Read32(buf + 4) != 2) {
        free(buf);
        return false;
    }

    kc.data = buf;
    kc.size = size;
    kc.keyCount = Read32(buf + 8);
    kc.groupCount = Read32(buf + 12);
    kc.templateCount = Read32(buf + 16);
    unsigned blobSize = Read32(buf + 20);

    kc.templateOffsets = buf + kHeaderSize;
    unsigned recOff = kHeaderSize + kc.templateCount * 4;
    unsigned recBytes = kc.groupCount * kc.keyCount * kRecordSize;
    if (recOff + recBytes + blobSize != size) {
        KeycapsFree(kc);
        return false;
    }
    kc.records = buf + recOff;
    kc.blob = buf + recOff + recBytes;
    return true;
}

void KeycapsFree(Keycaps& kc) {
    free(kc.data);
    memset(&kc, 0, sizeof(kc));
}

bool KeycapsTile(const Keycaps& kc, int group, unsigned char dik, KeycapTile& out) {
    if (!kc.data || group < 0 || (unsigned)group >= kc.groupCount)
        return false;
    const unsigned char* rec = kc.records + (unsigned)group * kc.keyCount * kRecordSize;
    for (unsigned i = 0; i < kc.keyCount; ++i, rec += kRecordSize) {
        if (Read16(rec) != dik)
            continue;
        unsigned length = Read32(rec + 12);
        if (!length)
            return false;
        out.x = rec[2];
        out.y = rec[3];
        out.w = rec[4];
        out.h = rec[5];
        out.pixels = kc.blob + Read32(rec + 8);
        return true;
    }
    return false;
}

const unsigned char* KeycapsBackdrop(const Keycaps& kc, int kind) {
    if (!kc.data || kind < 0 || (unsigned)kind >= kc.templateCount)
        return nullptr;
    unsigned off = Read32(kc.templateOffsets + kind * 4);
    if (kc.blob + off + kBackdropSize > kc.data + kc.size)
        return nullptr;
    return kc.blob + off;
}
