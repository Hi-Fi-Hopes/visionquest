# VisionQuest

An animated audio visualizer VST3 for Ableton Live (JUCE / C++). Audio passes through untouched; the window is the show.

## Modes (12)
Spectrum Bars · Oscilloscope · Radial Burst · Lissajous · Waterfall · Particle Storm · Warp Tunnel · VU Meters · Starfield · Kaleidoscope · Plasma · LED Matrix

## Themes (11)
Synthwave · Marantz Glow · Phosphor · Amber CRT · Psychedelic · Inferno · Glacier · Toxic · Sunset Strip · Vaporwave · Monochrome

## Controls
| Control | What it does |
|---|---|
| Sensitivity | Shifts the whole display up or down (-12 to +24 dB). Turn up for quiet tracks. |
| Smoothing | Twitchy (0%) to slow and syrupy (100%) |
| Speed | Animation speed: tunnel, stars, rotation, waterfall scroll |
| Trails | How long motion trails linger |
| Glow | Bloom around lines and dots |
| Beat React | How hard each detected beat hits (pulses, bursts, ripples) |
| Detail | More bars / particles / stars / segments |
| Hue Drift | Slowly rotates any theme around the colour wheel |
| Mirror | Symmetric version of each mode |
| Auto Cycle | Change mode every 8–64 s, or every 16 / 32 beats |
| Shuffle | Auto-cycle in random order, with random themes |

## Full screen
Click **FULL SCREEN** (or press **F**, or double-click the picture). With more than one monitor it asks which screen. Controls hide after 3 seconds; move the mouse to bring them back. **Esc** exits.

## Keys
← / → mode · ↑ / ↓ theme · H hide controls · F full screen · Esc exit full screen

## Building
GitHub Actions builds the Windows VST3 on every push to `main` (see `.github/workflows/build.yml`). The zip lands on the `builds` branch.

Analyzer test (no JUCE needed):
```
g++ -O2 -std=c++17 -I Source tests/analyzer_test.cpp -o analyzer_test && ./analyzer_test
```

Fonts: Shrikhand, Space Grotesk, JetBrains Mono — SIL Open Font License (see `Resources/OFL-*.txt`).
