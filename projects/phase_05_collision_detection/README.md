# Phase 05 — Collision Detection

## Goal
- Build shoreline collision masks from island chunk tilemaps.
- Keep collision query-only (no response forces yet).
- Preserve the existing camera/streaming loop with clear debug visuals.

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
./build/dev/projects/phase_05_collision_detection/phase_05_collision_detection
```

## Assets
- Chunk directory (collision source): `assets/tiles/chunk_non_empty`
- Tile atlas: `assets/tiles/16k-waves-trans-atlas.png`
- Boat sprite: `assets/boat-64.png`

## Controls
- ESC: Quit
- W/S/A/D: Move boat
- C: Toggle camera smoothing
- [: Decrease active radius
- ]: Increase active radius
- V: Toggle chunk bounds overlay
- B: Toggle collision debug overlay
- F12: Save screenshot to `assets/screenshots/phase_05_collision_detection.png`

## Notes
- Collision masks are derived from island chunk tilemaps by filtering tiles with class/type
  `shore` and marking those cells as solid.
- Collision queries test the boat AABB against active island chunks only.
- Tile flip flags are masked; flips are ignored (logged once if encountered).

## Screenshot
- `assets/screenshots/phase_05_collision_detection.png`
