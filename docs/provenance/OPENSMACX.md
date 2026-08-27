# OpenSMACX provenance ledger

| Native component | Recovered evidence | Treatment |
|---|---|---|
| staggered indexing and horizontal wrap | `OpenSMACX/src/map.cpp` (`map_loc`, `xrange`, `on_map`) and `map.h` (`RadiusOffset[1..8]`) | Re-expressed with owned `WorldMap`; all eight adjacent directions retained, with no addresses or globals |
| map tile fields and flags | `OpenSMACX/src/map.h`, GPL-3 | Re-expressed as fixed-width decoded fields while retaining raw bytes |
| climate, rainfall, temperature, and rockiness masks | `OpenSMACX/src/map.h` and `map.cpp` (`climate_at`, `rocky_at`) | Faithful bit decoding; unknown enum values remain representable |
| ocean boundary | `OpenSMACX/src/map.cpp` (`is_ocean`) | Faithful: altitude below shoreline (`0x60`) is ocean |
| TERRANMAP header extent | `map_read` and `map_write` use 2,724 bytes | Combined with observed 15-byte on-disk envelope; deliberate bounded validation added |
| map sea level, flat flag, and detailed elevation | contiguous map globals plus `elev_at` at original offset `0x5919C0` | Header offsets and bounded contour-to-elevation calculation retained; shared render corners are a native reconstruction |
| movement cost | `OpenSMACX/src/veh.cpp` (`hex_cost`, original offset `0x593510`) | Cost ordering and road-rate units retained; unit-aware owned traits replace prototype/global lookups |
| movement legality | `OpenSMACX/src/path.cpp`, `temp.cpp` ZOC helpers | Domain, occupancy, transport, wrapping, poles, and a conservative enemy-ZOC rule expressed without globals |
| movement command | movement concepts in `veh.cpp`/`path.cpp` | New command/event interface; combat destinations are deliberately rejected in M1 |
| pathfinding | recovered `Path` class | New deterministic Dijkstra implementation sharing the command legality evaluator, not a line port |

SMAC Native does not inherit OpenSMACX bug fixes silently. Every future behavioral port must add a row with source function/offset and state whether it matches the baseline GOG executable or deliberately differs.
