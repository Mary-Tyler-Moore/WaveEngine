# WaveEngine

A small, deliberately scoped 2D engine built in C++17 using SDL2.
Each phase introduces one core engine system in isolation.

## Phases

### Phase 00 — Engine Bootstrap
**Goal:** Validate the foundation: window, renderer, asset loading.

- SDL2 window creation (high-DPI)
- Minimal render loop
- Texture loading from shared `assets/`

▶ See: [projects/phase_00_engine_bootstrap/README.md](projects/phase_01_tilemap_rendering/README.md)


![Phase 00 Screenshot](assets/screenshots/phase_00_engine_bootstrap.png)

---

### Phase 01 — Tilemap Rendering
**Goal:** Load and render a real Tiled map with a 32×32 atlas.

- Load a Tiled JSON map from disk (`assets/tiles/ww-16k.json`)
- Decode base64 + zlib tile layers
- Render a 32×32 atlas (`assets/tiles/16k-waves-trans-atlas.png`) using SDL2

▶ See: [projects/phase_01_tilemap_rendering/README.md](projects/phase_01_tilemap_rendering/README.md)

![Phase 00 Screenshot](assets/screenshots/phase_01_tilemap_rendering.png)


---
