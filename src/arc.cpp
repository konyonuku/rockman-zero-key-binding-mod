#include "arc.h"

#include "third_party/miniz.h"

#include <windows.h>
#include <stdlib.h>
#include <string.h>

namespace {

const unsigned kHeaderSize = 8;
const unsigned kEntrySize = 80;
const unsigned kFirstDataOffset = 0x8000;

unsigned Read32(const unsigned char* p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) |
           ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}

void Write32(unsigned char* p, unsigned v) {
    p[0] = (unsigned char)(v);
    p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16);
    p[3] = (unsigned char)(v >> 24);
}

unsigned char* ReadWholeFile(const wchar_t* path, unsigned& sizeOut) {
    HANDLE fh = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                            OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fh == INVALID_HANDLE_VALUE)
        return nullptr;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(fh, &sz) || sz.QuadPart < kHeaderSize ||
        sz.QuadPart > 256 * 1024 * 1024) {
        CloseHandle(fh);
        return nullptr;
    }
    unsigned size = (unsigned)sz.QuadPart;
    unsigned char* buf = (unsigned char*)malloc(size);
    DWORD got = 0;
    if (!buf || !ReadFile(fh, buf, size, &got, nullptr) || got != size) {
        CloseHandle(fh);
        free(buf);
        return nullptr;
    }
    CloseHandle(fh);
    sizeOut = size;
    return buf;
}

bool WriteWholeFile(const wchar_t* path, const unsigned char* data, unsigned size) {
    HANDLE fh = CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
    if (fh == INVALID_HANDLE_VALUE)
        return false;
    DWORD put = 0;
    bool ok = WriteFile(fh, data, size, &put, nullptr) && put == size;
    CloseHandle(fh);
    return ok;
}

}  // namespace

bool ArcLoad(const wchar_t* path, Archive& arc) {
    memset(&arc, 0, sizeof(arc));

    unsigned size = 0;
    unsigned char* raw = ReadWholeFile(path, size);
    if (!raw)
        return false;
    if (memcmp(raw, "ARC\0", 4) != 0) {
        free(raw);
        return false;
    }

    arc.version = (unsigned short)(raw[4] | (raw[5] << 8));
    arc.count = raw[6] | (raw[7] << 8);
    if (arc.count <= 0 || kHeaderSize + (unsigned)arc.count * kEntrySize > size) {
        free(raw);
        return false;
    }

    arc.entries = (ArcEntry*)calloc(arc.count, sizeof(ArcEntry));
    if (!arc.entries) {
        free(raw);
        return false;
    }

    bool ok = true;
    for (int i = 0; i < arc.count && ok; ++i) {
        const unsigned char* e = raw + kHeaderSize + (unsigned)i * kEntrySize;
        ArcEntry& out = arc.entries[i];
        memcpy(out.name, e, 64);
        out.name[63] = 0;
        out.extHash = Read32(e + 64);
        unsigned comp = Read32(e + 68);
        unsigned packed = Read32(e + 72);
        unsigned offset = Read32(e + 76);
        out.size = packed & 0x1FFFFFFF;
        out.flags = packed >> 29;

        if ((unsigned long long)offset + comp > size || !out.size) {
            ok = false;
            break;
        }
        out.data = (unsigned char*)malloc(out.size);
        if (!out.data) {
            ok = false;
            break;
        }
        if (out.flags == 2) {
            mz_ulong got = out.size;
            ok = mz_uncompress(out.data, &got, raw + offset, comp) == MZ_OK &&
                 got == out.size;
        } else {
            ok = comp == out.size;
            if (ok)
                memcpy(out.data, raw + offset, out.size);
        }
    }

    free(raw);
    if (!ok)
        ArcFree(arc);
    return ok;
}

void ArcFree(Archive& arc) {
    for (int i = 0; i < arc.count; ++i)
        free(arc.entries[i].data);
    free(arc.entries);
    memset(&arc, 0, sizeof(arc));
}

int ArcFind(const Archive& arc, const char* name) {
    for (int i = 0; i < arc.count; ++i) {
        if (strcmp(arc.entries[i].name, name) == 0)
            return i;
    }
    return -1;
}

bool ArcSave(const wchar_t* path, const Archive& arc) {
    unsigned char** comp = (unsigned char**)calloc(arc.count, sizeof(unsigned char*));
    unsigned* compSize = (unsigned*)calloc(arc.count, sizeof(unsigned));
    if (!comp || !compSize) {
        free(comp);
        free(compSize);
        return false;
    }

    bool ok = true;
    unsigned total = kFirstDataOffset;
    for (int i = 0; i < arc.count && ok; ++i) {
        const ArcEntry& e = arc.entries[i];
        if (e.flags == 2) {
            mz_ulong bound = mz_compressBound(e.size);
            comp[i] = (unsigned char*)malloc(bound);
            if (!comp[i]) {
                ok = false;
                break;
            }
            mz_ulong got = bound;
            ok = mz_compress2(comp[i], &got, e.data, e.size, MZ_BEST_COMPRESSION) == MZ_OK;
            compSize[i] = (unsigned)got;
        } else {
            comp[i] = (unsigned char*)malloc(e.size);
            if (!comp[i]) {
                ok = false;
                break;
            }
            memcpy(comp[i], e.data, e.size);
            compSize[i] = e.size;
        }
        total += compSize[i];
    }

    unsigned char* out = nullptr;
    if (ok) {
        if (kHeaderSize + (unsigned)arc.count * kEntrySize > kFirstDataOffset)
            ok = false;
    }
    if (ok) {
        out = (unsigned char*)calloc(total, 1);
        ok = out != nullptr;
    }
    if (ok) {
        memcpy(out, "ARC\0", 4);
        out[4] = (unsigned char)(arc.version);
        out[5] = (unsigned char)(arc.version >> 8);
        out[6] = (unsigned char)(arc.count);
        out[7] = (unsigned char)(arc.count >> 8);

        unsigned offset = kFirstDataOffset;
        for (int i = 0; i < arc.count; ++i) {
            const ArcEntry& e = arc.entries[i];
            unsigned char* rec = out + kHeaderSize + (unsigned)i * kEntrySize;
            memcpy(rec, e.name, 64);
            Write32(rec + 64, e.extHash);
            Write32(rec + 68, compSize[i]);
            Write32(rec + 72, (e.size & 0x1FFFFFFF) | (e.flags << 29));
            Write32(rec + 76, offset);
            memcpy(out + offset, comp[i], compSize[i]);
            offset += compSize[i];
        }
        ok = WriteWholeFile(path, out, total);
    }

    for (int i = 0; i < arc.count; ++i)
        free(comp[i]);
    free(comp);
    free(compSize);
    free(out);
    return ok;
}
