#pragma once
// Drawing helpers shared by the visual modes.
#include <juce_gui_basics/juce_gui_basics.h>

namespace vq::detail
{
    inline constexpr float twoPi = juce::MathConstants<float>::twoPi;
    inline constexpr float pi = juce::MathConstants<float>::pi;

    inline float lineScale = 1.0f;   // Line Width knob

    inline float frac (float x) { return x - std::floor (x); }

    // Folds a position around a circle into 0..1..0 so low end meets low end.
    inline float fold (float u, int folds)
    {
        const float x = frac (u * (float) folds * 0.5f) * 2.0f;
        return x < 1.0f ? x : 2.0f - x;
    }

    inline void glowStroke (juce::Graphics& g, const juce::Path& p, juce::Colour c, float width, float glow)
    {
        using namespace juce;
        width *= lineScale;
        const PathStrokeType::JointStyle j = PathStrokeType::curved;
        const PathStrokeType::EndCapStyle e = PathStrokeType::rounded;
        if (glow > 0.01f)
        {
            g.setColour (c.withMultipliedAlpha (0.09f * glow));
            g.strokePath (p, PathStrokeType (width * (1.0f + 7.0f * glow), j, e));
            g.setColour (c.withMultipliedAlpha (0.22f * glow));
            g.strokePath (p, PathStrokeType (width * (1.0f + 2.8f * glow), j, e));
        }
        g.setColour (c);
        g.strokePath (p, PathStrokeType (width, j, e));
        if (glow > 0.35f)
        {
            g.setColour (Colours::white.withAlpha (0.45f * (glow - 0.35f) * c.getFloatAlpha()));
            g.strokePath (p, PathStrokeType (jmax (0.6f, width * 0.35f), j, e));
        }
    }

    inline void glowDot (juce::Graphics& g, juce::Point<float> p, float r, juce::Colour c, float glow)
    {
        if (glow > 0.01f)
        {
            g.setColour (c.withMultipliedAlpha (0.16f * glow));
            const float gr = r * (1.0f + 2.5f * glow);
            g.fillEllipse (p.x - gr, p.y - gr, gr * 2, gr * 2);
        }
        g.setColour (c);
        g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
    }

    // Glowing straight line (cheaper than a path for lots of short segments).
    inline void glowLine (juce::Graphics& g, juce::Line<float> l, juce::Colour c, float width, float glow)
    {
        width *= lineScale;
        if (glow > 0.01f)
        {
            g.setColour (c.withMultipliedAlpha (0.18f * glow));
            g.drawLine (l, width * (1.0f + 3.0f * glow));
        }
        g.setColour (c);
        g.drawLine (l, width);
    }
}
