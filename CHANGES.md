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
