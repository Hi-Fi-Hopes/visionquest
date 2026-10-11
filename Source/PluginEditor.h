#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "dsp/Analyzer.h"
#include "ui/Visuals.h"
#include "ui/Controls.h"

namespace vq
{

//==============================================================================
// The drawing surface: pulls audio from the tap, runs the analyzer, handles
// auto-cycling, and renders the current mode into a persistent trail buffer.
class Canvas : public juce::Component
{
public:
    explicit Canvas (VisionQuestProcessor&);

    void tick (float dt);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void showMode (int index, bool announce = true);
    void showTheme (int index, bool announce = true);
    int getDisplayMode() const { return displayMode; }
    int getDisplayTheme() const { return displayTheme; }

    void flashHint (const juce::String& text) { hintText = text; hintAlpha = 2.5f; }
    // Space taken by the control bars; layout modes slide to fill it when they hide.
    void setSafeInsets (float top, float bottom) { targetTop = top; targetBottom = bottom; }

    std::function<void()> onSingleClick, onDoubleClick;

private:
    float param (const char* id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    VisionQuestProcessor& proc;
    Analyzer analyzer;
    std::vector<float> L = std::vector<float> (Analyzer::fftSize), R = std::vector<float> (Analyzer::fftSize);
    uint64_t lastWritten = 0;
    float stall = 0;
    double analyzerRate = 0;

    std::vector<std::unique_ptr<Visual>> visuals;
    int displayMode = 0, displayTheme = 0, lastParamMode = -1, lastParamTheme = -1;
    float cycleTimer = 0; int beatsSince = 0;

    juce::Image trail, scratch, scanTile;
    bool trailFresh = false;
    float spinAngle = 0;
    float time = 0, pendingDt = 0, huePhase = 0;
    juce::Random rng;

    juce::String bannerText, bannerSub;
    float bannerAlpha = 0;
    juce::String hintText;
    float hintAlpha = 0;
    int clickToken = 0;
    float insetTop = 0, insetBottom = 0, targetTop = 0, targetBottom = 0;
};

//==============================================================================
class TopBar : public juce::Component
{
public:
    TopBar();
    void paint (juce::Graphics&) override;
    void resized() override;

    ArrowButton modePrev { false }, modeNext { true }, themePrev { false }, themeNext { true };
    juce::ComboBox modeBox, themeBox, cycleBox;
    juce::TextButton fullBtn { "FULL SCREEN" }, hideBtn { "HIDE" };
};

// A labelled drop-down that sits in a knob slot.
class ChoiceSlot : public juce::Component
{
public:
    explicit ChoiceSlot (const juce::String& name);
    void resized() override;
    void recolour (const Theme&);
    juce::ComboBox box;
    juce::Label label;
};

class BottomBar : public juce::Component
{
public:
    BottomBar();
    void paint (juce::Graphics&) override;
    void resized() override;
    void recolour (const Theme&);
    void setPage (int);

    // page 1: LOOK (unchanged from v1.0)
    Knob sens { "Sensitivity" }, smooth { "Smoothing" }, speed { "Speed" }, trails { "Trails" },
         glow { "Glow" }, react { "Beat React" }, detail { "Detail" }, hue { "Hue Drift" };
    // page 2: MOTION
    Knob zoom { "Zoom" }, spin { "Spin" }, shake { "Shake" }, feedback { "Feedback" },
         twist { "Twist" }, strobe { "Strobe" }, lineWidth { "Line Width" }, beatSens { "Beat Sens" };
    // page 3: COLOR / FX
    Knob saturation { "Saturation" }, brightness { "Brightness" }, palShift { "Pal Shift" }, scanlines { "Scanlines" },
         vignette { "Vignette" }, pixelate { "Pixelate" }, tilt { "Tilt" };
    ChoiceSlot symmetry { "Symmetry" };

    juce::TextButton mirror { "MIRROR" }, shuffle { "SHUFFLE" }, dice { "DICE" };
    juce::TextButton tabLook { "LOOK" }, tabMotion { "MOTION" }, tabColor { "COLOR" };

    std::array<Knob*, 8> knobs() { return { &sens, &smooth, &speed, &trails, &glow, &react, &detail, &hue }; }
    std::array<Knob*, 8> motionKnobs() { return { &zoom, &spin, &shake, &feedback, &twist, &strobe, &lineWidth, &beatSens }; }
    std::array<Knob*, 7> colorKnobs() { return { &saturation, &brightness, &palShift, &scanlines, &vignette, &pixelate, &tilt }; }

private:
    int page = 0;
};

//==============================================================================
// Canvas plus the floating control bars. This is what moves into the
// full-screen window and back.
class Stage : public juce::Component
{
public:
    explicit Stage (VisionQuestProcessor&);
    ~Stage() override;

    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;
    void mouseMove (const juce::MouseEvent&) override;

    void tick (float dt);
    void setFullscreen (bool);
    void setControlsVisible (bool);

    std::function<void()> onToggleFullscreen;

private:
    void stepMode (int delta);
    void stepTheme (int delta);
    void setChoice (const char* id, int index);
    void applyTheme (int index);

    VisionQuestProcessor& proc;
    VQLookAndFeel lnf;
    Canvas canvas;
    TopBar top;
    BottomBar bottom;

    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    std::vector<std::unique_ptr<SA>> sliderAttachments;
    std::unique_ptr<BA> mirrorAttach, shuffleAttach;
    std::unique_ptr<CA> cycleAttach, symmetryAttach;
    std::vector<int> menuOrder;   // modes in the order the menu shows them
    juce::Random diceRng;
    void rollDice();

    bool fullscreen = false, controlsShown = true;
    float idle = 0, barScale = 1;
    int appliedTheme = -1;
    void updateInsets();
    juce::VBlankAttachment vblank;
    double lastStamp = 0;
};

//==============================================================================
// Borderless window covering one monitor.
class FullscreenWindow : public juce::Component
{
public:
    FullscreenWindow (juce::Rectangle<int> area, std::function<void()> onExit);
    ~FullscreenWindow() override;
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    std::function<void()> exitFn;
};

} // namespace vq

//==============================================================================
class VisionQuestEditor : public juce::AudioProcessorEditor
{
public:
    explicit VisionQuestEditor (VisionQuestProcessor&);
    ~VisionQuestEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    void toggleFullscreen();
    void enterFullscreen (int displayIndex);
    void exitFullscreen();

    // For the snapshot tool: advance one frame without a vblank.
    void testTick (float dt) { stage->tick (dt); }
    vq::Stage& getStage() { return *stage; }

private:
    VisionQuestProcessor& proc;
    std::unique_ptr<vq::Stage> stage;
    std::unique_ptr<vq::FullscreenWindow> fsWindow;
    juce::String fsLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VisionQuestEditor)
};
