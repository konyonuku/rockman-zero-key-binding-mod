#pragma once

struct Keycaps;

// Redraws every cell Layout A uses, in place, inside a BC3 payload: one keycap
// per single cell, and the movement clusters and pairs rebuilt from their
// backdrop plus the lettering of whichever keys are bound to them. Cells the
// mod does not own keep their original compressed blocks untouched, which is
// possible because every cell is aligned to the 4x4 block grid.
//
// bind maps a Layout A key to the physical key the player presses.
// Returns how many cells were written.
int AtlasCompose(const Keycaps& kc, const unsigned char* bind,
                 unsigned char* bc3, unsigned bc3Size);
