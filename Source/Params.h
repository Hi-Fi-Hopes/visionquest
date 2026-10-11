#pragma once
#include <juce_core/juce_core.h>

// Parameter IDs and the option lists, shared by the processor and the UI.
namespace vq::ids
{
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

    inline juce::StringArray modeNames()
    {
        return { "Spectrum Bars", "Oscilloscope", "Radial Burst", "Lissajous", "Waterfall",
                 "Particle Storm", "Warp Tunnel", "VU Meters", "Starfield", "Kaleidoscope",
                 "Plasma", "LED Matrix" };
    }

    inline juce::StringArray cycleNames()
    {
        return { "Off", "Every 8 s", "Every 16 s", "Every 32 s", "Every 64 s", "Every 16 beats", "Every 32 beats" };
    }
}
