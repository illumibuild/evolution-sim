## Changelog

### v0.2.0 - 04/04/2026

#### Added:

- Tiles' energy values now increase by 1 every 10 generations.

- Added a new death rule so that when a cell dies, the energy on the tile below it increases by the age of the dead cell.

- Added the cell evolution system and three evolutions - motility, polydivision and energosynthesis.

- Added regressive evolution, so that a cell has a chance to lose an evolution every generation it doesn't utilize that evolution.

- Added new passive twitching animations for cells.

- Added a semi-transparent pointer that now follows the mouse instead of the old pointer, which now works differently.

- Added a percentage display on the world generation screen.

#### Changed:

- The living cost is now calculated as $ceil(age / 100)$ for every cell, making survival more demanding the older a cell is.

- Modified reproduction rules so that one of the cells retains the mother cell's age post-division.

- Refactored the internal advancement flow for a more balanced simulation.

- Completely reimplemented the internal cell event managing system for improved efficiency.

- Reworked the pointer so that it no longer follows the mouse cursor, but is instead fixed onto a tile using left click, and will follow the cell on that tile until it dies, allowing easier tracking.

- Changed the default speed from 1x to 2x.

- Revamped all sprites.

- Revamped the quit icon.

- Made the icons and the control panel smaller.

- Slightly changed the formatting of the coordinate display.

- Performed many memory optimizations.

- Plus many other small tweaks...

#### Fixed:

- Fixed a long-term bug where a cell would reproduce as soon as it had enough energy to do so, contradicting the written rules.

- Fixed zooming bugs.

- Fixed poor icon rendering quality.

- Fixed a long-term bug that caused random flickering and missed rendering frames.

- Optimized animation frame calculation.

- Plus many other small corrections...

### v0.1.0 - 01/17/2026
