#pragma once
#include <juce_core/juce_core.h>

// Parameter IDs and the option lists, shared by the processor and the UI.
// New modes and themes are only ever appended, so saved projects keep pointing
// at the same mode/theme.
namespace vq::ids
{
    // page 1: LOOK
    inline constexpr const char* mode       = "mode";
    inline constexpr const char* theme      = "theme";
    inline constexpr const char* sensitivity= "sens";
    inline constexpr const char* smoothing  = "smooth";
    inline constexpr const char* speed      = "speed";
    inline constexpr const char* trails     = "trails";
    inline constexpr const char* glow       = "glow";
    inline constexpr const char* react      = "react";
    inline constexpr const char* detail     = "detail";
    inline constexpr const char* hueDrift   = "huedrift";
    inline constexpr const char* mirror     = "mirror";
    inline constexpr const char* cycle      = "cycle";
    inline constexpr const char* shuffle    = "shuffle";

    // page 2: MOTION
    inline constexpr const char* zoom       = "zoom";
    inline constexpr const char* spin       = "spin";
    inline constexpr const char* shake      = "shake";
    inline constexpr const char* feedback   = "feedback";
    inline constexpr const char* twist      = "twist";
    inline constexpr const char* strobe     = "strobe";
    inline constexpr const char* lineWidth  = "linewidth";
    inline constexpr const char* beatSens   = "beatsens";

    // page 3: COLOR / FX
    inline constexpr const char* saturation = "saturation";
    inline constexpr const char* brightness = "brightness";
    inline constexpr const char* palShift   = "palshift";
    inline constexpr const char* scanlines  = "scanlines";
    inline constexpr const char* vignette   = "vignette";
    inline constexpr const char* pixelate   = "pixelate";
    inline constexpr const char* tilt       = "tilt";
    inline constexpr const char* symmetry   = "symmetry";

    inline juce::StringArray modeNames()
    {
        return { "Spectrum Bars", "Oscilloscope", "Radial Burst", "Lissajous", "Waterfall",
                 "Particle Storm", "Warp Tunnel", "VU Meters", "Starfield", "Kaleidoscope",
                 "Plasma", "LED Matrix",
                 // added in v1.1
                 "Pulse Rings", "Retro Horizon", "Ridgelines", "Orbit Scope", "Lava Lamp",
                 "Fireworks", "Digital Rain", "Ripple Grid", "Flow Field", "Hypno Spiral",
                 "Shape Shifter", "Lightning", "Disco Floor", "VFD Analyzer", "Ribbons", "Galaxy" };
    }

    // How the mode menu is grouped (indices into modeNames()).
    struct ModeGroup { const char* name; std::vector<int> modes; };
    inline const std::vector<ModeGroup>& modeGroups()
    {
        static const std::vector<ModeGroup> g {
            { "Classic Meters",       { 0, 1, 3, 4, 7, 11, 25 } },
            { "Radial & Geometric",   { 2, 9, 12, 15, 21, 22 } },
            { "Space & Energy",       { 5, 6, 8, 17, 23, 27 } },
            { "Retro & Psychedelic",  { 10, 13, 14, 16, 18, 24 } },
            { "Waves & Flow",         { 19, 20, 26 } },
        };
        return g;
    }

    inline juce::StringArray cycleNames()
    {
        return { "Off", "Every 8 s", "Every 16 s", "Every 32 s", "Every 64 s", "Every 16 beats", "Every 32 beats" };
    }

    inline juce::StringArray symmetryNames()
    {
        return { "Off", "Mirror L/R", "Mirror T/B", "Quad" };
    }
}
