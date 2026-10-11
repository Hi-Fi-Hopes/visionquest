#include "PluginEditor.h"

using namespace juce;

namespace vq
{

static constexpr int designW = 960;
static constexpr int topH = 48, bottomH = 108;
static float frac01 (float x) { return x - std::floor (x); }

//==============================================================================
Canvas::Canvas (VisionQuestProcessor& p) : proc (p)
{
    setOpaque (true);
    for (int i = 0; i < ids::modeNames().size(); ++i)
        visuals.push_back (createVisual (i));
    displayMode = lastParamMode = jlimit (0, (int) visuals.size() - 1, (int) param (ids::mode));
    displayTheme = lastParamTheme = jlimit (0, (int) themes().size() - 1, (int) param (ids::theme));
}

void Canvas::showMode (int index, bool announce)
{
    index = jlimit (0, (int) visuals.size() - 1, index);
    if (index != displayMode) visuals[(size_t) index]->reset();
    displayMode = index;
    trailFresh = false;
    cycleTimer = 0; beatsSince = 0;
    if (announce) { bannerText = ids::modeNames()[index]; bannerSub = themes()[(size_t) displayTheme].name; bannerAlpha = 1.6f; }
}

void Canvas::showTheme (int index, bool announce)
{
    displayTheme = jlimit (0, (int) themes().size() - 1, index);
    huePhase = 0;   // each theme starts from its own colours
    trailFresh = false;
    if (announce) { bannerText = themes()[(size_t) displayTheme].name; bannerSub = {}; bannerAlpha = 1.2f; }
}

void Canvas::tick (float dt)
{
    dt = jlimit (0.0f, 0.1f, dt);
    time += dt;
    pendingDt += dt;

    // follow the parameters when they change (from the UI, automation or a preset)
    if (const int pm = (int) param (ids::mode); pm != lastParamMode) { lastParamMode = pm; showMode (pm); }
    if (const int pt = (int) param (ids::theme); pt != lastParamTheme) { lastParamTheme = pt; showTheme (pt); }

    // audio
    const double sr = proc.currentSampleRate.load();
    if (sr != analyzerRate) { analyzer.prepare (sr); analyzerRate = sr; }
    const uint64_t w = proc.tap.readLatest (L.data(), R.data(), Analyzer::fftSize);
    int fresh = (int) jmin<uint64_t> (w - lastWritten, (uint64_t) Analyzer::fftSize);
    lastWritten = w;
    if (fresh == 0)
    {
        // Host stopped sending audio: let everything fall to rest instead of freezing.
        if ((stall += dt) > 0.15f) { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); }
    }
    else stall = 0;

    AnalyzerSettings s;
    s.sensitivityDb = param (ids::sensitivity);
    s.smoothing = param (ids::smoothing);
    s.tiltDbPerOct = param (ids::tilt);
    s.beatSens = param (ids::beatSens);
    analyzer.process (L.data(), R.data(), fresh, dt, s);

    // auto-cycle
    const int cyc = (int) param (ids::cycle);
    if (cyc > 0)
    {
        bool due = false;
        if (cyc <= 4) { cycleTimer += dt; due = cycleTimer >= (float) (8 << (cyc - 1)); }
        else { if (analyzer.beat) ++beatsSince; due = beatsSince >= (cyc == 5 ? 16 : 32); }
        if (due)
        {
            const int n = (int) visuals.size();
            int next = (displayMode + 1) % n;
            if (param (ids::shuffle) > 0.5f)
            {
                next = (displayMode + 1 + rng.nextInt (n - 1)) % n;
                const int nt = (int) themes().size();
                showTheme ((displayTheme + 1 + rng.nextInt (nt - 1)) % nt, false);
            }
            showMode (next);
        }
    }

    const auto& th = themes()[(size_t) displayTheme];
    huePhase = frac01 (huePhase + dt * ((th.rainbow ? 0.035f : 0.0f) + param (ids::hueDrift) * 0.12f));
    const float ease = 1.0f - std::exp (-dt / 0.12f);
    insetTop += (targetTop - insetTop) * ease;
    insetBottom += (targetBottom - insetBottom) * ease;
    bannerAlpha = jmax (0.0f, bannerAlpha - dt * 0.8f);
    hintAlpha = jmax (0.0f, hintAlpha - dt);
    repaint();
}

void Canvas::paint (Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    if (r.isEmpty()) return;

    // Render at the screen's real pixel density, capped so a 4K wall doesn't melt the CPU.
    float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const float maxPixels = 4.2e6f;
    if (r.getWidth() * r.getHeight() * scale * scale > maxPixels)
        scale = std::sqrt (maxPixels / (r.getWidth() * r.getHeight()));
    // Pixelate: render at a fraction of the resolution, then blow it up with hard edges.
    if (const float pix = param (ids::pixelate); pix > 0.001f)
        scale = jmax (0.02f, scale / (1.0f + pix * 5.0f + pix * pix * 15.0f));
    const int iw = jmax (1, (int) std::ceil (r.getWidth() * scale)), ih = jmax (1, (int) std::ceil (r.getHeight() * scale));
    if (trail.getWidth() != iw || trail.getHeight() != ih)
    {
        trail = Image (Image::ARGB, iw, ih, true);
        trailFresh = false;
    }

    const float dt = jmin (pendingDt, 0.1f);
    pendingDt = 0;
    const auto& th = themes()[(size_t) displayTheme];
    auto& vis = *visuals[(size_t) displayMode];

    Ctx ctx { analyzer, th, time, dt, param (ids::speed), param (ids::glow), param (ids::react),
              param (ids::detail), param (ids::mirror) > 0.5f, huePhase, rng, r,
              param (ids::saturation), param (ids::brightness), param (ids::palShift) };
    const auto safe = r.withTrimmedTop (insetTop).withTrimmedBottom (insetBottom);
    setLineWidthScale (param (ids::lineWidth));

    // Feedback / Twist: the previous frame is zoomed and rotated a little before
    // the new one is drawn on top, which turns trails into tunnels and swirls.
    const float fb = param (ids::feedback), tw = param (ids::twist);
    const bool feeding = vis.usesTrails() && trailFresh && (fb > 0.001f || std::abs (tw) > 0.001f);
    if (feeding)
    {
        if (scratch.getWidth() != iw || scratch.getHeight() != ih) scratch = Image (Image::ARGB, iw, ih, true);
        {
            Graphics sg (scratch);
            sg.fillAll (th.bg.withRotatedHue (huePhase));
            const float k = dt * 60.0f;
            sg.drawImageTransformed (trail, AffineTransform::translation (-0.5f * (float) iw, -0.5f * (float) ih)
                                                .scaled (1.0f + fb * 0.035f * k)
                                                .rotated (tw * 0.035f * k)
                                                .translated (0.5f * (float) iw, 0.5f * (float) ih));
        }
        std::swap (trail, scratch);
    }

    // Spin / Zoom / Shake move the whole picture.
    if (const float spin = param (ids::spin); std::abs (spin) > 0.001f)
        spinAngle += dt * spin * 1.6f;
    else
    {
        // Spin turned off: ease back to upright the short way round.
        spinAngle = std::remainder (spinAngle, MathConstants<float>::twoPi);
        spinAngle *= std::exp (-dt / 0.35f);
    }
    const float shakeAmt = param (ids::shake) * analyzer.beatPulse * jmin (r.getWidth(), r.getHeight()) * 0.045f;
    const Point<float> shakeOff (shakeAmt * (rng.nextFloat() * 2.0f - 1.0f), shakeAmt * (rng.nextFloat() * 2.0f - 1.0f));
    const float zoom = param (ids::zoom);
    const auto camera = AffineTransform::translation (-r.getCentreX(), -r.getCentreY())
                            .scaled (zoom).rotated (spinAngle)
                            .translated (r.getCentreX() + shakeOff.x, r.getCentreY() + shakeOff.y);

    {
        Graphics tg (trail);
        tg.addTransform (AffineTransform::scale (scale));
        if (! trailFresh || ! vis.usesTrails())
        {
            drawBackground (tg, r, th, huePhase, 1.0f);
            trailFresh = true;
        }
        else
        {
            // Trails knob: how much of the previous frame fades per 60th of a second.
            // Feedback needs persistence to show, so it holds the trails open too.
            const float trailAmt = jmax (param (ids::trails), fb > 0.001f ? 0.55f + 0.42f * fb : 0.0f);
            const float perFrame = 1.0f - trailAmt * 0.93f;
            const float alpha = 1.0f - std::pow (1.0f - perFrame, dt * 60.0f);
            if (alpha > 0.0f) drawBackground (tg, r, th, huePhase, alpha);
        }
        tg.addTransform (camera);
        vis.render (tg, vis.fullBleed() ? r : safe, ctx);
    }

    // Composite to the screen, with Symmetry mirroring the left / top half.
    const auto toScreen = AffineTransform::scale (r.getWidth() / (float) iw, r.getHeight() / (float) ih);
    g.setImageResamplingQuality (Graphics::lowResamplingQuality);
    g.drawImageTransformed (trail, toScreen);
    const int sym = (int) param (ids::symmetry);
    if (sym > 0)
    {
        const float fw = (float) iw, fh = (float) ih;
        auto mirrored = [&] (Rectangle<float> clip, bool flipX, bool flipY)
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (clip.toNearestInt());
            g.drawImageTransformed (trail, AffineTransform::scale (flipX ? -1.0f : 1.0f, flipY ? -1.0f : 1.0f)
                                               .translated (flipX ? fw : 0.0f, flipY ? fh : 0.0f).followedBy (toScreen));
        };
        const float hx = r.getCentreX(), hy = r.getCentreY();
        if (sym == 1 || sym == 3) mirrored (r.withLeft (hx), true, false);
        if (sym == 2)             mirrored (r.withTop (hy), false, true);
        if (sym == 3)
        {
            mirrored (r.withTop (hy).withRight (hx), false, true);
            mirrored (r.withTop (hy).withLeft (hx), true, true);
        }
    }

    // Post effects
    if (const float st = param (ids::strobe); st > 0.001f && analyzer.beatPulse > 0.01f)
    {
        g.setColour (Colours::white.interpolatedWith (th.accent, 0.3f).withAlpha (jmin (0.85f, st * std::pow (analyzer.beatPulse, 3.0f))));
        g.fillRect (r);
    }
    if (const float vg = param (ids::vignette); vg > 0.001f)
    {
        ColourGradient vgr (Colours::black.withAlpha (0.0f), r.getCentre(), Colours::black.withAlpha (jmin (1.0f, vg * 1.1f)), r.getTopLeft(), true);
        vgr.addColour (0.55, Colours::black.withAlpha (vg * 0.15f));
        g.setGradientFill (vgr);
        g.fillRect (r);
    }
    if (const float sc = param (ids::scanlines); sc > 0.001f)
    {
        if (! scanTile.isValid())
        {
            scanTile = Image (Image::ARGB, 4, 3, true);
            for (int x = 0; x < 4; ++x) { scanTile.setPixelAt (x, 2, Colours::black); scanTile.setPixelAt (x, 1, Colours::black.withAlpha (0.35f)); }
        }
        g.setTiledImageFill (scanTile, 0, 0, jmin (1.0f, sc * 0.9f));
        g.fillRect (r);
        // a slow rolling bright band, like an old monitor
        const float bandY = r.getY() + frac01 (time * 0.07f) * (r.getHeight() + 200.0f) - 100.0f;
        ColourGradient roll (Colours::white.withAlpha (0.0f), 0, bandY - 60.0f, Colours::white.withAlpha (0.05f * sc), 0, bandY, false);
        roll.addColour (0.99, Colours::white.withAlpha (0.0f));
        g.setGradientFill (roll);
        g.fillRect (r.getX(), bandY - 60.0f, r.getWidth(), 61.0f);
    }

    auto& f = Fonts::get();
    if (bannerAlpha > 0.0f)
    {
        const float a = jmin (1.0f, bannerAlpha);
        const float h = jlimit (28.0f, 110.0f, r.getHeight() * 0.11f);
        auto box = r.withSizeKeepingCentre (r.getWidth(), h * 2.2f);
        g.setFont (Fonts::font (f.display, h));
        g.setColour (Colours::black.withAlpha (0.45f * a));
        g.drawText (bannerText, box.translated (0, 3).removeFromTop (h * 1.3f), Justification::centred);
        g.setColour (th.at (0.85f, huePhase).withAlpha (0.3f * a));
        g.drawText (bannerText, box.expanded (2).removeFromTop (h * 1.3f), Justification::centred);
        g.setColour (Colours::white.withAlpha (a));
        g.drawText (bannerText, box.removeFromTop (h * 1.3f), Justification::centred);
        if (bannerSub.isNotEmpty())
        {
            g.setFont (Fonts::font (f.bold, h * 0.28f, 0.2f));
            g.setColour (th.text.withAlpha (0.8f * a));
            g.drawText (bannerSub.toUpperCase(), box, Justification::centredTop);
        }
    }
    if (hintAlpha > 0.0f)
    {
        g.setFont (Fonts::font (f.medium, 13.0f, 0.05f));
        g.setColour (th.text.withAlpha (0.7f * jmin (1.0f, hintAlpha)));
        g.drawText (hintText, r.withTrimmedBottom (14).removeFromBottom (20), Justification::centred);
    }
}

void Canvas::mouseUp (const MouseEvent& e)
{
    if (e.mouseWasDraggedSinceMouseDown() || e.getNumberOfClicks() > 1) return;
    // Wait a moment so a double-click doesn't also count as a single click.
    const int token = ++clickToken;
    Component::SafePointer<Canvas> safe (this);
    Timer::callAfterDelay (260, [safe, token]
    {
        if (safe != nullptr && safe->clickToken == token && safe->onSingleClick) safe->onSingleClick();
    });
}

void Canvas::mouseDoubleClick (const MouseEvent&)
{
    ++clickToken;
    if (onDoubleClick) onDoubleClick();
}

//==============================================================================
TopBar::TopBar()
{
    const auto names = ids::modeNames();
    for (auto& grp : ids::modeGroups())
    {
        modeBox.addSectionHeading (grp.name);
        for (int i : grp.modes) modeBox.addItem (names[i], i + 1);
    }
    themeBox.addItemList (themeNames(), 1);
    cycleBox.addItemList (ids::cycleNames(), 1);
    for (auto* c : std::initializer_list<Component*> { &modePrev, &modeBox, &modeNext, &themePrev, &themeBox, &themeNext, &cycleBox, &fullBtn, &hideBtn })
        addAndMakeVisible (c);
    modeBox.setTooltip ("Visual mode (Left / Right arrow keys)");
    themeBox.setTooltip ("Colour theme (Up / Down arrow keys)");
    cycleBox.setTooltip ("Auto-cycle through the modes");
    fullBtn.setTooltip ("Full screen (F, or double-click the picture). Esc to exit.");
    hideBtn.setTooltip ("Hide the controls (H, or click the picture)");
}

void TopBar::paint (Graphics& g)
{
    auto* lnf = dynamic_cast<VQLookAndFeel*> (&getLookAndFeel());
    if (lnf == nullptr) return;
    const auto& th = lnf->getTheme();
    auto b = getLocalBounds().toFloat().reduced (4, 4);
    g.setColour (lnf->panel());
    g.fillRoundedRectangle (b, 14.0f);
    g.setColour (th.accent.withAlpha (0.25f));
    g.drawRoundedRectangle (b, 14.0f, 1.0f);

    auto logo = b.withWidth (150).withTrimmedLeft (14);
    ColourGradient grad (th.at (0.1f), logo.getX(), 0, th.at (0.9f), logo.getRight(), 0, false);
    g.setGradientFill (grad);
    g.setFont (Fonts::font (Fonts::get().display, 24.0f));
    g.drawText ("VisionQuest", logo, Justification::centredLeft);
}

void TopBar::resized()
{
    auto b = getLocalBounds().reduced (10, 11);
    b.removeFromLeft (158);
    auto group = [&b] (Button& prev, ComboBox& box, Button& next, int w)
    {
        prev.setBounds (b.removeFromLeft (26));
        box.setBounds (b.removeFromLeft (w).reduced (2, 0));
        next.setBounds (b.removeFromLeft (26));
        b.removeFromLeft (10);
    };
    group (modePrev, modeBox, modeNext, 168);
    group (themePrev, themeBox, themeNext, 148);
    cycleBox.setBounds (b.removeFromLeft (142));
    hideBtn.setBounds (b.removeFromRight (62));
    b.removeFromRight (8);
    fullBtn.setBounds (b.removeFromRight (112));
}

//==============================================================================
ChoiceSlot::ChoiceSlot (const String& name)
{
    label.setText (name.toUpperCase(), dontSendNotification);
    label.setJustificationType (Justification::centred);
    label.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (label);
    addAndMakeVisible (box);
}

void ChoiceSlot::resized()
{
    auto b = getLocalBounds();
    label.setBounds (b.removeFromTop (14));
    box.setBounds (b.withSizeKeepingCentre (b.getWidth() - 4, 26));
}

void ChoiceSlot::recolour (const Theme& t) { label.setColour (Label::textColourId, t.text.withAlpha (0.65f)); }

BottomBar::BottomBar()
{
    for (auto* k : knobs()) addAndMakeVisible (k);
    for (auto* k : motionKnobs()) addChildComponent (k);
    for (auto* k : colorKnobs()) addChildComponent (k);
    addChildComponent (symmetry);
    symmetry.box.addItemList (ids::symmetryNames(), 1);

    for (auto* t : { &mirror, &shuffle })
    {
        t->setClickingTogglesState (true);
        addAndMakeVisible (t);
    }
    addAndMakeVisible (dice);
    mirror.setTooltip ("Symmetric versions of each mode");
    shuffle.setTooltip ("Auto-cycle in random order, with random themes");
    dice.setTooltip ("Randomise the look: theme, motion and colour knobs (leaves Sensitivity, Smoothing, Tilt and Beat Sens alone)");

    int i = 0;
    for (auto* t : { &tabLook, &tabMotion, &tabColor })
    {
        t->setClickingTogglesState (true);
        t->setRadioGroupId (4711);
        const int pg = i++;
        t->onClick = [this, pg] { setPage (pg); };
        addAndMakeVisible (t);
    }
    tabLook.setToggleState (true, dontSendNotification);
}

void BottomBar::setPage (int p)
{
    page = jlimit (0, 2, p);
    for (auto* k : knobs()) k->setVisible (page == 0);
    for (auto* k : motionKnobs()) k->setVisible (page == 1);
    for (auto* k : colorKnobs()) k->setVisible (page == 2);
    symmetry.setVisible (page == 2);
    tabLook.setToggleState (page == 0, dontSendNotification);
    tabMotion.setToggleState (page == 1, dontSendNotification);
    tabColor.setToggleState (page == 2, dontSendNotification);
}

void BottomBar::recolour (const Theme& t)
{
    for (auto* k : knobs()) k->recolour (t);
    for (auto* k : motionKnobs()) k->recolour (t);
    for (auto* k : colorKnobs()) k->recolour (t);
    symmetry.recolour (t);
}

void BottomBar::paint (Graphics& g)
{
    auto* lnf = dynamic_cast<VQLookAndFeel*> (&getLookAndFeel());
    if (lnf == nullptr) return;
    auto b = getLocalBounds().toFloat().reduced (4, 4);
    g.setColour (lnf->panel());
    g.fillRoundedRectangle (b, 16.0f);
    g.setColour (lnf->getTheme().accent.withAlpha (0.25f));
    g.drawRoundedRectangle (b, 16.0f, 1.0f);
}

void BottomBar::resized()
{
    auto b = getLocalBounds().reduced (14, 10);
    auto tabs = b.removeFromLeft (76).reduced (0, 6);
    const int th = tabs.getHeight() / 3;
    tabLook.setBounds (tabs.removeFromTop (th).reduced (0, 2));
    tabMotion.setBounds (tabs.removeFromTop (th).reduced (0, 2));
    tabColor.setBounds (tabs.reduced (0, 2));
    b.removeFromLeft (6);

    auto toggles = b.removeFromRight (100).reduced (4, 6);
    const int tgh = toggles.getHeight() / 3;
    mirror.setBounds (toggles.removeFromTop (tgh).reduced (0, 2));
    shuffle.setBounds (toggles.removeFromTop (tgh).reduced (0, 2));
    dice.setBounds (toggles.reduced (0, 2));

    const int kw = b.getWidth() / 8;
    auto lay = [&] (auto list, bool withSymmetry)
    {
        auto row = b;
        for (auto* k : list) k->setBounds (row.removeFromLeft (kw));
        if (withSymmetry) symmetry.setBounds (row.removeFromLeft (kw).reduced (0, 10));
    };
    lay (knobs(), false);
    lay (motionKnobs(), false);
    lay (colorKnobs(), true);
}

//==============================================================================
Stage::Stage (VisionQuestProcessor& p)
    : proc (p), canvas (p),
      vblank (this, [this] (double stamp)
      {
          const double dt = lastStamp > 0 ? stamp - lastStamp : 1.0 / 60.0;
          lastStamp = stamp;
          tick ((float) dt);
      })
{
    setLookAndFeel (&lnf);
    setWantsKeyboardFocus (true);
    addAndMakeVisible (canvas);
    addAndMakeVisible (top);
    addAndMakeVisible (bottom);
    addMouseListener (this, true);

    auto& ap = proc.apvts;
    const char* knobIds[] = { ids::sensitivity, ids::smoothing, ids::speed, ids::trails, ids::glow, ids::react, ids::detail, ids::hueDrift };
    auto ks = bottom.knobs();
    for (size_t i = 0; i < ks.size(); ++i)
    {
        sliderAttachments.push_back (std::make_unique<SA> (ap, knobIds[i], ks[i]->slider));
        if (auto* prm = ap.getParameter (knobIds[i]))
            ks[i]->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
    }
    auto attachAll = [&] (auto list, std::initializer_list<const char*> pids)
    {
        auto it = pids.begin();
        for (auto* k : list)
        {
            const char* pid = *it++;
            sliderAttachments.push_back (std::make_unique<SA> (ap, pid, k->slider));
            if (auto* prm = ap.getParameter (pid))
                k->slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
        }
    };
    attachAll (bottom.motionKnobs(), { ids::zoom, ids::spin, ids::shake, ids::feedback, ids::twist, ids::strobe, ids::lineWidth, ids::beatSens });
    attachAll (bottom.colorKnobs(),  { ids::saturation, ids::brightness, ids::palShift, ids::scanlines, ids::vignette, ids::pixelate, ids::tilt });
    symmetryAttach = std::make_unique<CA> (ap, ids::symmetry, bottom.symmetry.box);
    bottom.dice.onClick = [this] { rollDice(); };
    for (auto& grp : ids::modeGroups()) for (int i : grp.modes) menuOrder.push_back (i);

    mirrorAttach = std::make_unique<BA> (ap, ids::mirror, bottom.mirror);
    shuffleAttach = std::make_unique<BA> (ap, ids::shuffle, bottom.shuffle);
    cycleAttach = std::make_unique<CA> (ap, ids::cycle, top.cycleBox);

    // Mode and theme boxes show what's on screen (which may be auto-cycled),
    // so they're driven by hand instead of attached.
    top.modeBox.onChange  = [this] { if (const int i = top.modeBox.getSelectedId() - 1; i >= 0 && i != canvas.getDisplayMode()) { setChoice (ids::mode, i); canvas.showMode (i); } };
    top.themeBox.onChange = [this] { if (const int i = top.themeBox.getSelectedItemIndex(); i >= 0 && i != canvas.getDisplayTheme()) { setChoice (ids::theme, i); canvas.showTheme (i); } };
    top.modePrev.onClick  = [this] { stepMode (-1); };
    top.modeNext.onClick  = [this] { stepMode (1); };
    top.themePrev.onClick = [this] { stepTheme (-1); };
    top.themeNext.onClick = [this] { stepTheme (1); };
    top.hideBtn.onClick   = [this] { setControlsVisible (false); canvas.flashHint (fullscreen ? "Move the mouse to bring the controls back" : "Click the picture to bring the controls back"); };
    top.fullBtn.onClick   = [this] { if (onToggleFullscreen) onToggleFullscreen(); };

    canvas.onSingleClick = [this] { if (! fullscreen) setControlsVisible (! controlsShown); };
    canvas.onDoubleClick = [this] { if (onToggleFullscreen) onToggleFullscreen(); };

    applyTheme (canvas.getDisplayTheme());
    top.modeBox.setSelectedId (canvas.getDisplayMode() + 1, dontSendNotification);
}

Stage::~Stage()
{
    setLookAndFeel (nullptr);
}

void Stage::setChoice (const char* id, int index)
{
    if (auto* prm = proc.apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) index));
        prm->endChangeGesture();
    }
}

void Stage::stepMode (int delta)
{
    // step in the order the menu shows, so the arrows walk through each group
    const int n = (int) menuOrder.size();
    const auto it = std::find (menuOrder.begin(), menuOrder.end(), canvas.getDisplayMode());
    const int pos = it == menuOrder.end() ? 0 : (int) (it - menuOrder.begin());
    const int i = menuOrder[(size_t) ((pos + delta + n) % n)];
    setChoice (ids::mode, i);
    canvas.showMode (i);
}

void Stage::rollDice()
{
    auto& ap = proc.apvts;
    auto& r = diceRng;
    auto set = [&ap] (const char* id, float v)
    {
        if (auto* prm = ap.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (v));
            prm->endChangeGesture();
        }
    };
    auto chance = [&r] (float p) { return r.nextFloat() < p; };

    // LOOK (not Sensitivity or Smoothing, which are about the input)
    set (ids::speed,    0.5f + 1.5f * r.nextFloat());
    set (ids::trails,   0.9f * r.nextFloat());
    set (ids::glow,     0.3f + 0.7f * r.nextFloat());
    set (ids::react,    0.3f + 0.7f * r.nextFloat());
    set (ids::detail,   0.2f + 0.8f * r.nextFloat());
    set (ids::hueDrift, chance (0.3f) ? 0.5f * r.nextFloat() : 0.0f);
    set (ids::mirror,   chance (0.35f) ? 1.0f : 0.0f);
    // MOTION (not Beat Sens)
    const bool fbOn = chance (0.4f);
    set (ids::zoom,      chance (0.3f) ? 0.8f + 0.5f * r.nextFloat() : 1.0f);
    set (ids::spin,      chance (0.35f) ? (r.nextFloat() - 0.5f) * 0.6f : 0.0f);
    set (ids::shake,     chance (0.3f) ? 0.6f * r.nextFloat() : 0.0f);
    set (ids::feedback,  fbOn ? 0.15f + 0.55f * r.nextFloat() : 0.0f);
    set (ids::twist,     fbOn && chance (0.6f) ? (r.nextFloat() - 0.5f) * 0.8f : 0.0f);
    set (ids::strobe,    chance (0.15f) ? 0.4f * r.nextFloat() : 0.0f);
    set (ids::lineWidth, 0.7f + 1.3f * r.nextFloat());
    // COLOR (not Tilt)
    set (ids::saturation, 0.7f + 0.8f * r.nextFloat());
    set (ids::brightness, 0.9f + 0.3f * r.nextFloat());
    set (ids::palShift,   chance (0.4f) ? r.nextFloat() : 0.0f);
    set (ids::scanlines,  chance (0.3f) ? 0.3f + 0.5f * r.nextFloat() : 0.0f);
    set (ids::vignette,   chance (0.5f) ? 0.7f * r.nextFloat() : 0.0f);
    set (ids::pixelate,   chance (0.12f) ? 0.2f + 0.4f * r.nextFloat() : 0.0f);
    set (ids::symmetry,   chance (0.25f) ? (float) (1 + r.nextInt (3)) : 0.0f);

    const int nt = (int) themes().size();
    const int theme = (canvas.getDisplayTheme() + 1 + r.nextInt (nt - 1)) % nt;
    setChoice (ids::theme, theme);
    canvas.showTheme (theme);
}

void Stage::stepTheme (int delta)
{
    const int n = (int) themes().size();
    const int i = (canvas.getDisplayTheme() + delta + n) % n;
    setChoice (ids::theme, i);
    canvas.showTheme (i);
}

void Stage::applyTheme (int index)
{
    appliedTheme = index;
    const auto& th = themes()[(size_t) index];
    lnf.setTheme (th);
    bottom.recolour (th);
    top.themeBox.setSelectedItemIndex (index, dontSendNotification);
    sendLookAndFeelChange();
    repaint();
}

void Stage::tick (float dt)
{
    canvas.tick (dt);
    if (canvas.getDisplayTheme() != appliedTheme) applyTheme (canvas.getDisplayTheme());
    if (top.modeBox.getSelectedId() != canvas.getDisplayMode() + 1)
        top.modeBox.setSelectedId (canvas.getDisplayMode() + 1, dontSendNotification);

    // In full screen the controls get out of the way after a few still seconds.
    if (fullscreen && controlsShown && ! top.modeBox.isPopupActive() && ! top.themeBox.isPopupActive() && ! top.cycleBox.isPopupActive())
        if ((idle += dt) > 3.0f) setControlsVisible (false);
}

void Stage::setControlsVisible (bool shouldShow)
{
    controlsShown = shouldShow;
    idle = 0;
    auto& anim = Desktop::getInstance().getAnimator();
    for (auto* c : std::initializer_list<Component*> { &top, &bottom })
    {
        if (shouldShow && ! c->isVisible()) anim.fadeIn (c, 180);
        else if (! shouldShow && c->isVisible()) anim.fadeOut (c, 250);
    }
    setMouseCursor (fullscreen && ! shouldShow ? MouseCursor::NoCursor : MouseCursor::NormalCursor);
    updateInsets();
}

void Stage::updateInsets()
{
    canvas.setSafeInsets (controlsShown ? (topH + 10.0f) * barScale : 0.0f,
                          controlsShown ? (bottomH + 10.0f) * barScale : 0.0f);
}

void Stage::setFullscreen (bool fs)
{
    fullscreen = fs;
    top.fullBtn.setButtonText (fs ? "EXIT FULL" : "FULL SCREEN");
    top.fullBtn.setToggleState (fs, dontSendNotification);
    setControlsVisible (true);
    if (fs) canvas.flashHint ("Esc or double-click to exit full screen");
}

void Stage::mouseMove (const MouseEvent&)
{
    idle = 0;
    if (fullscreen && ! controlsShown) setControlsVisible (true);
}

bool Stage::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::leftKey)  { stepMode (-1); return true; }
    if (k == KeyPress::rightKey) { stepMode (1); return true; }
    if (k == KeyPress::upKey)    { stepTheme (-1); return true; }
    if (k == KeyPress::downKey)  { stepTheme (1); return true; }
    const juce_wchar ch = CharacterFunctions::toLowerCase (k.getTextCharacter() != 0 ? k.getTextCharacter() : (juce_wchar) k.getKeyCode());
    if (ch == 'h') { setControlsVisible (! controlsShown); return true; }
    if (ch == 'f') { if (onToggleFullscreen) onToggleFullscreen(); return true; }
    if (ch == 'd') { rollDice(); return true; }
    if (ch >= '1' && ch <= '3') { bottom.setPage ((int) (ch - '1')); return true; }
    if (k == KeyPress::escapeKey && fullscreen) { if (onToggleFullscreen) onToggleFullscreen(); return true; }
    return false;
}

void Stage::resized()
{
    auto b = getLocalBounds();
    canvas.setBounds (b);

    // The bars are laid out at a fixed design width and scaled down to fit.
    const float s = jmin (1.0f, (float) b.getWidth() / (float) designW, (float) b.getHeight() / 420.0f);
    const float x = ((float) b.getWidth() - designW * s) * 0.5f;
    top.setBounds (0, 0, designW, topH);
    top.setTransform (AffineTransform::scale (s).translated (x, 6.0f * s));
    bottom.setBounds (0, 0, designW, bottomH);
    bottom.setTransform (AffineTransform::scale (s).translated (x, (float) b.getHeight() - (bottomH + 6.0f) * s));
    barScale = s;
    updateInsets();
}

//==============================================================================
FullscreenWindow::FullscreenWindow (Rectangle<int> area, std::function<void()> onExit)
    : exitFn (std::move (onExit))
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    setBounds (area);
    addToDesktop (ComponentPeer::windowAppearsOnTaskbar);
    setAlwaysOnTop (true);
    setVisible (true);
    toFront (true);
}

FullscreenWindow::~FullscreenWindow()
{
    removeFromDesktop();
}

void FullscreenWindow::resized()
{
    for (auto* c : getChildren()) c->setBounds (getLocalBounds());
}

bool FullscreenWindow::keyPressed (const KeyPress& k)
{
    if (k == KeyPress::escapeKey) { if (exitFn) exitFn(); return true; }
    return false;
}

} // namespace vq

//==============================================================================
VisionQuestEditor::VisionQuestEditor (VisionQuestProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    stage = std::make_unique<vq::Stage> (p);
    stage->onToggleFullscreen = [this] { toggleFullscreen(); };
    addAndMakeVisible (*stage);

    setResizable (true, true);
    setResizeLimits (640, 400, 3840, 2400);
    setSize (jlimit (640, 3840, proc.savedWidth), jlimit (400, 2400, proc.savedHeight));
}

VisionQuestEditor::~VisionQuestEditor()
{
    // Take the stage back before the full-screen window goes away.
    if (fsWindow != nullptr) { fsWindow->removeChildComponent (stage.get()); fsWindow.reset(); }
}

void VisionQuestEditor::paint (Graphics& g)
{
    g.fillAll (Colour (0xff0a0612));
    if (fsWindow != nullptr)
    {
        auto& f = vq::Fonts::get();
        auto b = getLocalBounds().toFloat().reduced (20);
        g.setColour (Colours::white);
        g.setFont (vq::Fonts::font (f.display, 34.0f));
        g.drawText ("VisionQuest", b.removeFromTop (b.getHeight() * 0.5f), Justification::centredBottom);
        g.setColour (Colours::white.withAlpha (0.7f));
        g.setFont (vq::Fonts::font (f.medium, 15.0f));
        g.drawFittedText ("Showing full screen" + fsLabel + ".\nPress Esc on that screen, or click here, to bring it back.",
                          b.toNearestInt().withTrimmedTop (10), Justification::centredTop, 3);
    }
}

void VisionQuestEditor::resized()
{
    if (fsWindow == nullptr) stage->setBounds (getLocalBounds());
    proc.savedWidth = getWidth();
    proc.savedHeight = getHeight();
}

void VisionQuestEditor::mouseUp (const MouseEvent&)
{
    if (fsWindow != nullptr) exitFullscreen();
}

void VisionQuestEditor::toggleFullscreen()
{
    if (fsWindow != nullptr) { exitFullscreen(); return; }

    const auto& displays = Desktop::getInstance().getDisplays().displays;
    if (displays.size() <= 1) { enterFullscreen (0); return; }

    // More than one screen: ask which one.
    PopupMenu m;
    m.setLookAndFeel (&stage->getLookAndFeel());
    for (int i = 0; i < displays.size(); ++i)
    {
        const auto& d = displays.getReference (i);
        const auto px = (d.logicalBounds * (float) d.scale).toNearestInt();
        m.addItem (i + 1, "Screen " + String (i + 1) + "  (" + String (px.getWidth()) + " x " + String (px.getHeight()) + ")"
                            + (d.isMain ? "  main" : ""));
    }
    Component::SafePointer<VisionQuestEditor> safe (this);
    m.showMenuAsync (PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe] (int result)
    {
        if (safe != nullptr && result > 0) safe->enterFullscreen (result - 1);
    });
}

void VisionQuestEditor::enterFullscreen (int displayIndex)
{
    const auto& displays = Desktop::getInstance().getDisplays().displays;
    if (displays.isEmpty()) return;
    const auto& d = displays.getReference (jlimit (0, displays.size() - 1, displayIndex));
    fsLabel = displays.size() > 1 ? " on screen " + String (displayIndex + 1) : String();

    Component::SafePointer<VisionQuestEditor> safe (this);
    fsWindow = std::make_unique<vq::FullscreenWindow> (d.logicalBounds.toNearestInt(), [safe]
    {
        if (safe != nullptr) safe->exitFullscreen();
    });
    fsWindow->addAndMakeVisible (*stage);
    fsWindow->resized();
    stage->setFullscreen (true);
    stage->grabKeyboardFocus();
    repaint();
}

void VisionQuestEditor::exitFullscreen()
{
    if (fsWindow == nullptr) return;
    addAndMakeVisible (*stage);
    stage->setBounds (getLocalBounds());
    stage->setFullscreen (false);
    fsWindow->setVisible (false);
    // Destroy the window after this callback returns: it may be the one calling us.
    MessageManager::callAsync ([w = std::shared_ptr<vq::FullscreenWindow> (fsWindow.release())] {});
    repaint();
}
