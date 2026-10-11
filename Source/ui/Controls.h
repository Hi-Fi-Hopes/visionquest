#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "ui/Theme.h"
#include "ui/Fonts.h"

namespace vq
{

// Look and feel that re-colours itself from the current theme.
class VQLookAndFeel : public juce::LookAndFeel_V4
{
public:
    VQLookAndFeel() { setTheme (themes()[0]); }

    void setTheme (const Theme& t)
    {
        theme = t;
        using namespace juce;
        setColour (PopupMenu::backgroundColourId, t.bg.brighter (0.08f).withAlpha (0.97f));
        setColour (PopupMenu::textColourId, t.text);
        setColour (PopupMenu::highlightedBackgroundColourId, t.accent.withAlpha (0.35f));
        setColour (PopupMenu::highlightedTextColourId, Colours::white);
        setColour (ComboBox::textColourId, t.text);
        setColour (ComboBox::arrowColourId, t.accent);
        setColour (TextButton::textColourOffId, t.text.withAlpha (0.75f));
        setColour (TextButton::textColourOnId, Colours::white);
        setColour (Label::textColourId, t.text.withAlpha (0.7f));
    }

    const Theme& getTheme() const { return theme; }
    juce::Colour panel() const { return theme.bg.withAlpha (0.74f); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider&) override
    {
        using namespace juce;
        auto b = Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (4.0f);
        const float rad = jmin (b.getWidth(), b.getHeight()) * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        const float tw = jmax (3.0f, rad * 0.16f);

        Path track; track.addCentredArc (c.x, c.y, rad - tw, rad - tw, 0, a0, a1, true);
        g.setColour (theme.text.withAlpha (0.12f));
        g.strokePath (track, PathStrokeType (tw, PathStrokeType::curved, PathStrokeType::rounded));

        if (pos > 0.001f)
        {
            Path val; val.addCentredArc (c.x, c.y, rad - tw, rad - tw, 0, a0, ang, true);
            ColourGradient grad (theme.at (0.1f), c.x - rad, c.y + rad, theme.at (0.9f), c.x + rad, c.y - rad, false);
            g.setColour (theme.at (0.6f).withAlpha (0.25f));
            g.strokePath (val, PathStrokeType (tw * 2.6f, PathStrokeType::curved, PathStrokeType::rounded));
            g.setGradientFill (grad);
            g.strokePath (val, PathStrokeType (tw, PathStrokeType::curved, PathStrokeType::rounded));
        }

        const float cr = rad * 0.58f;
        ColourGradient body (theme.bg2.brighter (0.25f), c.x, c.y - cr, theme.bg.darker (0.3f), c.x, c.y + cr, false);
        g.setGradientFill (body);
        g.fillEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2);
        g.setColour (theme.text.withAlpha (0.18f));
        g.drawEllipse (c.x - cr, c.y - cr, cr * 2, cr * 2, 1.0f);

        const Point<float> dot (c.x + std::sin (ang) * cr * 0.65f, c.y - std::cos (ang) * cr * 0.65f);
        g.setColour (theme.accent.withAlpha (0.3f));
        g.fillEllipse (dot.x - 4.5f, dot.y - 4.5f, 9, 9);
        g.setColour (Colours::white);
        g.fillEllipse (dot.x - 2.2f, dot.y - 2.2f, 4.4f, 4.4f);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        using namespace juce;
        auto b = Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (theme.bg.brighter (0.1f).withAlpha (box.isMouseOver (true) ? 0.95f : 0.8f));
        g.fillRoundedRectangle (b, b.getHeight() * 0.5f);
        g.setColour (theme.accent.withAlpha (0.45f));
        g.drawRoundedRectangle (b, b.getHeight() * 0.5f, 1.0f);
        Path arrow;
        const float ax = (float) w - 16.0f, ay = (float) h * 0.5f;
        arrow.addTriangle (ax - 4, ay - 2, ax + 4, ay - 2, ax, ay + 3);
        g.setColour (theme.accent);
        g.fillPath (arrow);
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return Fonts::font (Fonts::get().bold, 14.0f); }
    juce::Font getPopupMenuFont() override { return Fonts::font (Fonts::get().medium, 15.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return Fonts::font (Fonts::get().bold, 12.0f, 0.08f); }
    juce::Font getLabelFont (juce::Label&) override { return Fonts::font (Fonts::get().bold, 11.0f, 0.1f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (12, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        using namespace juce;
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        if (on)
        {
            ColourGradient grad (theme.at (0.15f), r.getX(), 0, theme.at (0.85f), r.getRight(), 0, false);
            g.setGradientFill (grad);
            g.setOpacity (down ? 0.75f : 0.9f);
        }
        else
        {
            g.setColour (theme.bg.brighter (over ? 0.25f : 0.1f).withAlpha (down ? 1.0f : 0.8f));
        }
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setOpacity (1.0f);
        g.setColour (theme.accent.withAlpha (on ? 0.9f : 0.4f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
    }

    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (findColour (juce::PopupMenu::backgroundColourId));
        g.setColour (theme.accent.withAlpha (0.4f));
        g.drawRect (0, 0, w, h, 1);
    }

private:
    Theme theme;
};

// Small left/right arrow button.
class ArrowButton : public juce::Button
{
public:
    explicit ArrowButton (bool rightFacing) : juce::Button (rightFacing ? "next" : "prev"), right (rightFacing) {}

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto* lnf = dynamic_cast<VQLookAndFeel*> (&getLookAndFeel());
        const auto accent = lnf != nullptr ? lnf->getTheme().accent : juce::Colours::white;
        auto b = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (accent.withAlpha (over ? 0.25f : 0.1f));
        g.fillEllipse (b);
        juce::Path p;
        const auto c = b.getCentre();
        const float s = b.getHeight() * 0.2f * (down ? 0.85f : 1.0f);
        if (right) p.addTriangle (c.x - s * 0.7f, c.y - s, c.x - s * 0.7f, c.y + s, c.x + s, c.y);
        else       p.addTriangle (c.x + s * 0.7f, c.y - s, c.x + s * 0.7f, c.y + s, c.x - s, c.y);
        g.setColour (accent);
        g.fillPath (p);
    }

private:
    bool right;
};

// Rotary knob with a label above and the value below.
class Knob : public juce::Component
{
public:
    explicit Knob (const juce::String& name)
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 80, 16);
        slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
        slider.setPopupDisplayEnabled (false, false, nullptr);
        label.setText (name.toUpperCase(), juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (slider);
        addAndMakeVisible (label);
    }

    void recolour (const Theme& t)
    {
        // Set on each slider, otherwise the readout draws white-on-light.
        slider.setColour (juce::Slider::textBoxTextColourId, t.text.withAlpha (0.9f));
        slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        slider.setColour (juce::Slider::textBoxHighlightColourId, t.accent.withAlpha (0.4f));
        label.setColour (juce::Label::textColourId, t.text.withAlpha (0.65f));
    }

    void resized() override
    {
        auto b = getLocalBounds();
        label.setBounds (b.removeFromTop (14));
        slider.setBounds (b);
    }

    juce::Slider slider;
    juce::Label label;
};

} // namespace vq
