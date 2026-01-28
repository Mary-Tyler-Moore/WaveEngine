# Phase 04 — Level Streaming (Chunk Streaming Baseline)

## Why this stage exists
- Establish a chunk-streaming baseline and counters before culling/eviction/threading.
- Validate world coordinate mapping from chunk filenames.
- Compare full-map vs chunked rendering cost.

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
./build/dev/projects/phase_04_level_streaming/phase_04_level_streaming
```

## Assets
- Chunk directory: `assets/tiles/chunk_non_empty`
- Tile atlas: `assets/tiles/16k-waves-trans-atlas.png`
- Boat sprite: `assets/boat-64.png`

## Controls
- ESC: Quit
- W/S/A/D: Move boat
- C: Toggle camera smoothing
- [: Decrease active radius
- ]: Increase active radius
- V: Toggle chunk bounds overlay
- F12: Save screenshot to `assets/screenshots/phase_04_level_streaming.png`

## Notes
- Water chunks render before island chunks for deterministic layering.
- The window title updates once per second with FPS, active radius, loaded/active chunks,
  tiles per frame, chunk loads per second, and coarse timing buckets.

## Screenshot overlay
Screenshots include a small top-left overlay with the phase name, FPS/avg ms, radius and chunk
counts, boat/camera positions, and uptime.

## CSV telemetry
- Output: `assets/profiles/phase_04_level_streaming.csv` (created on first run).
- Sampled at ~5Hz with additional event rows on radius changes and chunk loads.
- Columns:
  - `t_ms`, `frame`, `dt_ms`, `fps_smooth`
  - `boat_x`, `boat_y`, `cam_x`, `cam_y`
  - `culling_enabled`, `radius_tiles`
  - `chunks_visible`, `chunks_loaded_total`
  - `chunks_load_started`, `chunks_load_finished`, `chunks_unload_started`, `chunks_unload_finished`
  - `load_queue_len`, `note`
