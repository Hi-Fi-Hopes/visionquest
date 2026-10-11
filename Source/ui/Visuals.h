#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "dsp/Analyzer.h"
#include "ui/Theme.h"

namespace vq
{

// Everything a visual mode gets each frame.
struct Ctx
{
    const Analyzer& a;
    const Theme& th;
    float time;      // seconds since the editor opened
    float dt;        // seconds since the last frame
    float speed;     // 0.1 .. 3
    float glow;      // 0 .. 1
    float react;     // 0 .. 1, how hard beats hit
    float detail;    // 0 .. 1, counts of bars / particles / segments
    bool mirror;
    float hue;       // palette rotation in turns
    juce::Random& rng;
    juce::Rectangle<float> full;   // the whole canvas, even the part under the control bars
    float saturation = 1.0f, brightness = 1.0f, palShift = 0.0f;   // COLOR page

    // Palette lookup with the colour knobs applied. Palette Shift slides along
    // the palette and bounces back, so it never jumps between colours.
    juce::Colour col (float t, float alpha = 1.0f) const
    {
        if (palShift > 0.0f)
        {
            const float x = (juce::jlimit (0.0f, 1.0f, t) + 2.0f * palShift) * 0.5f;
            t = 1.0f - std::abs ((x - std::floor (x)) * 2.0f - 1.0f);
        }
        auto c = th.at (t, hue);
        if (saturation != 1.0f) c = c.withSaturation (juce::jlimit (0.0f, 1.0f, c.getSaturation() * saturation));
        if (brightness != 1.0f) c = c.withMultipliedBrightness (brightness);
        return c.withMultipliedAlpha (alpha);
    }
    float pulse() const { return a.beatPulse * react; }
};

class Visual
{
public:
    virtual ~Visual() = default;
    virtual void reset() {}
    // Draws one frame. For modes with usesTrails(), the previous frame is still
    // there, faded by the Trails knob; the others get a fresh background.
    virtual void render (juce::Graphics&, juce::Rectangle<float> area, const Ctx&) = 0;
    virtual bool usesTrails() const { return true; }
    // Full-bleed modes use the whole canvas; the rest lay out between the control bars.
    virtual bool fullBleed() const { return false; }
};

std::unique_ptr<Visual> createVisual (int index);
void setLineWidthScale (float);   // Line Width knob, applied to every glowing stroke
void drawBackground (juce::Graphics&, juce::Rectangle<float>, const Theme&, float hue, float alpha);

} // namespace vq
