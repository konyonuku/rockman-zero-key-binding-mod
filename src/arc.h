#pragma once

// MT Framework ARC container, the form IconFont.arc takes.
//
//   "ARC\0", u16 version, u16 file count, then 80-byte entries:
//     char name[64]     backslash separated path, no extension
//     u32 ext_hash      jamcrc of the extension
//     u32 comp_size
//     u32 packed        low 29 bits original size, high 3 bits flags (2 = zlib)
//     u32 offset
//   File data starts at 0x8000 and is packed back to back.

struct ArcEntry {
    char name[64];
    unsigned extHash;
    unsigned flags;
    unsigned char* data;      // uncompressed contents, owned by the Archive
    unsigned size;
};

// Named Archive rather than Arc: windows.h declares a GDI function called Arc,
// which would hide the type.
struct Archive {
    unsigned short version;
    int count;
    ArcEntry* entries;
};

bool ArcLoad(const wchar_t* path, Archive& arc);
void ArcFree(Archive& arc);
int ArcFind(const Archive& arc, const char* name);   // -1 when absent
bool ArcSave(const wchar_t* path, const Archive& arc);
