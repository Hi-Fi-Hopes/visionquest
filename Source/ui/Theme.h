#pragma once
#include <juce_graphics/juce_graphics.h>

namespace vq
{

struct Theme
{
    juce::String name;
    juce::Colour bg, bg2;                 // background gradient (centre, edge)
    std::vector<juce::Colour> stops;      // the main palette, low -> high
    juce::Colour accent, text;
    bool rainbow = false;                 // palette spins around the colour wheel by itself

    // t in 0..1 along the palette. hueOffset in turns (0..1), from Hue Drift or rainbow.
    juce::Colour at (float t, float hueOffset = 0.0f) const
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        const float x = t * (float) (stops.size() - 1);
        const int i = juce::jmin ((int) x, (int) stops.size() - 2);
        auto c = stops[(size_t) i].interpolatedWith (stops[(size_t) i + 1], x - (float) i);
        return hueOffset != 0.0f ? c.withRotatedHue (hueOffset) : c;
    }
};

inline const std::vector<Theme>& themes()
{
    using C = juce::Colour;
    static const std::vector<Theme> list {
        { "Synthwave",    C (0xff0d0221), C (0xff240046), { C (0xff2de2e6), C (0xff7b2ff7), C (0xfff706cf), C (0xfffd8a26), C (0xfff9e45b) }, C (0xfff706cf), C (0xfff5e9ff) },
        { "Marantz Glow", C (0xff03050b), C (0xff0a1834), { C (0xff0b2f78), C (0xff1e6fff), C (0xff6fb6ff), C (0xffcfe6ff), C (0xfffff1d6) }, C (0xff5aa8ff), C (0xffe6eeff) },
        { "Phosphor",     C (0xff010803), C (0xff05180b), { C (0xff0b4a1d), C (0xff18a53c), C (0xff48ff7a), C (0xffb8ffca), C (0xffeaffef) }, C (0xff48ff7a), C (0xffc8ffd6) },
        { "Amber CRT",    C (0xff080400), C (0xff1a0d02), { C (0xff4a1d00), C (0xffb05200), C (0xffff9a1f), C (0xffffc56b), C (0xffffedc9) }, C (0xffffa630), C (0xffffe2b0) },
        { "Psychedelic",  C (0xff0a0014), C (0xff1d0636), { C (0xffff2a6d), C (0xffff9f1c), C (0xfff9f871), C (0xff3cf06e), C (0xff1fb6ff), C (0xffb14aff) }, C (0xfff9f871), C (0xffffffff), true },
        { "Inferno",      C (0xff050000), C (0xff1a0303), { C (0xff4a0000), C (0xffc4120a), C (0xffff5a00), C (0xffffb627), C (0xfffff6cc) }, C (0xffff5a00), C (0xffffe6cc) },
        { "Glacier",      C (0xff01050c), C (0xff06172e), { C (0xff0b2a52), C (0xff1c7bd4), C (0xff4fd0ff), C (0xffb8f3ff), C (0xffffffff) }, C (0xff4fd0ff), C (0xffe6f8ff) },
        { "Toxic",        C (0xff020500), C (0xff0d1600), { C (0xff1f4d00), C (0xff4fb000), C (0xff9dff00), C (0xffe8ff3a), C (0xfffcffe0) }, C (0xffb4ff00), C (0xffeaffc0) },
        { "Sunset Strip", C (0xff10041a), C (0xff2e0c33), { C (0xff3b1f7a), C (0xff8f2d97), C (0xffe0407e), C (0xffff7a59), C (0xffffc95e) }, C (0xffff7a59), C (0xffffe9de) },
        { "Vaporwave",    C (0xff160d2e), C (0xff2e1d55), { C (0xff01cdfe), C (0xff05ffa1), C (0xffb967ff), C (0xffff71ce), C (0xfffffb96) }, C (0xffff71ce), C (0xfff4ecff) },
        { "Monochrome",   C (0xff000000), C (0xff101010), { C (0xff2e2e2e), C (0xff6e6e6e), C (0xffb0b0b0), C (0xffffffff) }, C (0xffffffff), C (0xffeeeeee) },
        // added in v1.1
        { "Nixie Tube",   C (0xff0a0402), C (0xff200d05), { C (0xff3a1204), C (0xffa33a0c), C (0xffff7a1a), C (0xffffb35c), C (0xffffe2b8) }, C (0xffff7a1a), C (0xffffd9b0) },
        { "Bubblegum",    C (0xff1a0a1f), C (0xff3a1340), { C (0xff7ad7ff), C (0xffc86bff), C (0xffff5fa2), C (0xffff8fd8), C (0xffffe6f5) }, C (0xffff5fa2), C (0xffffeaf6) },
        { "Deep Ocean",   C (0xff00060d), C (0xff002233), { C (0xff003b5c), C (0xff00708f), C (0xff00b3b3), C (0xff57e8c9), C (0xffd6fff4) }, C (0xff00d1c1), C (0xffd6fff4) },
        { "Gold Record",  C (0xff0a0804), C (0xff1e170a), { C (0xff4a3a12), C (0xff9c7a24), C (0xffe0b84a), C (0xfffff0b0), C (0xffffffff) }, C (0xffe0b84a), C (0xfffff3d0) },
        { "Blood Moon",   C (0xff070000), C (0xff1c0206), { C (0xff2b0008), C (0xff6e0014), C (0xffc4102a), C (0xffff4d4d), C (0xffffd0c8) }, C (0xffff2d3d), C (0xffffd6d0) },
    };
    return list;
}

inline juce::StringArray themeNames()
{
    juce::StringArray s;
    for (auto& t : themes()) s.add (t.name);
    return s;
}

} // namespace vq
