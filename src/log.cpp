#include "log.h"

#include <windows.h>
#include <stdarg.h>
#include <stdio.h>

static wchar_t g_path[MAX_PATH];

void LogInit(const wchar_t* path) {
    wcscpy_s(g_path, path);
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, g_path, L"wb") == 0 && fp) {
        SYSTEMTIME st;
        GetLocalTime(&st);
        fprintf(fp, "KeyRebind log - %04d-%02d-%02d %02d:%02d:%02d\n",
                st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        fclose(fp);
    }
}

void Log(const char* fmt, ...) {
    if (!g_path[0])
        return;
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, g_path, L"ab") != 0 || !fp)
        return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(fp, fmt, ap);
    va_end(ap);
    fputc('\n', fp);
    fclose(fp);
}
