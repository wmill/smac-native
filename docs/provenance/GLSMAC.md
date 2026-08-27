# GLSMAC evidence ledger

GLSMAC is an independent AGPL-3.0 SMAC reimplementation. M1 used its source as corroborating
documentation for facts visible in the user-owned stock sheets; no renderer implementation was
copied.

| Native component | Evidence | Treatment |
|---|---|---|
| `texture.pcx` region coordinates | `src/game/backend/map/Consts.h` | Coordinate facts independently checked against the stock sheet and recorded as declarative metadata |
| terrain layer order and eight-direction links | `src/game/backend/map/module/LandSurface.cpp` | Used as corroboration for sheet semantics; reimplemented for SDL's 2D renderer and owned `WorldMap` |
| five-vertex ground mesh and planar water | `CalculateCoords.cpp`, `Prepare.cpp`, and `Finalize.cpp` | Reimplemented independently in screen-space SDL geometry; stock center contours are relaxed to GLSMAC's 650-unit maximum neighbor slope, averaged into shared corners, and pinned to sea level at coasts |
| smooth terrain lighting and ocean-depth tint | `types/mesh/Render.cpp`, `WaterSurface.cpp`, and the orthographic shader | Normal accumulation, the two-light model, and depth ramps were translated to SDL vertex colors without copying renderer code |
| `ter1.pcx` object regions | `src/game/backend/map/Consts.h` | Coordinate facts independently checked against the stock sheet; no pixels distributed |

This ledger does not make GLSMAC the behavioral baseline. OpenSMACX and the identified GOG SMACX
executable remain the rules evidence defined by the project roadmap.
