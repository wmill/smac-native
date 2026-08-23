# GLSMAC evidence ledger

GLSMAC is an independent AGPL-3.0 SMAC reimplementation. M1 used its source as corroborating
documentation for facts visible in the user-owned stock sheets; no renderer implementation was
copied.

| Native component | Evidence | Treatment |
|---|---|---|
| `texture.pcx` region coordinates | `src/game/backend/map/Consts.h` | Coordinate facts independently checked against the stock sheet and recorded as declarative metadata |
| terrain layer order and eight-direction links | `src/game/backend/map/module/LandSurface.cpp` | Used as corroboration for sheet semantics; reimplemented for SDL's 2D renderer and owned `WorldMap` |
| `ter1.pcx` object regions | `src/game/backend/map/Consts.h` | Coordinate facts independently checked against the stock sheet; no pixels distributed |

This ledger does not make GLSMAC the behavioral baseline. OpenSMACX and the identified GOG SMACX
executable remain the rules evidence defined by the project roadmap.
