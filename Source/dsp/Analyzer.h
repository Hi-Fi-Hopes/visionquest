#pragma once
// VisionQuest analysis core. No JUCE in here, so it can be unit-tested with plain g++.
//
// Runs on the UI thread once per animation frame: takes the latest window of
// audio, and produces everything the visual modes draw from (bands, peaks,
// bass/mid/treble, VU levels, beat pulse, a triggered scope trace).

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdint>
#include <vector>

namespace vq
{

inline constexpr double kPi = 3.14159265358979323846;

//==============================================================================
// Plain iterative radix-2 FFT (complex, in place).
class FFT
{
public:
    explicit FFT (int order) : n (1 << order)
    {
        rev.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            int r = 0;
            for (int b = 0; b < order; ++b)
                if (i & (1 << b)) r |= 1 << (order - 1 - b);
            rev[(size_t) i] = r;
        }
        twiddle.resize ((size_t) n / 2);
        for (int i = 0; i < n / 2; ++i)
        {
            const double a = -2.0 * kPi * i / n;
            twiddle[(size_t) i] = { (float) std::cos (a), (float) std::sin (a) };
        }
    }

    int size() const { return n; }

    void forward (std::complex<float>* d) const
    {
        for (int i = 0; i < n; ++i)
            if (i < rev[(size_t) i]) std::swap (d[i], d[rev[(size_t) i]]);

        for (int len = 2; len <= n; len <<= 1)
        {
            const int half = len / 2, step = n / len;
            for (int i = 0; i < n; i += len)
                for (int j = 0; j < half; ++j)
                {
                    const auto w = twiddle[(size_t) (j * step)];
                    const auto u = d[i + j];
                    const auto v = d[i + j + half] * w;
                    d[i + j] = u + v;
                    d[i + j + half] = u - v;
                }
        }
    }

private:
    int n;
    std::vector<int> rev;
    std::vector<std::complex<float>> twiddle;
};

//==============================================================================
struct AnalyzerSettings
{
    float sensitivityDb = 0.0f;  // -12 .. +24, shifts the whole display up/down
    float smoothing = 0.5f;      // 0 = twitchy, 1 = slow and syrupy
    float tiltDbPerOct = 3.0f;   // spectrum tilt around 1 kHz; higher favours the highs
    float beatSens = 0.65f;      // 0 = only big hits count as beats, 1 = everything does
};

class Analyzer
{
public:
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;   // 2048
    static constexpr int numBands = 96;             // log-spaced, 30 Hz .. 18 kHz
    static constexpr int logSpecSize = 256;         // finer log-spaced row for the waterfall
    static constexpr int scopeSize = 1024;          // samples in the triggered scope trace

    Analyzer() : fft (fftOrder)
    {
        window.resize (fftSize);
        windowSum = 0.0f;
        for (int i = 0; i < fftSize; ++i)
        {
            window[(size_t) i] = 0.5f - 0.5f * (float) std::cos (2.0 * kPi * i / (fftSize - 1));
            windowSum += window[(size_t) i];
        }
        buf.resize (fftSize);
        mags.resize (fftSize / 2);
        prepare (44100.0);
    }

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 1000.0 ? newSampleRate : 44100.0;
        const double nyq = sampleRate * 0.5;
        const double lo = 30.0, hi = std::min (18000.0, nyq * 0.95);

        for (int b = 0; b <= numBands; ++b)
            bandEdge[b] = (float) (lo * std::pow (hi / lo, (double) b / numBands));
        for (int b = 0; b < numBands; ++b)
            bandCentre[b] = std::sqrt (bandEdge[b] * bandEdge[b + 1]);
        for (int i = 0; i < logSpecSize; ++i)
            logFreq[i] = (float) (lo * std::pow (hi / lo, (double) i / (logSpecSize - 1)));

        reset();
    }

    void reset()
    {
        std::fill (std::begin (bands), std::end (bands), 0.0f);
        std::fill (std::begin (rawBands), std::end (rawBands), 0.0f);
        std::fill (std::begin (peaks), std::end (peaks), 0.0f);
        std::fill (std::begin (peakHold), std::end (peakHold), 0.0f);
        std::fill (std::begin (logSpec), std::end (logSpec), 0.0f);
        std::fill (std::begin (scopeL), std::end (scopeL), 0.0f);
        std::fill (std::begin (scopeR), std::end (scopeR), 0.0f);
        bass = mid = treble = level = 0.0f;
        vuL = vuR = -60.0f; peakL = peakR = 0.0f;
        beat = false; beatPulse = 0.0f; beatCount = 0;
        bassAvg = 0.0f; prevBassEnergy = 0.0f; sinceBeat = 10.0f;
        flux = 0.0f;
    }

    double getSampleRate() const { return sampleRate; }

    // L/R: the latest fftSize samples (oldest first). newSamples: how many of
    // those arrived since the previous call. dt: seconds since the previous call.
    void process (const float* L, const float* R, int newSamples, float dt, const AnalyzerSettings& s)
    {
        dt = std::clamp (dt, 1.0e-4f, 0.25f);
        newSamples = std::clamp (newSamples, 0, fftSize);
        const float gain = std::pow (10.0f, s.sensitivityDb / 20.0f);
        tilt = s.tiltDbPerOct;

        //---------------------------------------------------------------- spectrum
        for (int i = 0; i < fftSize; ++i)
        {
            const float m = 0.5f * (L[i] + R[i]);
            buf[(size_t) i] = { sanitise (m) * window[(size_t) i], 0.0f };
        }
        fft.forward (buf.data());

        const float norm = 2.0f / windowSum;
        for (int k = 0; k < fftSize / 2; ++k)
            mags[(size_t) k] = std::abs (buf[(size_t) k]) * norm * gain;

        // Fast attack, release time set by the Smoothing knob.
        const float release = 0.04f + s.smoothing * 0.7f;
        const float attack = 0.012f + s.smoothing * 0.05f;
        const float aUp = 1.0f - std::exp (-dt / attack);
        const float aDown = 1.0f - std::exp (-dt / release);

        float fluxSum = 0.0f;
        for (int b = 0; b < numBands; ++b)
        {
            const float v = toUnit (bandMagnitude (bandEdge[b], bandEdge[b + 1], bandCentre[b]), bandCentre[b]);
            fluxSum += std::max (0.0f, v - rawBands[b]);
            rawBands[b] = v;
            bands[b] += (v - bands[b]) * (v > bands[b] ? aUp : aDown);

            if (bands[b] >= peaks[b]) { peaks[b] = bands[b]; peakHold[b] = 0.45f; }
            else if ((peakHold[b] -= dt) < 0.0f) peaks[b] = std::max (bands[b], peaks[b] - dt * 0.9f);
        }
        flux += (fluxSum / numBands * 8.0f - flux) * (1.0f - std::exp (-dt / 0.05f));

        const float lsA = 1.0f - std::exp (-dt / (0.02f + s.smoothing * 0.2f));
        for (int i = 0; i < logSpecSize; ++i)
        {
            const float v = toUnit (interpMagnitude (logFreq[i]), logFreq[i]);
            logSpec[i] += (v - logSpec[i]) * lsA;
        }

        // Bass / mid / treble, from the band values.
        auto rangeAvg = [this] (float f0, float f1)
        {
            float sum = 0.0f; int cnt = 0;
            for (int b = 0; b < numBands; ++b)
                if (bandCentre[b] >= f0 && bandCentre[b] < f1) { sum += bands[b]; ++cnt; }
            return cnt > 0 ? sum / (float) cnt : 0.0f;
        };
        const float a3 = 1.0f - std::exp (-dt / 0.06f);
        bass   += (rangeAvg (30.0f, 250.0f)     - bass)   * a3;
        mid    += (rangeAvg (250.0f, 4000.0f)   - mid)    * a3;
        treble += (rangeAvg (4000.0f, 20000.0f) - treble) * a3;

        //---------------------------------------------------------------- levels
        const int n = std::max (1, newSamples);
        double sl = 0.0, sr = 0.0; float pl = 0.0f, pr = 0.0f;
        for (int i = fftSize - n; i < fftSize; ++i)
        {
            const float l = sanitise (L[i]) * gain, r = sanitise (R[i]) * gain;
            sl += (double) l * l; sr += (double) r * r;
            pl = std::max (pl, std::abs (l)); pr = std::max (pr, std::abs (r));
        }
        const float rmsL = newSamples > 0 ? (float) std::sqrt (sl / n) : 0.0f;
        const float rmsR = newSamples > 0 ? (float) std::sqrt (sr / n) : 0.0f;
        const float vuA = 1.0f - std::exp (-dt / 0.3f / 0.9f);    // classic VU: ~300 ms
        vuL += (toDb (rmsL) - vuL) * vuA;
        vuR += (toDb (rmsR) - vuR) * vuA;
        peakL = pl > peakL ? pl : peakL * std::exp (-dt / 0.6f);
        peakR = pr > peakR ? pr : peakR * std::exp (-dt / 0.6f);

        const float lvTarget = std::clamp ((std::max (vuL, vuR) + 48.0f) / 48.0f, 0.0f, 1.0f);
        level += (lvTarget - level) * (1.0f - std::exp (-dt / 0.08f));

        //---------------------------------------------------------------- beats
        // Low-end energy (40-160 Hz) against its own running average.
        float e = 0.0f;
        {
            const int k0 = std::max (1, (int) std::floor (40.0 * fftSize / sampleRate));
            const int k1 = std::max (k0 + 1, (int) std::ceil (160.0 * fftSize / sampleRate));
            for (int k = k0; k <= k1 && k < fftSize / 2; ++k) e += mags[(size_t) k] * mags[(size_t) k];
        }
        sinceBeat += dt;
        beat = false;
        const bool loudEnough = e > 1.0e-5f;
        const float threshold = 2.0f - 0.85f * std::clamp (s.beatSens, 0.0f, 1.0f);   // 0.65 -> 1.45
        if (loudEnough && e > bassAvg * threshold && e > prevBassEnergy * 1.05f && sinceBeat > 0.22f)
        {
            beat = true;
            sinceBeat = 0.0f;
            ++beatCount;
            beatPulse = 1.0f;
        }
        else
        {
            beatPulse *= std::exp (-dt / 0.18f);
        }
        prevBassEnergy = e;
        bassAvg += (e - bassAvg) * (1.0f - std::exp (-dt / 0.9f));

        //---------------------------------------------------------------- scope
        // Trigger on the latest rising zero crossing that still leaves a full trace.
        int start = fftSize - scopeSize;
        for (int i = fftSize - scopeSize; i > 1; --i)
        {
            const float a = L[i - 1] + R[i - 1], b = L[i] + R[i];
            if (a < 0.0f && b >= 0.0f) { start = i; break; }
        }
        for (int i = 0; i < scopeSize; ++i)
        {
            scopeL[i] = std::clamp (sanitise (L[start + i]) * gain, -2.0f, 2.0f);
            scopeR[i] = std::clamp (sanitise (R[start + i]) * gain, -2.0f, 2.0f);
        }
        // Lissajous uses the very latest samples, untriggered.
        for (int i = 0; i < scopeSize; ++i)
        {
            xyL[i] = std::clamp (sanitise (L[fftSize - scopeSize + i]) * gain, -2.0f, 2.0f);
            xyR[i] = std::clamp (sanitise (R[fftSize - scopeSize + i]) * gain, -2.0f, 2.0f);
        }
    }

    // Band value with linear interpolation, for any count of display columns.
    float bandAt (float t) const
    {
        const float x = std::clamp (t, 0.0f, 1.0f) * (numBands - 1);
        const int i = (int) x;
        const int j = std::min (i + 1, numBands - 1);
        return bands[i] + (bands[j] - bands[i]) * (x - (float) i);
    }
    float peakAt (float t) const
    {
        const float x = std::clamp (t, 0.0f, 1.0f) * (numBands - 1);
        const int i = (int) x;
        const int j = std::min (i + 1, numBands - 1);
        return std::max (peaks[i], peaks[j]);
    }

    //---------------------------------------------------------------- outputs
    float bands[numBands] {};     // 0..1, smoothed
    float rawBands[numBands] {};  // 0..1, this frame
    float peaks[numBands] {};     // 0..1, falling peak caps
    float logSpec[logSpecSize] {};
    float bandCentre[numBands] {};
    float bass = 0, mid = 0, treble = 0, level = 0;  // 0..1
    float vuL = -60, vuR = -60;   // dBFS RMS with VU ballistics
    float peakL = 0, peakR = 0;   // linear
    float flux = 0;               // ~0..1, how much the spectrum is changing
    bool beat = false;            // true on the frame a beat lands
    float beatPulse = 0;          // 1 on a beat, decays toward 0
    int beatCount = 0;
    float scopeL[scopeSize] {}, scopeR[scopeSize] {};
    float xyL[scopeSize] {}, xyR[scopeSize] {};

    static constexpr float floorDb = -72.0f;

private:
    static float sanitise (float x) { return std::isfinite (x) ? std::clamp (x, -8.0f, 8.0f) : 0.0f; }
    static float toDb (float lin) { return 20.0f * std::log10 (std::max (lin, 1.0e-6f)); }

    // dB with a +3 dB/octave tilt around 1 kHz so music reads balanced.
    float toUnit (float mag, float freq) const
    {
        const float db = toDb (mag) + tilt * std::log2 (std::max (freq, 20.0f) / 1000.0f);
        return std::clamp ((db - floorDb) / -floorDb, 0.0f, 1.0f);
    }

    float interpMagnitude (float freq) const
    {
        const float x = (float) (freq * fftSize / sampleRate);
        const int k = std::clamp ((int) x, 0, fftSize / 2 - 2);
        const float f = std::clamp (x - (float) k, 0.0f, 1.0f);
        return mags[(size_t) k] + (mags[(size_t) k + 1] - mags[(size_t) k]) * f;
    }

    float bandMagnitude (float f0, float f1, float fc) const
    {
        const int k0 = (int) std::ceil (f0 * fftSize / sampleRate);
        const int k1 = (int) std::floor (f1 * fftSize / sampleRate);
        if (k1 < k0) return interpMagnitude (fc);   // narrower than one bin
        float m = 0.0f;
        for (int k = std::max (0, k0); k <= std::min (k1, fftSize / 2 - 1); ++k)
            m = std::max (m, mags[(size_t) k]);
        return m;
    }

    FFT fft;
    double sampleRate = 44100.0;
    std::vector<float> window, mags;
    std::vector<std::complex<float>> buf;
    float windowSum = 1.0f;
    float tilt = 3.0f;
    float bandEdge[numBands + 1] {};
    float logFreq[logSpecSize] {};
    float peakHold[numBands] {};
    float bassAvg = 0, prevBassEnergy = 0, sinceBeat = 10;
};

} // namespace vq
