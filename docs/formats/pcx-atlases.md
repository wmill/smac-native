# Indexed PCX and atlas evidence

SMACX `ter1.pcx`, `texture.pcx`, and `Units.pcx` are 1024×768, one-plane, eight-bit PCX files with RLE
scanlines and a 256-entry RGB palette. The native reader retains palette indices and all 256 colors
rather than asking a true-color decoder to flatten them. It accepts only this bounded layout,
validates runs and row padding, and reports structured errors for malformed data.

The stock sheets use cyan one-pixel cell guides; atlas rectangles in `smac/formats/atlas.hpp` exclude
those guides. `ter1.pcx` terrain-object cells are 100×62, `texture.pcx` terrain cells are 56×56 with a
57-pixel stride, and the native lifecycle cells used from `Units.pcx` are 100×76 with a 102-pixel
horizontal stride. Animation and connectivity variants are represented as a base rectangle, frame
count, grid width, and strides rather than repeated rendering constants.

Transparency is palette-index based, not an RGB comparison. In the observed stock SMACX sheets,
`ter1.pcx` uses index 253 for its empty background and index 254 for cyan guides, while `texture.pcx`
uses index 255. The selected `Units.pcx` lifecycle cells require both indices 253 and 255 to be keyed;
their cyan guides remain outside the declared rectangles. Palette RGB values are preserved even for
transparent pixels because index identity, palette ramps, and user-modified sheets must remain
distinguishable.

The current declarative regions cover M1 water and rainfall surfaces, fungus, rockiness, resources,
improvements, pods, forest/jungle/river connectivity variants, eight-direction road/tube links, and
the native lifecycle frames. The client warps each 56×56 surface into the projected tile diamond,
then layers rockiness, farms, forest/jungle, fungus, rivers, transport links, `ter1.pcx` objects,
ownership, route/cursor overlays, and units in that order. Full-sheet SDL textures are uploaded once
and reused; viewport culling limits composition to visible wrapped copies.

Coordinates were checked against the user-owned GOG SMACX sheets and corroborated against the
coordinate tables in the independent AGPL GLSMAC renderer. No original pixels are present in the
repository. Regions for later systems will be added only when those systems become renderable.
