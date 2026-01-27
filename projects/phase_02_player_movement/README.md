# Phase 02 — Player Movement

## Why this stage exists
- Add a controllable player object without introducing engine architecture.
- Validate simple, frame-rate independent movement over a real tilemap.
- Keep the rendering pipeline stable while layering gameplay visuals.

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
./build/dev/projects/phase_02_player_movement/phase_02_player_movement
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
- W/S/A/D: Move boat
- Arrow Keys: Pan view
- +/-: Zoom in/out
- R: Reset view
- F12: Save screenshot to `assets/screenshots/phase_02_player_movement.png`

## Notes
- Boat sprite: `assets/boat-64.png`
- Acceleration/boat feel is intentionally deferred to a later phase.
