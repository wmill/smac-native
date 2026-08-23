# Contributing

Use C++20, keep `smac_core` deterministic and free of SDL types, and return structured errors at all data boundaries. New binary parsing needs truncation and malformed-input tests. Never add proprietary game data.

Run `cmake --preset asan`, `cmake --build --preset asan`, and `ctest --preset asan` before submitting changes. Record recovered behavior or structures in `docs/provenance/OPENSMACX.md`; distinguish faithful behavior from chosen fixes.

