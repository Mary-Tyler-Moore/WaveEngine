# WaveEngine

A small, deliberately scoped 2D engine built in C++17 using SDL2.
Each phase introduces one core engine system in isolation.

## Phases

### Phase 00 — Engine Bootstrap
**Goal:** Validate the foundation: window, renderer, asset loading.

- SDL2 window creation (high-DPI)
- Minimal render loop
- Texture loading from shared `assets/`

▶ See: `projects/phase_00_engine_bootstrap/README.md`

![Phase 00 Screenshot](assets/screenshots/phase_00_engine_bootstrap.png)

---
