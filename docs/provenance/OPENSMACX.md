# OpenSMACX provenance ledger

| Native component | Recovered evidence | Treatment |
|---|---|---|
| staggered indexing and horizontal wrap | `OpenSMACX/src/map.cpp` (`map_loc`, `xrange`, `on_map`) | Re-expressed with owned `WorldMap`; no addresses or globals |
| map tile fields and flags | `OpenSMACX/src/map.h`, GPL-3 | Re-expressed as fixed-width decoded fields while retaining raw bytes |
| climate, rainfall, temperature, and rockiness masks | `OpenSMACX/src/map.h` and `map.cpp` (`climate_at`, `rocky_at`) | Faithful bit decoding; unknown enum values remain representable |
| ocean boundary | `OpenSMACX/src/map.cpp` (`is_ocean`) | Faithful: altitude below shoreline (`0x60`) is ocean |
| TERRANMAP header extent | `map_read` and `map_write` use 2,724 bytes | Combined with observed 15-byte on-disk envelope; deliberate bounded validation added |
| movement command | movement concepts in `veh.cpp`/`path.cpp` | New command/event interface; first slice intentionally implements adjacent land movement only |
| pathfinding | recovered `Path` class | New deterministic Dijkstra implementation, not a line port |

SMAC Native does not inherit OpenSMACX bug fixes silently. Every future behavioral port must add a row with source function/offset and state whether it matches the baseline GOG executable or deliberately differs.
