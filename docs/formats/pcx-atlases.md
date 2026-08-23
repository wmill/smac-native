# Indexed PCX and atlas evidence

SMACX `ter1.pcx` and `Units.pcx` are 1024×768, one-plane, eight-bit PCX files with RLE scanlines and
a 256-entry RGB palette. The native reader retains palette indices and all 256 colors rather than
asking a true-color decoder to flatten them. It accepts only this bounded layout, validates runs and
row padding, and reports structured errors for malformed data.

The stock sheets use cyan one-pixel cell guides; atlas rectangles in `smac/formats/atlas.hpp` exclude
those guides. Terrain cells are 100×62 with a 101-pixel horizontal or 63-pixel vertical stride. Unit
cells are 100×76 with a 102-pixel horizontal stride and 78-pixel row stride. Animation variants are
represented as a base rectangle, frame count, and stride rather than repeated rendering constants.

Transparency is palette-index based, not an RGB comparison. In the observed stock SMACX sheets,
`ter1.pcx` uses index 253 for its empty background and index 254 for cyan guides, while `Units.pcx`
uses index 255 for its empty background. The palette RGB values are preserved even for transparent
pixels because special palette ramps and user-modified sheets must remain distinguishable.

The current declarative regions cover the M1 terrain bases, fungus, rockiness, resources,
improvements, pods, and the debug native-life unit frames. Coordinates were checked against the
user-owned GOG SMACX sheets; no original pixels are present in the repository. Regions for later
systems will be added only when those systems become renderable.
