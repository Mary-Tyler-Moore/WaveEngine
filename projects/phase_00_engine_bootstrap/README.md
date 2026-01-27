# Phase 00 — Engine Bootstrap

## Why this stage exists
- Prove SDL2 can open a high-DPI window at the target size.
- Validate asset loading from the repo at runtime.
- Establish a minimal render loop and texture draw path.

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
./build/dev/projects/phase_00_engine_bootstrap/phase_00_engine_bootstrap
```

## Notes
- Asset path: `assets/boat-64.png`

## Screenshot Capture

Press **F12** at runtime to capture the current frame buffer.

The screenshot is saved to:

assets/screenshots/phase_00_engine_bootstrap.png

This is used for documentation and phase verification.