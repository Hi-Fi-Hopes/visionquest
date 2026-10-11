#include "ui/Visuals.h"
#include "ui/Fonts.h"
#include "ui/VisualHelpers.h"

using namespace juce;

namespace vq
{

using namespace detail;

void setLineWidthScale (float s) { lineScale = jlimit (0.1f, 5.0f, s); }

void drawBackground (Graphics& g, Rectangle<float> r, const Theme& th, float hue, float alpha)
{
    auto inner = th.bg2.withRotatedHue (hue), outer = th.bg.withRotatedHue (hue);
    ColourGradient grad (inner.withAlpha (alpha), r.getCentre(), outer.withAlpha (alpha), r.getTopLeft(), true);
    g.setGradientFill (grad);
    g.fillRect (r);
}

//==============================================================================
// 1. Spectrum bars with falling peak caps and a reflection.
class Bars : public Visual
{
public:
    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const int n = 16 + (int) (c.detail * 80.0f);
        const bool m = c.mirror;
        const float baseY = m ? r.getCentreY() : r.getY() + r.getHeight() * 0.76f;
        const float maxH = (m ? 0.44f : 0.68f) * r.getHeight();
        const float slot = r.getWidth() / (float) n;
        const float gap = slot * (n > 56 ? 0.18f : 0.26f);
        const float bw = slot - gap;
        const float bump = 1.0f + c.pulse() * 0.2f;

        // horizon
        g.setColour (c.th.accent.withRotatedHue (c.hue).withAlpha (0.25f));
        g.fillRect (r.getX(), baseY, r.getWidth(), 1.0f);

        for (int i = 0; i < n; ++i)
        {
            const float u = ((float) i + 0.5f) / (float) n;
            const float t = m ? std::abs (u * 2.0f - 1.0f) : u;
            const float v = a.bandAt (t);
            const float pk = a.peakAt (t);
            const float h = jmax (2.0f, v * maxH * bump);
            const float x = r.getX() + (float) i * slot + gap * 0.5f;
            const auto top = c.col (t * 0.8f + v * 0.2f);
            const auto bottom = c.col (t * 0.8f, 0.55f);

            auto drawBar = [&] (float y0, float y1, float alphaMul)
            {
                const Rectangle<float> bar (x, jmin (y0, y1), bw, std::abs (y1 - y0));
                if (c.glow > 0.01f)
                {
                    g.setColour (top.withMultipliedAlpha (0.16f * c.glow * alphaMul * (0.3f + v)));
                    g.fillRoundedRectangle (bar.expanded (slot * 0.5f * c.glow), bw * 0.5f);
                }
                ColourGradient grad (bottom.withMultipliedAlpha (alphaMul), x, y0, top.withMultipliedAlpha (alphaMul), x, y1, false);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (bar, jmin (3.0f, bw * 0.3f));
            };

            drawBar (baseY, baseY - h, 1.0f);
            const float capY = baseY - jmax (pk * maxH * bump, h) - 4.0f;
            g.setColour (c.th.text.withAlpha (0.85f));
            g.fillRoundedRectangle (x, capY, bw, 2.5f, 1.0f);

            if (m)
            {
                drawBar (baseY, baseY + h, 1.0f);
                g.fillRoundedRectangle (x, baseY + jmax (pk * maxH * bump, h) + 1.5f, bw, 2.5f, 1.0f);
            }
            else
            {
                // reflection on the floor
                const float rh = jmin (h * 0.45f, r.getBottom() - baseY - 2.0f);
                ColourGradient grad (top.withAlpha (0.22f), x, baseY + 2, top.withAlpha (0.0f), x, baseY + 2 + rh, false);
                g.setGradientFill (grad);
                g.fillRect (x, baseY + 2.0f, bw, rh);
            }
        }
    }
};

//==============================================================================
// 2. Oscilloscope with a CRT graticule.
class Scope : public Visual
{
public:
    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const float cy = r.getCentreY();
        const auto ink = c.th.text;

        for (int i = 1; i < 10; ++i)
        {
            g.setColour (ink.withAlpha (i == 5 ? 0.12f : 0.05f));
            g.fillRect (r.getX() + r.getWidth() * (float) i / 10.0f, r.getY(), 1.0f, r.getHeight());
        }
        for (int i = 1; i < 8; ++i)
        {
            g.setColour (ink.withAlpha (i == 4 ? 0.12f : 0.05f));
            g.fillRect (r.getX(), r.getY() + r.getHeight() * (float) i / 8.0f, r.getWidth(), 1.0f);
        }

        const float amp = r.getHeight() * 0.36f * (1.0f + c.pulse() * 0.25f);
        const int span = jlimit (128, Analyzer::scopeSize, (int) ((float) Analyzer::scopeSize / jmax (0.5f, c.speed)));
        const int pts = jlimit (64, 600, (int) (r.getWidth() / 1.5f));
        const float w = 1.4f + 2.6f * a.level;

        auto trace = [&] (auto sampleFn, float sign)
        {
            Path p;
            for (int i = 0; i < pts; ++i)
            {
                const int s = i * (span - 1) / (pts - 1);
                const float x = r.getX() + r.getWidth() * (float) i / (float) (pts - 1);
                const float y = cy - sign * sampleFn (s) * amp;
                if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            return p;
        };

        if (c.mirror)
        {
            auto mid = [&a] (int s) { return 0.5f * (a.scopeL[s] + a.scopeR[s]); };
            glowStroke (g, trace (mid, -1.0f), c.col (0.25f, 0.8f), w, c.glow);
            glowStroke (g, trace (mid, 1.0f), c.col (0.85f), w, c.glow);
        }
        else
        {
            glowStroke (g, trace ([&a] (int s) { return a.scopeR[s]; }, 1.0f), c.col (0.8f, 0.85f), w, c.glow);
            glowStroke (g, trace ([&a] (int s) { return a.scopeL[s]; }, 1.0f), c.col (0.3f), w, c.glow);
        }
    }
};

//==============================================================================
// 3. Radial burst: spectrum as spokes around a pulsing core, with beat ripples.
class Radial : public Visual
{
public:
    void reset() override { rot = 0; ripples.clear(); }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        rot += c.dt * c.speed * (0.15f + 0.6f * a.treble);
        const auto ctr = r.getCentre();
        const float md = jmin (r.getWidth(), r.getHeight());
        const float r0 = md * (0.15f + 0.05f * a.bass + 0.07f * c.pulse());
        const int n = 48 + (int) (c.detail * 144.0f);
        const int folds = c.mirror ? 4 : 2;

        if (a.beat && c.react > 0.01f) ripples.push_back ({ r0, 1.0f });
        for (auto& rp : ripples)
        {
            rp.radius += c.dt * md * 0.55f * c.speed;
            rp.alpha -= c.dt * 0.9f;
            Path p; p.addEllipse (ctr.x - rp.radius, ctr.y - rp.radius, rp.radius * 2, rp.radius * 2);
            glowStroke (g, p, c.col (0.6f, jmax (0.0f, rp.alpha) * c.react), 2.0f, c.glow);
        }
        ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [] (auto& rp) { return rp.alpha <= 0; }), ripples.end());

        Path outline;
        const float spokeW = jmax (1.5f, twoPi * r0 / (float) n * 0.55f);
        for (int i = 0; i < n; ++i)
        {
            const float u = (float) i / (float) n;
            const float t = fold (u, folds);
            const float v = a.bandAt (t);
            const float ang = rot + u * twoPi;
            const Point<float> dir (std::cos (ang), std::sin (ang));
            const float len = v * md * 0.3f * (1.0f + c.pulse() * 0.35f);
            const auto p0 = ctr + dir * (r0 + 3.0f), p1 = ctr + dir * (r0 + 3.0f + len);
            const auto col = c.col (t * 0.85f + v * 0.15f);
            if (c.glow > 0.01f)
            {
                g.setColour (col.withMultipliedAlpha (0.18f * c.glow));
                g.drawLine ({ p0, p1 }, spokeW * (1.0f + 2.5f * c.glow));
            }
            g.setColour (col);
            g.drawLine ({ p0, p1 }, spokeW);
            const auto tip = ctr + dir * (r0 + 10.0f + len * 1.08f);
            if (i == 0) outline.startNewSubPath (tip); else outline.lineTo (tip);
        }
        outline.closeSubPath();
        glowStroke (g, outline, c.col (0.95f, 0.7f), 1.5f, c.glow * 0.7f);

        // core
        const float cr = r0 * 0.92f;
        ColourGradient core (c.col (0.7f, 0.25f + 0.6f * c.pulse() + 0.2f * a.level), ctr,
                             c.col (0.2f, 0.0f), ctr.translated (cr, 0), true);
        g.setGradientFill (core);
        g.fillEllipse (ctr.x - cr, ctr.y - cr, cr * 2, cr * 2);
        Path ring; ring.addEllipse (ctr.x - r0, ctr.y - r0, r0 * 2, r0 * 2);
        glowStroke (g, ring, c.col (0.5f), 2.0f, c.glow);
    }

private:
    struct Ripple { float radius, alpha; };
    std::vector<Ripple> ripples;
    float rot = 0;
};

//==============================================================================
// 4. Lissajous / goniometer: mono is a vertical line, wide stereo blooms out.
class Lissajous : public Visual
{
public:
    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const auto ctr = r.getCentre();
        const float md = jmin (r.getWidth(), r.getHeight());
        const float sc = md * 0.42f * (1.0f + c.pulse() * 0.15f);
        const auto ink = c.th.text;

        g.setColour (ink.withAlpha (0.07f));
        g.drawEllipse (ctr.x - sc, ctr.y - sc, sc * 2, sc * 2, 1.0f);
        g.drawLine (ctr.x - sc * 0.7071f, ctr.y - sc * 0.7071f, ctr.x + sc * 0.7071f, ctr.y + sc * 0.7071f, 1.0f);
        g.drawLine (ctr.x - sc * 0.7071f, ctr.y + sc * 0.7071f, ctr.x + sc * 0.7071f, ctr.y - sc * 0.7071f, 1.0f);
        g.setColour (ink.withAlpha (0.12f));
        g.drawLine (ctr.x, ctr.y - sc, ctr.x, ctr.y + sc, 1.0f);

        wobble += c.dt * c.speed * 0.4f;
        const float ang = std::sin (wobble) * 0.25f;
        const float ca = std::cos (ang), sa = std::sin (ang);
        constexpr int segs = 8;
        const int per = Analyzer::scopeSize / segs;
        const float w = 1.1f + 1.8f * a.level;

        for (int mirrorPass = 0; mirrorPass < (c.mirror ? 2 : 1); ++mirrorPass)
        {
            const float flip = mirrorPass == 0 ? 1.0f : -1.0f;
            for (int sgi = 0; sgi < segs; ++sgi)
            {
                Path p;
                for (int i = sgi * per; i <= jmin (Analyzer::scopeSize - 1, (sgi + 1) * per); ++i)
                {
                    const float x0 = (a.xyR[i] - a.xyL[i]) * 0.7071f * flip;
                    const float y0 = (a.xyL[i] + a.xyR[i]) * 0.7071f;
                    const float x = ctr.x + (x0 * ca - y0 * sa) * sc;
                    const float y = ctr.y - (x0 * sa + y0 * ca) * sc;
                    if (i == sgi * per) p.startNewSubPath (x, y); else p.lineTo (x, y);
                }
                glowStroke (g, p, c.col ((float) sgi / (segs - 1), 0.75f), w, c.glow);
            }
        }
    }

private:
    float wobble = 0;
};

//==============================================================================
// 5. Waterfall spectrogram with a live spectrum ridge on top.
class Waterfall : public Visual
{
public:
    bool usesTrails() const override { return false; }
    void reset() override { img = {}; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        constexpr int W = Analyzer::logSpecSize, H = 240;
        if (! img.isValid())
        {
            img = Image (Image::RGB, W, H, true, SoftwareImageType());
            Graphics ig (img); ig.fillAll (c.th.bg.withRotatedHue (c.hue));
            head = 0;
        }

        // palette lookup for this frame
        Colour lut[256];
        const auto bg = c.th.bg.withRotatedHue (c.hue);
        for (int i = 0; i < 256; ++i)
        {
            const float v = (float) i / 255.0f;
            lut[i] = bg.interpolatedWith (c.col (v), jmin (1.0f, v * 1.6f));
        }

        rowAcc += c.dt * 60.0f * c.speed;
        int rows = jmin (H, (int) rowAcc);
        rowAcc -= (float) rows;
        while (rows-- > 0)
        {
            head = (head + H - 1) % H;   // newest row sits at 'head', older rows follow
            Image::BitmapData bd (img, 0, head, W, 1, Image::BitmapData::writeOnly);
            for (int x = 0; x < W; ++x)
            {
                const float u = ((float) x + 0.5f) / (float) W;
                const float t = c.mirror ? std::abs (u * 2.0f - 1.0f) : u;
                const float v = specAt (c.a, t);
                bd.setPixelColour (x, 0, lut[jlimit (0, 255, (int) (v * 255.0f))]);
            }
        }

        drawBackground (g, r, c.th, c.hue, 1.0f);
        const float topH = r.getHeight() * 0.26f;
        auto fall = r.withTrimmedTop (topH);

        g.setImageResamplingQuality (Graphics::mediumResamplingQuality);
        const float rowH = fall.getHeight() / (float) H;
        const int firstLen = H - head;   // rows head..H-1 are newest..older
        g.drawImage (img, (int) fall.getX(), (int) fall.getY(), (int) fall.getWidth(), roundToInt (rowH * (float) firstLen),
                     0, head, W, firstLen);
        if (head > 0)
            g.drawImage (img, (int) fall.getX(), (int) (fall.getY() + rowH * (float) firstLen), (int) fall.getWidth(),
                         (int) std::ceil (rowH * (float) head), 0, 0, W, head);

        // fade the far end
        ColourGradient fade (bg.withAlpha (0.0f), 0, fall.getY() + fall.getHeight() * 0.55f, bg, 0, fall.getBottom(), false);
        g.setGradientFill (fade);
        g.fillRect (fall);

        // live ridge
        Path ridge, fill;
        const int pts = 200;
        const float base = r.getY() + topH;
        for (int i = 0; i < pts; ++i)
        {
            const float u = (float) i / (float) (pts - 1);
            const float t = c.mirror ? std::abs (u * 2.0f - 1.0f) : u;
            const float v = specAt (c.a, t) * (1.0f + c.pulse() * 0.25f);
            const float x = r.getX() + u * r.getWidth();
            const float y = base - v * topH * 0.88f;
            if (i == 0) { ridge.startNewSubPath (x, y); fill.startNewSubPath (x, base); }
            ridge.lineTo (x, y); fill.lineTo (x, y);
        }
        fill.lineTo (r.getRight(), base); fill.closeSubPath();
        ColourGradient fg (c.col (0.9f, 0.45f), 0, r.getY(), c.col (0.2f, 0.05f), 0, base, false);
        g.setGradientFill (fg);
        g.fillPath (fill);
        glowStroke (g, ridge, c.col (0.85f), 2.0f, c.glow);
    }

private:
    static float specAt (const Analyzer& a, float t)
    {
        const float x = jlimit (0.0f, 1.0f, t) * (Analyzer::logSpecSize - 1);
        const int i = (int) x, j = jmin (i + 1, Analyzer::logSpecSize - 1);
        return a.logSpec[i] + (a.logSpec[j] - a.logSpec[i]) * (x - (float) i);
    }

    Image img;
    int head = 0;
    float rowAcc = 0;
};

//==============================================================================
// 6. Particle storm: a fountain from the centre, bursting on every beat.
class Particles : public Visual
{
public:
    bool fullBleed() const override { return true; }
    void reset() override { ps.clear(); acc = 0; swirl = 0; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const auto ctr = r.getCentre();
        const float md = jmin (r.getWidth(), r.getHeight());

        acc += c.dt * (15.0f + 420.0f * a.level * (0.3f + c.detail));
        int toEmit = (int) acc;
        acc -= (float) toEmit;
        if (a.beat) toEmit += (int) ((40.0f + 260.0f * c.detail) * c.react);
        for (int k = 0; k < toEmit && (int) ps.size() < maxParticles; ++k)
            emit (ctr, md, c);

        swirl = c.dt * c.speed * (0.5f + 1.5f * a.mid);
        const float cs = std::cos (swirl), sn = std::sin (swirl);
        const float drag = std::exp (-c.dt * 0.7f);

        for (auto& p : ps)
        {
            const float vx = p.vx * cs - p.vy * sn, vy = p.vx * sn + p.vy * cs;
            p.vx = vx * drag; p.vy = vy * drag;
            p.x += p.vx * c.dt * c.speed;
            p.y += p.vy * c.dt * c.speed;
            p.life -= c.dt;
        }
        ps.erase (std::remove_if (ps.begin(), ps.end(), [&r] (const P& p)
                  { return p.life <= 0 || ! r.expanded (40).contains (p.x, p.y); }), ps.end());

        for (auto& p : ps)
        {
            const float f = jlimit (0.0f, 1.0f, p.life / p.maxLife);
            const float sz = p.size * (0.4f + 0.6f * f) * (1.0f + c.pulse() * 0.5f);
            glowDot (g, { p.x, p.y }, sz, c.col (p.t, f), c.glow);
        }

        glowDot (g, ctr, md * (0.012f + 0.03f * a.bass + 0.03f * c.pulse()), c.col (0.9f, 0.9f), c.glow);
    }

private:
    struct P { float x, y, vx, vy, life, maxLife, t, size; };

    void emit (Point<float> ctr, float md, const Ctx& c)
    {
        const float t = c.rng.nextFloat();
        const float v = c.a.bandAt (t);
        const float ang = c.rng.nextFloat() * twoPi;
        const float sp = md * (0.08f + 0.9f * v * (0.4f + 0.6f * c.rng.nextFloat()));
        P p { ctr.x, ctr.y, std::cos (ang) * sp, std::sin (ang) * sp, 0, 0, t, 1.2f + 4.0f * v * c.rng.nextFloat() + md * 0.002f };
        p.maxLife = p.life = 1.0f + 1.8f * c.rng.nextFloat();
        ps.push_back (p);
        if (c.mirror) { p.vx = -p.vx; ps.push_back (p); }
    }

    static constexpr int maxParticles = 2600;
    std::vector<P> ps;
    float acc = 0, swirl = 0;
};

//==============================================================================
// 7. Warp tunnel: rings rush toward you, each one shaped by the spectrum at birth.
class Tunnel : public Visual
{
public:
    bool fullBleed() const override { return true; }
    Tunnel() { reset(); }

    void reset() override
    {
        rings.clear();
        for (int i = 0; i < numRings; ++i)
        {
            Ring rg; rg.z = (float) (i + 1) / (float) numRings; rg.hueT = frac ((float) i * 0.07f); rg.rot = (float) i * 0.3f;
            std::fill (std::begin (rg.shape), std::end (rg.shape), 0.0f);
            rings.push_back (rg);
        }
        spawned = numRings;
    }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const float md = jmin (r.getWidth(), r.getHeight());
        swayT += c.dt * c.speed;
        const Point<float> vp (r.getCentreX() + std::sin (swayT * 0.7f) * r.getWidth() * 0.06f,
                               r.getCentreY() + std::cos (swayT * 0.5f) * r.getHeight() * 0.06f);
        const float vz = c.dt * c.speed * (0.12f + 0.7f * a.level + 0.9f * c.pulse());

        for (auto& rg : rings)
        {
            rg.z -= vz;
            if (rg.z <= 0.03f)
            {
                rg.z += 1.0f;
                rg.hueT = frac ((float) spawned++ * 0.07f);
                rg.rot = swayT * 0.4f;
                for (int k = 0; k < shapeSize; ++k)
                    rg.shape[k] = a.bandAt ((float) k / (float) (shapeSize - 1));
            }
        }
        std::sort (rings.begin(), rings.end(), [] (const Ring& x, const Ring& y) { return x.z > y.z; });

        // speed lines
        for (int i = 0; i < 16; ++i)
        {
            const float ang = (float) i / 16.0f * twoPi + swayT * 0.1f;
            g.setColour (c.col ((float) i / 16.0f, 0.05f + 0.1f * a.level));
            g.drawLine (vp.x, vp.y, vp.x + std::cos (ang) * md * 1.5f, vp.y + std::sin (ang) * md * 1.5f, 1.0f);
        }

        const int sides = 48 + (int) (c.detail * 48.0f);
        for (auto& rg : rings)
        {
            const float rad = md * 0.07f / rg.z;
            const float alpha = jmin (1.0f, (1.0f - rg.z) / 0.25f) * jmin (1.0f, rg.z / 0.12f);
            if (alpha <= 0.01f) continue;
            Path p;
            for (int s = 0; s <= sides; ++s)
            {
                const float u = (float) s / (float) sides;
                const float t = fold (u, c.mirror ? 6 : 2);
                const float sv = rg.shape[jlimit (0, shapeSize - 1, (int) (t * (shapeSize - 1)))];
                const float rr = rad * (1.0f + 0.45f * sv * (0.4f + c.react));
                const float ang = u * twoPi + rg.rot;
                const float x = vp.x + std::cos (ang) * rr, y = vp.y + std::sin (ang) * rr;
                if (s == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
            }
            p.closeSubPath();
            glowStroke (g, p, c.col (rg.hueT, alpha), 0.8f + 3.0f * (1.0f - rg.z), c.glow);
        }
    }

private:
    static constexpr int numRings = 26, shapeSize = 32;
    struct Ring { float z, hueT, rot; float shape[shapeSize]; };
    std::vector<Ring> rings;
    int spawned = 0;
    float swayT = 0;
};

//==============================================================================
// 8. VU meters: two backlit needle meters on a silverface fascia.
class VUMeters : public Visual
{
public:
    bool usesTrails() const override { return false; }
    void reset() override { for (auto& n : needles) n = {}; lamp = 0; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        // 0 VU = -10 dBFS RMS, so a loud mix swings around 0 (use Sensitivity
        // for quieter tracks). Needle position follows voltage, like a real
        // meter, so -20 sits near the left stop.
        auto target = [] (float dbfs)
        {
            const float vu = dbfs + 10.0f;
            return jlimit (0.0f, 1.12f, std::pow (10.0f, vu / 20.0f) / std::pow (10.0f, 3.0f / 20.0f));
        };
        const float tl = target (a.vuL), tr = target (a.vuR);
        const float dt = jmin (c.dt, 0.05f);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& nd = needles[ch];
            const float tgt = ch == 0 ? tl : tr;
            // slightly under-damped spring: a little overshoot like the real thing
            const float k = 170.0f, damp = 2.0f * std::sqrt (k) * 0.62f;
            for (int sub = 0; sub < 4; ++sub)
            {
                const float h = dt / 4.0f;
                nd.vel += ((tgt - nd.pos) * k - nd.vel * damp) * h;
                nd.pos = jlimit (-0.02f, 1.1f, nd.pos + nd.vel * h);
            }
        }
        lamp = jmax (lamp * std::exp (-c.dt / 0.25f), (a.peakL > 0.89f || a.peakR > 0.89f) ? 1.0f : 0.0f);

        // fascia
        ColourGradient brushed (Colour (0xffe1e2e4), 0, r.getY(), Colour (0xffa9abaf), 0, r.getBottom(), false);
        brushed.addColour (0.5, Colour (0xffcfd1d4));
        g.setGradientFill (brushed);
        g.fillRect (c.full);
        Random lines (42);
        for (int i = 0; i < (int) c.full.getHeight(); i += 2)
        {
            g.setColour (Colours::black.withAlpha (0.015f + 0.025f * lines.nextFloat()));
            g.fillRect (c.full.getX(), c.full.getY() + (float) i, c.full.getWidth(), 1.0f);
        }

        const float pad = r.getWidth() * 0.05f;
        auto area = r.reduced (pad, r.getHeight() * 0.06f);
        auto label = area.removeFromBottom (area.getHeight() * 0.2f);
        const float meterW = jmin ((area.getWidth() - pad) * 0.5f, area.getHeight() * 1.65f);
        const float meterH = meterW / 1.65f;
        const float totalW = meterW * 2.0f + pad;
        const float x0 = area.getCentreX() - totalW * 0.5f, y0 = area.getCentreY() - meterH * 0.5f;

        for (int ch = 0; ch < 2; ++ch)
            drawMeter (g, { x0 + (float) ch * (meterW + pad), y0, meterW, meterH }, needles[ch].pos, ch == 0 ? "LEFT" : "RIGHT", c);

        // engraved labels
        auto& f = Fonts::get();
        auto textArea = label.withY (y0 + meterH + label.getHeight() * 0.25f);
        g.setFont (Fonts::font (f.bold, jmax (11.0f, textArea.getHeight() * 0.42f), 0.25f));
        g.setColour (Colours::white.withAlpha (0.55f));
        g.drawText ("VISIONQUEST", textArea.translated (0, 1), Justification::centredTop);
        g.setColour (Colour (0xff3a3c40));
        g.drawText ("VISIONQUEST", textArea, Justification::centredTop);
        g.setFont (Fonts::font (f.medium, jmax (9.0f, textArea.getHeight() * 0.2f), 0.3f));
        g.setColour (Colour (0xff55575c));
        g.drawText ("STEREO VU METER", textArea.withTrimmedTop (textArea.getHeight() * 0.5f), Justification::centredTop);

        // beat lamp + peak lamp on the fascia
        const float lr = jmax (4.0f, meterH * 0.035f);
        auto drawLamp = [&] (Point<float> p, Colour on, float amt, const char* txt)
        {
            g.setColour (Colour (0xff2a2b2e)); g.fillEllipse (p.x - lr * 1.35f, p.y - lr * 1.35f, lr * 2.7f, lr * 2.7f);
            glowDot (g, p, lr, Colour (0xff3a1010).interpolatedWith (on, amt), amt * 0.9f);
            g.setFont (Fonts::font (f.medium, jmax (8.0f, lr * 1.8f), 0.2f));
            g.setColour (Colour (0xff55575c));
            g.drawText (txt, Rectangle<float> (p.x - 40, p.y + lr * 1.6f, 80, lr * 3), Justification::centredTop);
        };
        drawLamp ({ x0 + lr * 2, y0 + meterH + label.getHeight() * 0.45f }, c.th.accent, a.beatPulse, "BEAT");
        drawLamp ({ x0 + totalW - lr * 2, y0 + meterH + label.getHeight() * 0.45f }, Colour (0xffff3020), lamp, "PEAK");
    }

private:
    void drawMeter (Graphics& g, Rectangle<float> b, float pos, const char* name, const Ctx& c)
    {
        auto& f = Fonts::get();
        // bezel
        g.setColour (Colour (0xff111214));
        g.fillRoundedRectangle (b.expanded (b.getHeight() * 0.05f), b.getHeight() * 0.06f);
        g.setColour (Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (b.expanded (b.getHeight() * 0.05f), b.getHeight() * 0.06f, 1.0f);

        // backlit face
        const auto lightC = c.th.at (0.4f, c.hue);
        const float bright = 0.8f + 0.2f * c.glow;
        ColourGradient face (lightC.brighter (0.35f).withMultipliedSaturation (0.85f).withMultipliedBrightness (bright),
                             b.getCentreX(), b.getY() + b.getHeight() * 0.35f,
                             lightC.darker (1.2f), b.getX(), b.getBottom(), true);
        face.addColour (0.55, lightC.withMultipliedBrightness (bright));
        g.setGradientFill (face);
        g.fillRoundedRectangle (b, b.getHeight() * 0.03f);

        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (b.toNearestInt());

        const Point<float> pivot (b.getCentreX(), b.getBottom() + b.getHeight() * 0.32f);
        const float rad = b.getHeight() * 1.02f;
        const float a0 = degreesToRadians (-47.0f), a1 = degreesToRadians (47.0f);
        auto angFor = [&] (float p) { return a0 + (a1 - a0) * p; };
        auto posFor = [] (float vu) { return std::pow (10.0f, vu / 20.0f) / std::pow (10.0f, 3.0f / 20.0f); };
        auto polar = [&] (float ang, float rr) { return Point<float> (pivot.x + std::sin (ang) * rr, pivot.y - std::cos (ang) * rr); };
        const Colour ink (0xff15161a), red (0xffc81e14);

        // scale arc: black up to 0 VU, red above
        Path black, redArc;
        black.addCentredArc (pivot.x, pivot.y, rad, rad, 0, angFor (posFor (-20)), angFor (posFor (0)), true);
        redArc.addCentredArc (pivot.x, pivot.y, rad, rad, 0, angFor (posFor (0)), angFor (1.0f), true);
        g.setColour (ink); g.strokePath (black, PathStrokeType (1.6f));
        g.setColour (red); g.strokePath (redArc, PathStrokeType (b.getHeight() * 0.04f));

        g.setFont (Fonts::font (f.bold, b.getHeight() * 0.085f));
        for (float vu : { -20.0f, -10.0f, -7.0f, -5.0f, -3.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f, 3.0f })
        {
            const float ang = angFor (posFor (vu));
            g.setColour (vu > 0 ? red : ink);
            g.drawLine ({ polar (ang, rad), polar (ang, rad + b.getHeight() * 0.07f) }, 1.6f);
            const auto lp = polar (ang, rad + b.getHeight() * 0.15f);
            const String s = vu > 0 ? "+" + String ((int) vu) : String ((int) vu);
            g.drawText (s, Rectangle<float> (lp.x - 30, lp.y - 10, 60, 20), Justification::centred);
        }
        g.setColour (ink);
        g.setFont (Fonts::font (f.display, b.getHeight() * 0.2f));
        g.drawText ("VU", Rectangle<float> (b.getX(), b.getY() + b.getHeight() * 0.42f, b.getWidth(), b.getHeight() * 0.26f), Justification::centred);
        g.setFont (Fonts::font (f.medium, b.getHeight() * 0.07f, 0.2f));
        g.drawText (name, Rectangle<float> (b.getX() + b.getWidth() * 0.06f, b.getBottom() - b.getHeight() * 0.16f, b.getWidth() * 0.4f, b.getHeight() * 0.12f), Justification::centredLeft);

        // needle with a soft shadow
        const float ang = angFor (pos);
        const auto tip = polar (ang, rad + b.getHeight() * 0.1f);
        const auto base = polar (ang, b.getHeight() * 0.38f);
        g.setColour (Colours::black.withAlpha (0.18f));
        g.drawLine ({ base.translated (3, 4), tip.translated (3, 4) }, 2.4f);
        g.setColour (Colour (0xff1b1b1b));
        g.drawLine ({ base, tip }, 2.0f);

        // glass sheen
        ColourGradient sheen (Colours::white.withAlpha (0.18f), b.getX(), b.getY(), Colours::white.withAlpha (0.0f), b.getX(), b.getCentreY(), false);
        g.setGradientFill (sheen);
        g.fillRect (b.withHeight (b.getHeight() * 0.5f));
    }

    struct Needle { float pos = 0, vel = 0; };
    Needle needles[2];
    float lamp = 0;
};

//==============================================================================
// 9. Starfield: warp speed rides the loudness, beats kick it into hyperdrive.
class Starfield : public Visual
{
public:
    bool fullBleed() const override { return true; }
    void reset() override { stars.clear(); }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const int want = 250 + (int) (c.detail * 950.0f);
        while ((int) stars.size() < want) { S s; respawn (s, c.rng, c.rng.nextFloat()); stars.push_back (s); }
        if ((int) stars.size() > want) stars.resize ((size_t) want);

        const auto ctr = r.getCentre();
        const float md = jmax (r.getWidth(), r.getHeight()) * 0.5f;
        rot += c.dt * c.speed * (0.03f + 0.35f * a.treble) * (c.mirror ? -1.0f : 1.0f);
        const float cr = std::cos (rot), sr = std::sin (rot);
        const float vz = c.dt * c.speed * (0.08f + 1.1f * a.level + 2.2f * c.pulse());

        auto project = [&] (float x, float y, float z)
        {
            const float px = x / z, py = y / z;
            return Point<float> (ctr.x + (px * cr - py * sr) * md, ctr.y + (px * sr + py * cr) * md);
        };

        for (auto& s : stars)
        {
            const float pz = s.z;
            s.z -= vz;
            if (s.z <= 0.02f) { respawn (s, c.rng, 1.0f); continue; }
            const auto p1 = project (s.x, s.y, s.z), p0 = project (s.x, s.y, pz);
            if (! r.expanded (20).contains (p1)) { respawn (s, c.rng, 1.0f); continue; }
            const float near = 1.0f - s.z;
            const auto col = c.col (s.t, std::pow (near, 0.7f));
            const float w = 0.5f + near * 2.6f;
            if (c.glow > 0.01f && near > 0.5f)
            {
                g.setColour (col.withMultipliedAlpha (0.2f * c.glow));
                g.drawLine ({ p0, p1 }, w * 3.5f);
            }
            g.setColour (col);
            if (p0.getDistanceFrom (p1) < 0.8f) g.fillEllipse (p1.x - w * 0.5f, p1.y - w * 0.5f, w, w);
            else g.drawLine ({ p0, p1 }, w);
        }

        if (c.pulse() > 0.02f)
        {
            g.setColour (c.th.accent.withRotatedHue (c.hue).withAlpha (0.07f * c.pulse()));
            g.fillRect (r);
        }
    }

private:
    struct S { float x, y, z, t; };
    static void respawn (S& s, Random& rng, float z)
    {
        s.x = rng.nextFloat() * 2.0f - 1.0f; s.y = rng.nextFloat() * 2.0f - 1.0f;
        s.z = jmax (0.05f, z); s.t = rng.nextFloat();
    }
    std::vector<S> stars;
    float rot = 0;
};

//==============================================================================
// 10. Kaleidoscope: shapes driven by the bands, mirrored around a wheel.
class Kaleido : public Visual
{
public:
    bool fullBleed() const override { return true; }
    void reset() override { rot = 0; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const auto ctr = r.getCentre();
        const float md = jmin (r.getWidth(), r.getHeight());
        const int segs = 6 + 2 * (int) (c.detail * 5.0f);
        const float wedge = twoPi / (float) segs;
        rot += c.dt * c.speed * 0.25f * (1.0f + a.mid);
        t += c.dt * c.speed;
        constexpr int K = 11;

        for (int layer = 0; layer < (c.mirror ? 2 : 1); ++layer)
        {
            const float dir = layer == 0 ? 1.0f : -1.0f;
            const float layerScale = layer == 0 ? 1.0f : 0.62f;
            Path links;
            for (int k = 0; k < K; ++k)
            {
                const float bt = (float) k / (K - 1);
                const float v = a.bandAt (bt);
                const float dist = md * (0.05f + 0.42f * bt) * layerScale * (1.0f + c.pulse() * 0.15f);
                const float ang = wedge * 0.5f * 0.8f * std::sin (t * (0.3f + 0.11f * (float) k) + (float) k);
                const Point<float> pc (std::cos (ang) * dist, std::sin (ang) * dist);
                const float size = md * (0.008f + 0.05f * v) * (0.45f + 0.9f * bt) * layerScale;

                Path shape;
                switch (k % 4)
                {
                    case 0: shape.addEllipse (pc.x - size, pc.y - size, size * 2, size * 2); break;
                    case 1: shape.addTriangle (pc.x, pc.y - size * 1.2f, pc.x - size, pc.y + size * 0.8f, pc.x + size, pc.y + size * 0.8f); break;
                    case 2: { Path d; d.addRectangle (-size * 0.8f, -size * 0.8f, size * 1.6f, size * 1.6f);
                              shape.addPath (d, AffineTransform::rotation (MathConstants<float>::pi * 0.25f + t).translated (pc)); break; }
                    default: { Path ring; ring.addEllipse (pc.x - size, pc.y - size, size * 2, size * 2);
                               PathStrokeType (size * 0.35f).createStrokedPath (shape, ring); break; }
                }
                if (k == 0) links.startNewSubPath (pc); else links.lineTo (pc);

                const auto col = c.col (bt, 0.55f + 0.45f * v);
                for (int s = 0; s < segs; ++s)
                {
                    auto tf = (s % 2 == 1 ? AffineTransform::scale (1.0f, -1.0f) : AffineTransform())
                                  .rotated (dir * rot + (float) s * wedge).translated (ctr);
                    if (c.glow > 0.01f)
                    {
                        g.setColour (col.withMultipliedAlpha (0.18f * c.glow));
                        g.strokePath (shape, PathStrokeType (size * 0.8f * c.glow + 1.0f), tf);
                    }
                    g.setColour (col);
                    g.fillPath (shape, tf);
                }
            }
            for (int s = 0; s < segs; ++s)
            {
                auto tf = (s % 2 == 1 ? AffineTransform::scale (1.0f, -1.0f) : AffineTransform())
                              .rotated (dir * rot + (float) s * wedge).translated (ctr);
                g.setColour (c.th.text.withAlpha (0.12f + 0.2f * a.level));
                g.strokePath (links, PathStrokeType (1.0f), tf);
            }
        }
        glowDot (g, ctr, md * (0.015f + 0.04f * c.pulse() + 0.02f * a.bass), c.col (0.95f), c.glow);
    }

private:
    float rot = 0, t = 0;
};

//==============================================================================
// 11. Plasma: classic demoscene plasma, bent by bass / mid / treble.
class Plasma : public Visual
{
public:
    bool fullBleed() const override { return true; }
    bool usesTrails() const override { return false; }
    void reset() override { img = {}; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const int W = 128 + (int) (c.detail * 96.0f), H = jmax (40, (int) ((float) W * r.getHeight() / jmax (1.0f, r.getWidth())));
        if (! img.isValid() || img.getWidth() != W || img.getHeight() != H)
            img = Image (Image::RGB, W, H, false, SoftwareImageType());

        phase += c.dt * c.speed * (0.5f + 1.6f * a.level + 2.5f * c.pulse());
        const float f1 = (0.07f + a.bass * 0.09f) * 160.0f / (float) W;
        const float f2 = (0.06f + a.mid * 0.09f) * 160.0f / (float) W;
        const float f3 = (0.045f + a.treble * 0.08f) * 160.0f / (float) W;
        const float bright = jlimit (0.35f, 1.0f, 0.45f + 0.6f * a.level + 0.3f * c.pulse());

        Colour lut[256];
        for (int i = 0; i < 256; ++i)
        {
            const float v = (float) i / 255.0f;
            const float tt = 1.0f - std::abs (2.0f * frac (v * 1.5f + phase * 0.04f) - 1.0f);
            lut[i] = c.col (tt).withMultipliedBrightness (bright);
        }

        const float cx = (float) W * (0.5f + 0.3f * std::sin (phase * 0.31f));
        const float cy = (float) H * (0.5f + 0.3f * std::cos (phase * 0.23f));
        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        for (int y = 0; y < H; ++y)
        {
            for (int x = 0; x < W; ++x)
            {
                const float xx = c.mirror ? std::abs ((float) x - (float) W * 0.5f) : (float) x;
                const float dx = xx - cx, dy = (float) y - cy;
                const float v = std::sin (xx * f1 + phase)
                              + std::sin ((float) y * f2 - phase * 1.3f)
                              + std::sin ((xx + (float) y) * f3 + phase * 0.7f)
                              + std::sin (std::sqrt (dx * dx + dy * dy) * f1 * 1.4f - phase * 2.0f);
                const int idx = jlimit (0, 255, (int) ((v + 4.0f) * 31.875f));
                bd.setPixelColour (x, y, lut[idx]);
            }
        }

        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (img, r, RectanglePlacement::stretchToFit);

        const auto bg = c.th.bg.withRotatedHue (c.hue);
        ColourGradient vig (bg.withAlpha (0.0f), r.getCentre(), bg.withAlpha (0.75f), r.getTopLeft(), true);
        g.setGradientFill (vig);
        g.fillRect (r);
    }

private:
    Image img;
    float phase = 0;
};

//==============================================================================
// 12. LED matrix: an 80s car-stereo spectrum display.
class LedMatrix : public Visual
{
public:
    bool usesTrails() const override { return false; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        drawBackground (g, r, c.th, c.hue, 1.0f);
        const int cols = 16 + (int) (c.detail * 24.0f);
        constexpr int rows = 18;
        const auto area = r.reduced (r.getWidth() * 0.06f, r.getHeight() * 0.12f);
        const float cell = jmin (area.getWidth() / (float) cols, area.getHeight() / (float) rows);
        const float gx = area.getCentreX() - cell * (float) cols * 0.5f;
        const float gy = area.getCentreY() - cell * (float) rows * 0.5f;
        const float dot = cell * 0.78f, rad = jmax (1.0f, cell * 0.14f);

        // panel
        auto panel = Rectangle<float> (gx, gy, cell * (float) cols, cell * (float) rows).expanded (cell * 0.8f);
        g.setColour (Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (panel, cell * 0.5f);
        g.setColour (c.th.accent.withRotatedHue (c.hue).withAlpha (0.15f + 0.5f * c.pulse()));
        g.drawRoundedRectangle (panel, cell * 0.5f, 1.5f + 3.0f * c.pulse());

        for (int i = 0; i < cols; ++i)
        {
            const float u = ((float) i + 0.5f) / (float) cols;
            const float t = c.mirror ? std::abs (u * 2.0f - 1.0f) : u;
            const int lit = roundToInt (a.bandAt (t) * (1.0f + 0.15f * c.pulse()) * rows);
            const int pk = jmin (rows - 1, roundToInt (a.peakAt (t) * rows));
            for (int j = 0; j < rows; ++j)
            {
                const float x = gx + (float) i * cell + (cell - dot) * 0.5f;
                const float y = gy + (float) (rows - 1 - j) * cell + (cell - dot) * 0.5f;
                const auto col = c.col ((float) j / (rows - 1));
                const bool on = j < lit, isPeak = (j == pk && pk > 0 && ! on);
                if (on || isPeak)
                {
                    const auto cc = isPeak ? c.th.text : col;
                    if (c.glow > 0.01f)
                    {
                        g.setColour (cc.withAlpha (0.18f * c.glow));
                        g.fillRoundedRectangle (x - dot * 0.3f, y - dot * 0.3f, dot * 1.6f, dot * 1.6f, rad * 2);
                    }
                    g.setColour (cc);
                }
                else
                {
                    g.setColour (col.withAlpha (0.07f));
                }
                g.fillRoundedRectangle (x, y, dot, dot, rad);
            }
        }
    }
};

//==============================================================================
std::unique_ptr<Visual> createVisual2 (int index);   // the v1.1 modes, in Visuals2.cpp

std::unique_ptr<Visual> createVisual (int index)
{
    if (index >= 12) return createVisual2 (index);
    switch (index)
    {
        case 0:  return std::make_unique<Bars>();
        case 1:  return std::make_unique<Scope>();
        case 2:  return std::make_unique<Radial>();
        case 3:  return std::make_unique<Lissajous>();
        case 4:  return std::make_unique<Waterfall>();
        case 5:  return std::make_unique<Particles>();
        case 6:  return std::make_unique<Tunnel>();
        case 7:  return std::make_unique<VUMeters>();
        case 8:  return std::make_unique<Starfield>();
        case 9:  return std::make_unique<Kaleido>();
        case 10: return std::make_unique<Plasma>();
        default: return std::make_unique<LedMatrix>();
    }
}

} // namespace vq
