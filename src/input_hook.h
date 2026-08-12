#pragma once

struct Config;

// Redirects the game's keyboard reads through the player's bindings.
// The config must stay alive for as long as the hook is installed.
bool InputHookInstall(const Config* cfg);

// Re-reads the config after the binding GUI has changed it.
void InputHookRefresh();
