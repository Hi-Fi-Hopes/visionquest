# VisionQuest

An animated audio visualizer VST3 for Ableton Live (JUCE / C++). Audio passes through untouched; the window is the show.

## Modes (28)
**Classic Meters:** Spectrum Bars · Oscilloscope · Lissajous · Waterfall · VU Meters · LED Matrix · VFD Analyzer
**Radial & Geometric:** Radial Burst · Kaleidoscope · Pulse Rings · Orbit Scope · Hypno Spiral · Shape Shifter
**Space & Energy:** Particle Storm · Warp Tunnel · Starfield · Fireworks · Lightning · Galaxy
**Retro & Psychedelic:** Plasma · Retro Horizon · Ridgelines · Lava Lamp · Digital Rain · Disco Floor
**Waves & Flow:** Ripple Grid · Flow Field · Ribbons

## Themes (16)
Synthwave · Marantz Glow · Phosphor · Amber CRT · Psychedelic · Inferno · Glacier · Toxic · Sunset Strip · Vaporwave · Monochrome · Nixie Tube · Bubblegum · Deep Ocean · Gold Record · Blood Moon

## Controls
Three knob pages (LOOK / MOTION / COLOR, or keys 1 / 2 / 3). Double-click any knob to reset it.

**LOOK**
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

**MOTION**
| Control | What it does |
|---|---|
| Zoom | Scale the picture (0.5x to 2x) |
| Spin | Rotate the picture continuously, either direction. Back to 0 eases upright. |
| Shake | Camera shake on every beat |
| Feedback | Each frame zooms the last one: trails become tunnels (trail-based modes) |
| Twist | Rotates the feedback: swirls and vortexes |
| Strobe | Flash on every beat |
| Line Width | Thickness of every glowing line |
| Beat Sens | How easily a beat counts (higher = more beats) |

**COLOR**
| Control | What it does |
|---|---|
| Saturation | Washed out to super vivid |
| Brightness | Dim to hot |
| Pal Shift | Slides the theme's colours around |
| Scanlines | CRT scanlines with a rolling band |
| Vignette | Darkens the corners |
| Pixelate | Chunky retro pixels |
| Tilt | Spectrum balance: lower favours bass, higher favours treble |
| Symmetry | Mirror L/R, mirror T/B, or quad (applies to every mode) |

**Buttons:** Mirror (symmetric version of each mode) · Shuffle (random order + themes when auto-cycling) · Dice (random look)
**Auto Cycle:** change mode every 8–64 s, or every 16 / 32 beats.

## Full screen
Click **FULL SCREEN** (or press **F**, or double-click the picture). With more than one monitor it asks which screen. Controls hide after 3 seconds; move the mouse to bring them back. **Esc** exits.

## Keys
← / → mode · ↑ / ↓ theme · 1 / 2 / 3 knob page · D dice · H hide controls · F full screen · Esc exit full screen

## Building
GitHub Actions builds the Windows VST3 on every push to `main` (see `.github/workflows/build.yml`). The zip lands on the `builds` branch.

Analyzer test (no JUCE needed):
```
g++ -O2 -std=c++17 -I Source tests/analyzer_test.cpp -o analyzer_test && ./analyzer_test
```

Fonts: Shrikhand, Space Grotesk, JetBrains Mono — SIL Open Font License (see `Resources/OFL-*.txt`).
