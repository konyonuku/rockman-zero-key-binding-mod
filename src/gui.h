#pragma once

struct Config;

// Shows the binding window and does not return until the player closes it.
// True when the settings were applied and are worth saving, false when the
// window was dismissed and the changes should be dropped.
//
// The same window serves both entry points: the mod calls it from mod_open
// while the game is still starting, and KeyRebindConfig.exe calls it on its
// own.
bool GuiRun(Config& cfg);
