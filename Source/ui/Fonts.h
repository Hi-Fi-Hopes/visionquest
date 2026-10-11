#pragma once
#include <juce_graphics/juce_graphics.h>
#include "BinaryData.h"

namespace vq
{

// Embedded fonts (SIL Open Font License, see Resources/OFL-*.txt).
struct Fonts
{
    static Fonts& get() { static Fonts f; return f; }

    juce::Typeface::Ptr display = make (BinaryData::ShrikhandRegular_ttf,      BinaryData::ShrikhandRegular_ttfSize);
    juce::Typeface::Ptr medium  = make (BinaryData::SpaceGroteskMedium_ttf,    BinaryData::SpaceGroteskMedium_ttfSize);
    juce::Typeface::Ptr bold    = make (BinaryData::SpaceGroteskBold_ttf,      BinaryData::SpaceGroteskBold_ttfSize);
    juce::Typeface::Ptr mono    = make (BinaryData::JetBrainsMonoSemiBold_ttf, BinaryData::JetBrainsMonoSemiBold_ttfSize);

    static juce::Font font (juce::Typeface::Ptr tf, float height, float kerning = 0.0f)
    {
        return juce::Font (juce::FontOptions (tf).withHeight (height).withKerningFactor (kerning));
    }

private:
    static juce::Typeface::Ptr make (const char* data, int size) { return juce::Typeface::createSystemTypefaceFor (data, (size_t) size); }
};

} // namespace vq
