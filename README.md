# SMAC Native

SMAC Native is an early, clean native reimplementation of Sid Meier's Alpha Centauri: Alien Crossfire for macOS and Linux. It contains no Firaxis assets; you must point it at your own extracted installation.

The first slice provides a deterministic C++20 simulation core, bounded rules/map readers, asset validation, a reverse-engineering CLI, and an optional SDL3 map client.

See [ROADMAP.md](ROADMAP.md) for the current implementation status and the milestone path to a
complete playable game.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
cmake --build --preset dev --target format-check
build/dev/smac-tool verify-data --data-dir /path/to/smac_extracted
build/dev/smac-tool inspect-map /path/to/smac_extracted/maps/xplanet.MP
build/dev/smac-native --data-dir /path/to/smac_extracted
build/dev/smac-native --data-dir /path/to/smac_extracted --acceptance-check
build/dev/smac-native --data-dir /path/to/smac_extracted --benchmark-frames 120
```

SDL is deliberately not downloaded by the default configure. Install SDL3, SDL3_image, and SDL3_ttf, or opt in with `-DSMAC_FETCH_DEPENDENCIES=ON`. Dependency versions are pinned in `CMakeLists.txt`.

Deterministic command logs use the documented native [replay format](docs/formats/replay.md). CI
builds the headless and SDL configurations, checks formatting, renders and validates a synthetic
indexed-map screenshot, and runs sanitized reader fuzzing.

Only user-owned data is loaded at runtime. Do not commit or redistribute original executables, text, maps, fonts, art, or audio. Tests use generated synthetic inputs.

Copyright 2026 SMAC Native contributors. Licensed under GPL-3.0-or-later; see `LICENSE`.
