// Standalone test for the VisionQuest analysis core. No JUCE needed:
//   g++ -O2 -std=c++17 -I Source tests/analyzer_test.cpp -o analyzer_test && ./analyzer_test
#include "dsp/Analyzer.h"
#include "dsp/SampleTap.h"
#include <chrono>
#include <cstdio>
#include <random>

using namespace vq;

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf ("  FAIL: " __VA_ARGS__); std::printf ("\n"); } } while (0)

static constexpr double sr = 48000.0;
static constexpr float fps = 60.0f;
static constexpr int hop = (int) (sr / fps);   // 800 samples per frame

// Feeds a signal generator through a SampleTap frame by frame, like the plugin does.
struct Rig
{
    SampleTap tap;
    Analyzer an;
    AnalyzerSettings s;
    std::vector<float> L = std::vector<float> (Analyzer::fftSize), R = std::vector<float> (Analyzer::fftSize);
    std::vector<float> bl = std::vector<float> (hop), br = std::vector<float> (hop);
    uint64_t lastWritten = 0;
    long long t = 0;

    Rig() { an.prepare (sr); }

    template <typename Gen> void frame (Gen&& gen)
    {
        for (int i = 0; i < hop; ++i, ++t) gen (t, bl[(size_t) i], br[(size_t) i]);
        tap.push (bl.data(), br.data(), hop);
        const uint64_t w = tap.readLatest (L.data(), R.data(), Analyzer::fftSize);
        an.process (L.data(), R.data(), (int) (w - lastWritten), 1.0f / fps, s);
        lastWritten = w;
    }
};

static int argmaxBand (const Analyzer& a)
{
    int best = 0;
    for (int b = 1; b < Analyzer::numBands; ++b) if (a.bands[b] > a.bands[best]) best = b;
    return best;
}

static bool allFinite (const Analyzer& a)
{
    for (int b = 0; b < Analyzer::numBands; ++b)
        if (! std::isfinite (a.bands[b]) || ! std::isfinite (a.peaks[b]) || a.bands[b] < 0 || a.bands[b] > 1) return false;
    for (int i = 0; i < Analyzer::logSpecSize; ++i) if (! std::isfinite (a.logSpec[i])) return false;
    for (int i = 0; i < Analyzer::scopeSize; ++i)
        if (! std::isfinite (a.scopeL[i]) || ! std::isfinite (a.xyR[i])) return false;
    for (float v : { a.bass, a.mid, a.treble, a.level, a.vuL, a.vuR, a.peakL, a.flux, a.beatPulse })
        if (! std::isfinite (v)) return false;
    return true;
}

int main()
{
    //------------------------------------------------------------------ 1. tap order
    std::printf ("1. sample tap returns the newest samples in order\n");
    {
        SampleTap tap;
        std::vector<float> a (5000), b (5000);
        for (int i = 0; i < 5000; ++i) { a[(size_t) i] = (float) i; b[(size_t) i] = (float) -i; }
        tap.push (a.data(), b.data(), 5000);
        std::vector<float> l (100), r (100);
        const auto w = tap.readLatest (l.data(), r.data(), 100);
        CHECK (w == 5000, "written=%llu", (unsigned long long) w);
        CHECK (l[0] == 4900.0f && l[99] == 4999.0f && r[99] == -4999.0f, "l0=%g l99=%g", l[0], l[99]);
        // more than the ring holds
        std::vector<float> big (SampleTap::capacity + 777);
        for (size_t i = 0; i < big.size(); ++i) big[i] = (float) i;
        tap.push (big.data(), big.data(), (int) big.size());
        tap.readLatest (l.data(), r.data(), 100);
        CHECK (l[99] == (float) (big.size() - 1), "after wrap l99=%g", l[99]);
    }

    //------------------------------------------------------------------ 2. tone lands in the right band
    std::printf ("2. sine tones peak in the band that contains them\n");
    for (float f : { 60.0f, 220.0f, 1000.0f, 5000.0f, 12000.0f })
    {
        Rig rig;
        for (int k = 0; k < 30; ++k)
            rig.frame ([f] (long long t, float& l, float& r) { l = r = 0.5f * (float) std::sin (2 * kPi * f * t / sr); });
        const int b = argmaxBand (rig.an);
        const float c = rig.an.bandCentre[b];
        std::printf ("   %6.0f Hz -> band %2d (centre %7.1f Hz), value %.2f\n", f, b, c, rig.an.bands[b]);
        CHECK (std::abs (std::log2 (c / f)) < 0.25f, "%.0f Hz landed at %.1f Hz", f, c);
        CHECK (rig.an.bands[b] > 0.6f, "level %.2f too low", rig.an.bands[b]);
    }

    //------------------------------------------------------------------ 3. silence
    std::printf ("3. silence reads as nothing\n");
    {
        Rig rig;
        for (int k = 0; k < 60; ++k) rig.frame ([] (long long, float& l, float& r) { l = r = 0.0f; });
        float mx = 0; for (float v : rig.an.bands) mx = std::max (mx, v);
        CHECK (mx < 0.01f, "max band %.3f", mx);
        CHECK (rig.an.beatCount == 0, "beats in silence: %d", rig.an.beatCount);
        CHECK (rig.an.level < 0.01f, "level %.3f", rig.an.level);
        CHECK (allFinite (rig.an), "non-finite in silence");
    }

    //------------------------------------------------------------------ 4. kick drum at 120 BPM
    std::printf ("4. beat detection on a 120 BPM kick with hats and a pad\n");
    {
        Rig rig;
        std::mt19937 rng (1);
        std::uniform_real_distribution<float> noise (-1, 1);
        const long long beatLen = (long long) (sr * 0.5);
        auto gen = [&] (long long t, float& l, float& r)
        {
            const long long p = t % beatLen;
            const double tt = p / sr;
            const double kickF = 50.0 + 90.0 * std::exp (-tt * 30.0);
            const float kick = (float) (0.9 * std::exp (-tt * 9.0) * std::sin (2 * kPi * kickF * tt));
            const long long hp = t % (beatLen / 2);
            const float hat = 0.12f * noise (rng) * (float) std::exp (-(hp / sr) * 60.0);
            const float pad = 0.08f * (float) (std::sin (2 * kPi * 330 * t / sr) + std::sin (2 * kPi * 415 * t / sr));
            l = kick + hat + pad; r = kick - hat * 0.5f + pad;
        };
        for (int k = 0; k < 60; ++k) rig.frame (gen);     // 1 s warm-up
        const int before = rig.an.beatCount;
        for (int k = 0; k < 600; ++k) rig.frame (gen);    // 10 s = 20 beats
        const int got = rig.an.beatCount - before;
        std::printf ("   detected %d beats in 10 s (expected 20)\n", got);
        CHECK (got >= 18 && got <= 22, "beats %d", got);
        CHECK (rig.an.bass > rig.an.treble, "bass %.2f treble %.2f", rig.an.bass, rig.an.treble);
    }

    //------------------------------------------------------------------ 5. steady tone: no false beats
    std::printf ("5. a steady bass tone does not trigger beats\n");
    {
        Rig rig;
        for (int k = 0; k < 120; ++k) rig.frame ([] (long long, float&, float&) {}); // silence first
        int startCount = 0;
        for (int k = 0; k < 600; ++k)
        {
            rig.frame ([] (long long t, float& l, float& r) { l = r = 0.6f * (float) std::sin (2 * kPi * 80 * t / sr); });
            if (k == 30) startCount = rig.an.beatCount;   // the onset itself may count once
        }
        CHECK (rig.an.beatCount - startCount == 0, "false beats on steady tone: %d", rig.an.beatCount - startCount);
    }

    //------------------------------------------------------------------ 6. VU calibration
    std::printf ("6. VU reads the RMS level\n");
    {
        Rig rig;
        for (int k = 0; k < 120; ++k)
            rig.frame ([] (long long t, float& l, float& r) { l = 0.5f * (float) std::sin (2 * kPi * 1000 * t / sr); r = 0.125f * (float) std::sin (2 * kPi * 1000 * t / sr); });
        std::printf ("   L %.1f dBFS (expect -9.0), R %.1f dBFS (expect -21.1)\n", rig.an.vuL, rig.an.vuR);
        CHECK (std::abs (rig.an.vuL - -9.03f) < 0.5f, "vuL %.2f", rig.an.vuL);
        CHECK (std::abs (rig.an.vuR - -21.07f) < 0.5f, "vuR %.2f", rig.an.vuR);
        CHECK (rig.an.peakL > 0.45f && rig.an.peakL < 0.55f, "peakL %.2f", rig.an.peakL);
    }

    //------------------------------------------------------------------ 7. scope trigger
    std::printf ("7. scope trace starts on a rising zero crossing\n");
    {
        Rig rig;
        for (int k = 0; k < 10; ++k)
            rig.frame ([] (long long t, float& l, float& r) { l = r = 0.7f * (float) std::sin (2 * kPi * 440 * t / sr + 1.0); });
        CHECK (std::abs (rig.an.scopeL[0]) < 0.06f && rig.an.scopeL[3] > 0.0f, "scope[0]=%.3f scope[3]=%.3f", rig.an.scopeL[0], rig.an.scopeL[3]);
    }

    //------------------------------------------------------------------ 8. garbage and extremes
    std::printf ("8. NaN, inf, clipping and random settings stay finite and bounded\n");
    {
        Rig rig;
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> u (0, 1);
        for (int k = 0; k < 900; ++k)
        {
            rig.s.sensitivityDb = -12.0f + 36.0f * u (rng);
            rig.s.smoothing = u (rng);
            const int kind = k % 5;
            rig.frame ([&] (long long t, float& l, float& r)
            {
                if (kind == 0) { l = NAN; r = INFINITY; }
                else if (kind == 1) { l = r = (t % 2) ? 40.0f : -40.0f; }
                else { l = 2 * u (rng) - 1; r = 2 * u (rng) - 1; }
            });
            if (! allFinite (rig.an)) { CHECK (false, "non-finite at frame %d", k); break; }
        }
    }

    //------------------------------------------------------------------ 9. beat sensitivity knob
    std::printf ("9. Beat Sens: low setting ignores soft kicks, high setting catches them\n");
    {
        auto run = [] (float sens)
        {
            Rig rig;
            rig.s.beatSens = sens;
            const long long beatLen = (long long) (sr * 0.5);
            auto gen = [&] (long long t, float& l, float& r)
            {
                const long long p = t % beatLen;
                const double tt = p / sr;
                // soft kick over a steady bass bed: only ~1.6x energy jump
                const float kick = (float) (0.12 * std::exp (-tt * 12.0) * std::sin (2 * kPi * 60.0 * tt));
                const float bed = 0.25f * (float) std::sin (2 * kPi * 70.0 * t / sr);
                l = r = kick + bed;
            };
            for (int k = 0; k < 60; ++k) rig.frame (gen);
            const int before = rig.an.beatCount;
            for (int k = 0; k < 600; ++k) rig.frame (gen);
            return rig.an.beatCount - before;
        };
        const int lo = run (0.0f), def = run (0.65f), hi = run (1.0f);
        std::printf ("   beats in 10 s: sens 0 -> %d, 0.65 -> %d, 1 -> %d\n", lo, def, hi);
        CHECK (lo <= def && def <= hi, "not monotonic");
        CHECK (hi > lo, "knob has no effect");
    }

    //------------------------------------------------------------------ 10. tilt
    std::printf ("10. Tilt: more tilt lifts the highs relative to the lows\n");
    {
        auto run = [] (float tilt)
        {
            Rig rig; rig.s.tiltDbPerOct = tilt;
            for (int k = 0; k < 30; ++k)
                rig.frame ([] (long long t, float& l, float& r) { l = r = 0.2f * (float) (std::sin (2 * kPi * 100 * t / sr) + std::sin (2 * kPi * 8000 * t / sr)); });
            return rig.an.treble - rig.an.bass;
        };
        const float flat = run (-3.0f), steep = run (9.0f);
        std::printf ("   treble-bass: tilt -3 -> %.2f, tilt +9 -> %.2f\n", flat, steep);
        CHECK (steep > flat + 0.1f, "tilt has no effect");
    }

    //------------------------------------------------------------------ 11. CPU
    std::printf ("11. CPU cost per frame\n");
    {
        Rig rig;
        std::vector<float> L (Analyzer::fftSize), R (Analyzer::fftSize);
        for (int i = 0; i < Analyzer::fftSize; ++i) { L[(size_t) i] = (float) std::sin (i * 0.1); R[(size_t) i] = (float) std::cos (i * 0.07); }
        const int frames = 6000;
        const auto t0 = std::chrono::steady_clock::now();
        for (int k = 0; k < frames; ++k) rig.an.process (L.data(), R.data(), hop, 1.0f / 60, rig.s);
        const double us = std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - t0).count() / frames;
        std::printf ("   %.1f us per frame (%.2f%% of a 60 fps frame)\n", us, us / 16667.0 * 100.0);
        CHECK (us < 1500.0, "too slow: %.1f us", us);
    }

    std::printf (failures == 0 ? "\nALL PASSED\n" : "\n%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
