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
              param (ids::detail), param (ids::mirror) > 0.5f, huePhase, rng, r };
    const auto safe = r.withTrimmedTop (insetTop).withTrimmedBottom (insetBottom);

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
            const float perFrame = 1.0f - param (ids::trails) * 0.93f;
            const float alpha = 1.0f - std::pow (1.0f - perFrame, dt * 60.0f);
            if (alpha > 0.0f) drawBackground (tg, r, th, huePhase, alpha);
        }
        vis.render (tg, vis.fullBleed() ? r : safe, ctx);
    }

    g.setImageResamplingQuality (Graphics::lowResamplingQuality);
    g.drawImageTransformed (trail, AffineTransform::scale (r.getWidth() / (float) iw, r.getHeight() / (float) ih));

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
    modeBox.addItemList (ids::modeNames(), 1);
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
BottomBar::BottomBar()
{
    for (auto* k : knobs()) addAndMakeVisible (k);
    for (auto* t : { &mirror, &shuffle })
    {
        t->setClickingTogglesState (true);
        addAndMakeVisible (t);
    }
    mirror.setTooltip ("Symmetric versions of each mode");
    shuffle.setTooltip ("Auto-cycle in random order, with random themes");
}

void BottomBar::recolour (const Theme& t) { for (auto* k : knobs()) k->recolour (t); }

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
    auto b = getLocalBounds().reduced (16, 10);
    auto toggles = b.removeFromRight (110);
    const int kw = b.getWidth() / 8;
    for (auto* k : knobs()) k->setBounds (b.removeFromLeft (kw));
    toggles.reduce (6, 14);
    mirror.setBounds (toggles.removeFromTop (toggles.getHeight() / 2).reduced (0, 4));
    shuffle.setBounds (toggles.reduced (0, 4));
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
    mirrorAttach = std::make_unique<BA> (ap, ids::mirror, bottom.mirror);
    shuffleAttach = std::make_unique<BA> (ap, ids::shuffle, bottom.shuffle);
    cycleAttach = std::make_unique<CA> (ap, ids::cycle, top.cycleBox);

    // Mode and theme boxes show what's on screen (which may be auto-cycled),
    // so they're driven by hand instead of attached.
    top.modeBox.onChange  = [this] { if (const int i = top.modeBox.getSelectedItemIndex(); i >= 0 && i != canvas.getDisplayMode()) { setChoice (ids::mode, i); canvas.showMode (i); } };
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
    top.modeBox.setSelectedItemIndex (canvas.getDisplayMode(), dontSendNotification);
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
    const int n = ids::modeNames().size();
    const int i = (canvas.getDisplayMode() + delta + n) % n;
    setChoice (ids::mode, i);
    canvas.showMode (i);
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
    if (top.modeBox.getSelectedItemIndex() != canvas.getDisplayMode())
        top.modeBox.setSelectedItemIndex (canvas.getDisplayMode(), dontSendNotification);

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
    if (k.getTextCharacter() == 'h' || k.getTextCharacter() == 'H') { setControlsVisible (! controlsShown); return true; }
    if (k.getTextCharacter() == 'f' || k.getTextCharacter() == 'F') { if (onToggleFullscreen) onToggleFullscreen(); return true; }
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
