# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

SMAC Native is a clean-room native reimplementation of Sid Meier's Alpha Centauri: Alien Crossfire
(macOS/Linux). It contains no Firaxis assets — the tool only reads a user-supplied, extracted game
installation. It is early: a deterministic C++20 simulation core, bounded rules/map readers, asset
validation, a reverse-engineering CLI, and an optional SDL3 map client. See `ROADMAP.md` for the full
milestone plan (M0–M10) and what is/isn't implemented yet — check it before assuming a system (combat,
economy, AI, save format, etc.) exists.

## Build, test, run

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

- `build/dev/smac-tool verify-data --data-dir /path/to/smac_extracted`
- `build/dev/smac-tool dump-rules --data-dir /path/to/smac_extracted`
- `build/dev/smac-tool inspect-map /path/to/smac_extracted/maps/xplanet.MP`
- `build/dev/smac-native --data-dir /path/to/smac_extracted` (graphical client; falls back to a stub
  binary explaining how to enable it if SDL3 isn't found)

Before submitting changes, run the sanitizer preset (required by `CONTRIBUTING.md`):

```sh
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

There is one test binary (`tests/tests.cpp`, `smac_tests`) using a small in-house `CHECK` macro (no
external test framework) — add new checks there rather than creating new test files. Run a subset by
temporarily narrowing the file, or invoke the built binary directly: `build/dev/smac_tests`.

SDL3, SDL3_image, and SDL3_ttf are not downloaded by default — install them yourself, or pass
`-DSMAC_FETCH_DEPENDENCIES=ON` at configure time to fetch the pinned versions in `CMakeLists.txt`.
`SMAC_BUILD_CLIENT=OFF` skips the client target entirely.

Formatting follows `.clang-format` (LLVM-based, 100 cols, 4-space indent); there is no CI formatting
check yet (tracked in `ROADMAP.md` under M0).

## Architecture

Four CMake targets with a strict dependency direction: `smac_core` → `smac_formats` → {`smac-tool`,
`smac-native`}. `smac_core` must stay free of SDL types and original packed structures — this is
enforced by convention, not by the build, so don't add SDL or raw-binary-layout includes there.

- **`smac_core`** (`include/smac/core/`, `src/core/`) — the deterministic simulation: `WorldMap`
  (staggered coordinates, horizontal wrap, polar boundaries, neighbor lookup), `GameState` (owned units
  and turn state, mutated only via a `Command` → `Event` interface: `MoveUnit`/`EndTurn` in,
  `UnitMoved`/`TurnAdvanced`/`CommandRejected` out), and bounded Dijkstra pathfinding
  (`find_path`, budget-limited). `GameState::stable_hash()` is a deterministic hash of authoritative
  state, currently narrow — expand it whenever new authoritative state is added (see ROADMAP M0).
- **`smac_formats`** (`include/smac/formats/`, `src/formats/`) — bounded readers for original file
  formats, layered on `smac_core` types: `data_directory` (case-insensitive asset lookup + validation
  report), `rules` (line-based `alphax.txt` section parser, not yet a typed `RulesDatabase`), `text`
  (CP1252→UTF-8, CRLF normalization), `terran_map` (`.MP` binary reader → `WorldMap` via
  `to_world_map()`), `sha256` (baseline executable hashing). All parsers return
  `Result<T> = std::variant<T, Error>` (`smac/formats/result.hpp`) instead of throwing; `Error` carries
  a message and byte offset. Follow this pattern for any new binary/text reader, and pair it with
  truncated/malformed-input tests.
- **`smac-tool`** (`src/tool/main.cpp`) — headless CLI (`verify-data`, `dump-rules`, `inspect-map`) used
  for reverse-engineering and as a scriptable inspection surface; it's often the fastest way to
  exercise a new format reader without the graphical client.
- **`smac-native`** (`src/client/`) — SDL3 map client (`sdl_main.cpp`) when SDL3 is available, otherwise
  `stub_main.cpp`. Presentation code may consume `Event`s from `GameState` but must never become
  authoritative — game state mutation happens only through `Command`s.

### Format evidence and provenance

Binary/text format layouts are reverse-engineered from a user-owned GOG SMACX 2.0 install and from
OpenSMACX (GPL-3 reference source). Document format layout findings in `docs/formats/` (see
`docs/formats/terranmap.md` for the expected level of detail: byte offsets, field sizes, open
hypotheses) and log every OpenSMACX-derived adaptation in `docs/provenance/OPENSMACX.md`, stating
whether the native behavior is faithful to the baseline executable or a deliberate difference. Never
inherit an OpenSMACX bug fix silently — it must be a recorded, explicit choice.

### Hard rules (also in `CONTRIBUTING.md`)

- No original executable, art, audio, text, maps, fonts, or any data derived from them may be
  committed — tests use synthetic/generated fixtures only (see the fixture-building code at the top of
  `tests/tests.cpp` for the pattern: hand-built byte buffers, temp directories for case-insensitivity
  tests).
- `smac_core` stays deterministic: state changes only through `Command`/`Event`, no SDL or original
  packed types.
- New binary/text parsers need structured `Result<T>` errors and truncation/malformed-input tests.
