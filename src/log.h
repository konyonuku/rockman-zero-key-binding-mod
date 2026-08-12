#pragma once

// Small append-only log next to the DLL. The loader has a console but it is
// off by default, and the interesting failures happen before the game window.
void LogInit(const wchar_t* path);
void Log(const char* fmt, ...);
