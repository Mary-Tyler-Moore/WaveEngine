# Phase 01 — Tilemap Rendering

## Why this stage exists
- Load real map data and render deterministically.
- Prove atlas-based SDL2 rendering end-to-end.
- Keep a minimal tilemap data flow for later phases.

## Build (Debug)
```bash
source ~/conan_venv/bin/activate
conan install . --output-folder=build/dev -pr:h clang17 -pr:b clang17 -s build_type=Debug --build=missing
cmake --preset dev
cmake --build --preset dev
```

## Build (Release)
```bash
source ~/conan_venv/bin/activate
conan install . --output-folder=build/release -pr:h clang17 -pr:b clang17 -s build_type=Release --build=missing
cmake --preset release
cmake --build --preset release
```

## Run
```bash
./build/dev/projects/phase_01_tilemap_rendering/phase_01_tilemap_rendering
```

## Initial focus
The renderer centers on the bounds of the first tile layer that uses the atlas tileset and has
nonzero tiles. This avoids centering the viewport on an empty area (the map midpoint can be all
zeroes), while still remaining deterministic. If no atlas-backed layer has tiles, it falls back to
the map midpoint.

## Assets
- Tile atlas: `assets/tiles/16k-waves-trans-atlas.png`
- Tilemap JSON: `assets/tiles/ww-16k.json`

## JSON fields used
- Map size: `width`, `height`
- Tile size: `tilewidth`, `tileheight`
- Layers: `layers[].type`, `layers[].name`, `layers[].data`
- Atlas tileset: `tilesets[].image`, `tilesets[].firstgid`, `tilesets[].columns`,
  `tilesets[].tilewidth`, `tilesets[].tileheight`, `tilesets[].tilecount`

## Controls
- ESC: Quit
- F12: Save screenshot to `assets/screenshots/phase_01_tilemap_rendering.png`
- WASD / Arrow Keys: Pan view
- +/-: Zoom in/out
- R: Reset view
