## Changelog

All notable changes to this project will be documented here.

### v0.1.0 alpha 1 - 01/07/2026

### v0.1.0 alpha 2 - 01/11/2026

#### Added:

- Added current zoom level display.

- Added the ability to change simulation speed.

#### Changed:

- Optimized the program to work on 32-bit machines by removing usage of 64-bit values, which limits capability on 64-bit machines only theoretically, and not functionally, while allowing support for older or less powerful devices.

- Reworked zooming and camera panning to use integers as much as possible to prevent bugs regarding floating-point precision.

- Reduced the minimum window size from 1280x720 to 853x480, effectively enabling usage with very small displays.

- Plus many other small tweaks...

#### Fixed:

- Fixed a bug where generating a larger world after generating a smaller one first would leave a large portion of the new, larger world completely ungenerated, because of how the world generator's state was saved.

- Fixed minor issues regarding zooming.

- Plus many other small corrections...

### v0.1.0 alpha 3 - 01/15/2026

#### Added:

- Added the ability to see the coords and energy of the tile the mouse is hovering over, as well as the age and energy of the cell on it, if there is one currently alive on the tile.

#### Changed:

- Reworked texture rendering.

- Plus many other small tweaks...

#### Fixed:

- Many small corrections...

### v0.1.0 beta 1 - 01/16/2026

#### Changed:

- Switched from modified XorShift32 to SplitMix32 PRNG for better pseudorandomness.

#### Fixed:

- Added additional cleanups before the program quits with an error.

- Plus many other small corrections...

### v0.1.0 beta 2 - 01/17/2026

#### Fixed:

- Fixed zooming bugs.

- Plus many other small corrections...

### v0.1.0 beta 3 - 01/17/2026

#### Changed:

- Revamped icons.

- Switched to a new texture atlas system for better space optimization.

#### Fixed:

- Fixed a long-term bug with world generation, where the chance for a live cell to spawn on a tile was 2% for each tile, even though it was supposed to be 10% according to the official written rule.

### v0.1.0 - 01/17/2026

### v0.2.0 alpha 1 - 01/25/2026

#### Added:

- Added a new death rule so that when a cell dies, the energy on the tile below it increases by the age of the dead cell.

- Added the cell evolution system and the first evolution - motility, the ability to move.

- Added new passive twitching animations for cells.

#### Changed:

- Modified reproduction rules so that one of the cells retains the mother cell's age post-division.

- Revamped all sprites.

- Revamped the quit icon.

- Changed the default speed from 1x to 2x.

- Refactored the internal advancement flow for a more balanced simulation.

- Completely reimplemented the internal cell event managing system for improved efficiency.

- Performed many memory optimizations.

- Rewrote many code components better and more optimized.

- Plus many other small tweaks...

#### Fixed:

- Fixed a long-term bug where a cell would reproduce as soon as it had enough energy to do so, contradicting the written rules.

- Fixed more zooming bugs.

- Plus many other small corrections...
