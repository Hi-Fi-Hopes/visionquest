// Local check tool: plays a synthetic track through the plugin and renders
// every mode to PNG, so the look can be checked without a DAW.
// Built only with -DVQ_BUILD_SNAPSHOT=ON.
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

namespace
{
    // ~124 BPM: kick, bassline, chord stabs, hats, stereo pad.
    struct Song
    {
        double sr = 48000; long long t = 0; Random rng { 3 };
        void next (float& l, float& r)
        {
            const double bt = 60.0 / 124.0, tt = (double) t / sr;
            const double pb = std::fmod (tt, bt);
            const double kf = 48.0 + 100.0 * std::exp (-pb * 35.0);
            const double kick = 0.85 * std::exp (-pb * 8.0) * std::sin (MathConstants<double>::twoPi * kf * pb);
            static const double notes[] = { 55.0, 55.0, 65.41, 49.0 };
            const double bassF = notes[(int) (tt / (bt * 4)) % 4];
            const double bass = 0.25 * std::sin (MathConstants<double>::twoPi * bassF * tt) * (0.6 + 0.4 * std::sin (tt * 7.0));
            const double off = std::fmod (tt + bt * 0.5, bt);
            const double hat = 0.10 * (rng.nextFloat() * 2 - 1) * std::exp (-off * 50.0);
            const double stabEnv = std::exp (-std::fmod (tt, bt * 2) * 5.0);
            double chord = 0;
            for (double f : { 220.0, 261.63, 329.63, 392.0 })
                chord += std::sin (MathConstants<double>::twoPi * f * tt + f);
            chord *= 0.05 * stabEnv;
            const double padL = 0.06 * std::sin (MathConstants<double>::twoPi * 440.0 * tt);
            const double padR = 0.06 * std::sin (MathConstants<double>::twoPi * 441.5 * tt + 1.0);
            const double shimmer = 0.03 * std::sin (MathConstants<double>::twoPi * 3520.0 * tt) * (0.5 + 0.5 * std::sin (tt * 3.0));
            l = (float) (kick + bass + hat + chord + padL + shimmer);
            r = (float) (kick + bass - hat * 0.7 + chord * 0.8 + padR + shimmer * 0.5);
            ++t;
        }
    };

    void setParam (VisionQuestProcessor& p, const char* id, float plain)
    {
        auto* prm = p.apvts.getParameter (id);
        prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
    }
}

int main (int argc, char** argv)
{
    ScopedJuceInitialiser_GUI init;
    const File outDir (argc > 1 ? String (argv[1]) : File::getCurrentWorkingDirectory().getFullPathName());
    outDir.createDirectory();

    VisionQuestProcessor proc;
    proc.setPlayConfigDetails (2, 2, 48000.0, 800);
    proc.prepareToPlay (48000.0, 800);
    std::unique_ptr<VisionQuestEditor> ed (dynamic_cast<VisionQuestEditor*> (proc.createEditor()));
    ed->setSize (960, 600);

    Song song;
    AudioBuffer<float> buf (2, 800);
    MidiBuffer midi;
    auto runFrames = [&] (int frames, bool paintEach)
    {
        for (int f = 0; f < frames; ++f)
        {
            for (int i = 0; i < 800; ++i) song.next (buf.getWritePointer (0)[i], buf.getWritePointer (1)[i]);
            proc.processBlock (buf, midi);
            ed->testTick (1.0f / 60.0f);
            if (paintEach) ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        }
    };
    auto save = [&] (const String& name, float scale)
    {
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
        FileOutputStream os (outDir.getChildFile (name));
        os.setPosition (0); os.truncate();
        PNGImageFormat().writeImageToStream (img, os);
        std::printf ("wrote %s\n", name.toRawUTF8());
    };

    const auto modes = vq::ids::modeNames();
    const int nThemes = (int) vq::themes().size();
    const bool onlyNew = argc > 2 && String (argv[2]) == "new";
    std::printf ("per-mode average paint (software renderer, 960x600):\n");
    for (int m = onlyNew ? 12 : 0; m < modes.size(); ++m)
    {
        setParam (proc, vq::ids::mode, (float) m);
        setParam (proc, vq::ids::theme, (float) ((m * 5) % nThemes));
        const double t0 = Time::getMillisecondCounterHiRes();
        runFrames (100, true);   // let the banner fade and the trails build
        std::printf ("  %-16s %6.2f ms\n", modes[m].toRawUTF8(), (Time::getMillisecondCounterHiRes() - t0) / 100.0);
        save (String::formatted ("mode%02d_", m) + modes[m].removeCharacters (" ") + ".png", 1.0f);
    }

    // new effects, one shot each
    struct Fx { const char* name; int mode; int theme; std::vector<std::pair<const char*, float>> params; };
    const std::vector<Fx> fx {
        { "fx_feedback_twist", 2, 4,  { { vq::ids::feedback, 0.6f }, { vq::ids::twist, 0.5f } } },
        { "fx_symmetry_quad",  20, 9, { { vq::ids::symmetry, 3.0f } } },
        { "fx_pixelate",       10, 0, { { vq::ids::pixelate, 0.5f } } },
        { "fx_scan_vignette",  13, 0, { { vq::ids::scanlines, 0.8f }, { vq::ids::vignette, 0.7f } } },
        { "fx_zoom_spin",      0, 6,  { { vq::ids::zoom, 0.7f }, { vq::ids::spin, 0.6f } } },
        { "fx_palshift_sat",   12, 1, { { vq::ids::palShift, 0.5f }, { vq::ids::saturation, 1.8f }, { vq::ids::lineWidth, 2.5f } } },
    };
    for (auto& f : fx)
    {
        setParam (proc, vq::ids::mode, (float) f.mode);
        setParam (proc, vq::ids::theme, (float) f.theme);
        for (auto& [id, v] : f.params) setParam (proc, id, v);
        runFrames (90, true);
        save (String (f.name) + ".png", 1.0f);
        for (auto& [id, v] : f.params)
        {
            auto* prm = proc.apvts.getParameter (id);
            prm->setValueNotifyingHost (prm->getDefaultValue());
        }
    }

    // knob pages
    setParam (proc, vq::ids::mode, 0.0f);
    setParam (proc, vq::ids::theme, 1.0f);
    for (int pg = 1; pg <= 3; ++pg)
    {
        ed->getStage().keyPressed (KeyPress ('0' + pg));
        runFrames (30, true);
        save (String::formatted ("ui_page%d.png", pg), 1.0f);
    }
    ed->getStage().keyPressed (KeyPress ('1'));

    ed.reset();
    return 0;
}
