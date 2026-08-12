#include "config.h"
#include "keys.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

void ConfigDefaults(Config& cfg) {
    memset(cfg.bind, 0, sizeof(cfg.bind));
    for (int i = 0; i < kGameKeyCount; ++i)
        cfg.bind[kGameKeys[i]] = kGameKeys[i];
    cfg.showGui = true;
    cfg.diag = false;
}

static char* Trim(char* s) {
    while (*s == ' ' || *s == '\t') ++s;
    char* e = s + strlen(s);
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n'))
        *--e = 0;
    return s;
}

bool ConfigLoad(const wchar_t* path, Config& cfg) {
    ConfigDefaults(cfg);

    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path, L"rb") != 0 || !fp)
        return false;

    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char* s = Trim(line);
        if (!*s || *s == '#' || *s == '[')
            continue;

        char* eq = strchr(s, '=');
        if (!eq)
            continue;
        *eq = 0;
        char* key = Trim(s);
        char* val = Trim(eq + 1);

        if (_stricmp(key, "show_gui") == 0) {
            cfg.showGui = (_stricmp(val, "true") == 0 || strcmp(val, "1") == 0);
            continue;
        }
        if (_stricmp(key, "diag") == 0) {
            cfg.diag = (_stricmp(val, "true") == 0 || strcmp(val, "1") == 0);
            continue;
        }

        unsigned char game = KeyFromName(key);
        unsigned char phys = KeyFromName(val);
        if (game && phys && IsGameKey(game))
            cfg.bind[game] = phys;
    }
    fclose(fp);
    return true;
}

bool ConfigSave(const wchar_t* path, const Config& cfg) {
    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path, L"wb") != 0 || !fp)
        return false;

    fprintf(fp, "# MZZXLC KeyRebind\n");
    fprintf(fp, "# Left side is the stock Layout A key, right side is the key you press.\n");
    fprintf(fp, "show_gui = %s\n\n", cfg.showGui ? "true" : "false");
    fprintf(fp, "[bindings]\n");
    for (int i = 0; i < kGameKeyCount; ++i) {
        unsigned char game = kGameKeys[i];
        const char* gameName = NameFromKey(game);
        const char* physName = NameFromKey(cfg.bind[game]);
        if (gameName && physName)
            fprintf(fp, "%-8s = %s\n", gameName, physName);
    }
    fclose(fp);
    return true;
}

bool ConfigValidate(const Config& cfg, unsigned char* clashOut) {
    bool used[256] = {};
    for (int i = 0; i < kGameKeyCount; ++i) {
        unsigned char phys = cfg.bind[kGameKeys[i]];
        if (!phys)
            continue;
        if (used[phys]) {
            if (clashOut) *clashOut = phys;
            return false;
        }
        used[phys] = true;
    }
    return true;
}
