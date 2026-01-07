# Evolution Simulation Cellular Automata

A simple simulation using a cellular automata system designed to explore evolution caused by emergent behaviors.

## Features

- Tile-based world with cells representing **organisms**

- Each cell has **age** and **energy**

- Simple rules for **energy harvesting**, **reproduction** and **death**

- Visualizes evolution over time with **step-by-step updates**

## Getting started

### Dependencies

- [SDL](https://github.com/libsdl-org/SDL) and [SDL_image](https://github.com/libsdl-org/SDL_image) for graphics - SDL2

- [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) for the GUI

- [Microsoft Visual C++ Runtime](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist) (only if you're on Windows)

### Build

Linux (GCC):

```
gcc -std=c17 -Wall -O2 main.c -o evolution-sim -I<SDL2 path> -I<Nuklear path> -L<libs path> \
-lSDL2 -lSDL2_image -lm
```

Windows (MSVC):

```
cl /std:c17 /W4 /O2 main.c /Fe:evolution-sim.exe /I"<SDL2 path>" /I"<SDL2_image path>" ^
/I"<Nuklear path>" /link Shell32.lib /LIBPATH:"<SDL2 path>" SDL2main.lib SDL2.lib ^
/LIBPATH:"<SDL2_image path>" SDL2_image.lib /SUBSYSTEM:WINDOWS
```

## Controls

- **Right click drag** - camera panning

- **Mouse wheel** - zoom

## Rules

### Basics

The world is a **tilemap**, where each tile can house a cell. Each cell has two basic properties: **age** and **energy**. Energy is the foundation of all life. When a cell runs out of energy, it **dies**.

### Generation

At world generation, each tile is assigned a random energy source value, ranging from 0 to 75.
The possible energy source values' probabilities are **weighted** so that **lower values** are **more likely**.
Each tile also has a **10% (1 in 10) chance** to **house a live cell**.
Each initial live cell starts with a **random** amount of energy, ranging from 5 to 10.

### Advancement

Every generation, a live cell's age **increases** by 1.

#### Energy harvesting

If a live cell is on a tile with **available energy**, it will **harvest** up to 5 energy, with the harvested amount of energy being **based on the following rules**:

- if the available energy is **100 or greater**, the cell harvests **3 energy**

- otherwise, if the available energy is **50 or greater**, the cell harvests **2 energy**

- otherwise, the cell harvests **1 energy**

- if the cell is **at least 40 generations old**, it harvests an additional **2 energy**

- otherwise, if the cell is **at least 20 generations old**, it harvests an additional **1 energy**

- otherwise, the cell **doesn't harvest any additional energy**

Of course, the actual harvested amount is limited by how much energy is **available**, and a cell **can't harvest more energy than the tile below it can provide**.

#### Living cost

Every live cell's energy **decreases** by 1.
As already mentioned above, if the cell's energy reaches 0 after this, **it dies**.

#### Reproduction

Every cell that has **at least 10 energy**, is **at least 10 generations old** and has **at least 1 free tile** out of its **4 adjacent tiles** has a **25% (1 in 4) chance** to divide itself into two cells, **splitting the mother cell's energy equally**, and **losing a unit of energy if the mother cell's energy is odd**.
When a cell divides itself, one daughter cell **stays on the same tile** as the mother cell, while the other is born on **the most energy-rich free tile out of the 4 adjacent tiles** to the original mother cell.
Both new cells start at **0 age**.

## Current development state

I'm working on this project solo.
I was inspired by Conway's game of life, and decided to make something more complex and "alive", less order-based and more chaos-based.
The rendering engine is written in C with SDL2 and SDL2_image for graphics and Nuklear for GUI.
It currently lacks a mipmap system, so zooming out too far causes performance issues.

## Future plans

- better rendering optimization and mipmap implementation

- more control and more simulation parameters

- evolution paths and more sophisticated and complex cell behavior, including movement, parasitism, symbiosis, self-sustenance and energy production, etc.

- mutations and cooperation between cells with the same mutation, as well as aggression towards cells lacking the mutation

- option to save simulation state

- cleaner UI
