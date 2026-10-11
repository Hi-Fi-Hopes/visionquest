// VisionQuest v1.1 modes (indices 12..27).
#include "ui/Visuals.h"
#include "ui/Fonts.h"
#include "ui/VisualHelpers.h"

using namespace juce;

namespace vq
{

using namespace detail;

//==============================================================================
// 13. Pulse Rings: concentric arcs, one per band, spinning both ways.
class PulseRings : public Visual
{
public:
    void reset() override { rot = 0; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const auto ctr = r.getCentre();
        const float md = jmin (r.getWidth(), r.getHeight());
        const int n = 10 + (int) (c.detail * 26.0f);
        rot += c.dt * c.speed * (0.4f + a.mid);

        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) (n - 1);
            const float v = a.bandAt (t);
            const float rad = md * (0.05f + 0.43f * t) * (1.0f + c.pulse() * 0.06f * (1.0f - t));
            const float dir = (i % 2 == 0) ? 1.0f : -1.0f;
            const float start = rot * dir * (0.6f + t * 1.4f) + (float) i * 0.9f;
            const float width = 1.0f + v * md * 0.022f;
            const auto col = c.col (t, 0.3f + 0.7f * v);

            // faint full ring underneath
            Path ring; ring.addEllipse (ctr.x - rad, ctr.y - rad, rad * 2, rad * 2);
            g.setColour (col.withMultipliedAlpha (0.12f));
            g.strokePath (ring, PathStrokeType (1.0f));

            if (v < 0.02f) continue;
            const int arcs = c.mirror ? 2 : 1;
            const float sweep = v * twoPi / (float) arcs * 0.95f;
            for (int k = 0; k < arcs; ++k)
            {
                Path arc;
                const float s0 = start + (float) k * pi;
                arc.addCentredArc (ctr.x, ctr.y, rad, rad, 0.0f, s0, s0 + sweep, true);
                glowStroke (g, arc, col, width, c.glow);
            }
        }
        glowDot (g, ctr, md * (0.012f + 0.03f * c.pulse() + 0.02f * a.bass), c.col (0.0f), c.glow);
    }

private:
    float rot = 0;
};

//==============================================================================
// 14. Retro Horizon: synthwave sun, spectrum mountains, scrolling neon grid.
class RetroHorizon : public Visual
{
public:
    bool usesTrails() const override { return false; }
    bool fullBleed() const override { return true; }

    RetroHorizon()
    {
        Random rr (99);
        for (auto& s : stars) s = { rr.nextFloat(), rr.nextFloat() * 0.9f, rr.nextFloat() * twoPi, 0.5f + rr.nextFloat() };
    }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const float w = r.getWidth(), h = r.getHeight();
        const float horizon = r.getY() + h * 0.6f;
        scroll = frac (scroll + c.dt * c.speed * (0.5f + 1.6f * a.level + 2.0f * c.pulse()));
        twinkle += c.dt;

        // sky
        auto sky = r.withBottom (horizon);
        ColourGradient sg (c.th.bg.withRotatedHue (c.hue), 0, sky.getY(), c.col (0.2f).darker (0.6f), 0, horizon, false);
        sg.addColour (0.75, c.th.bg2.withRotatedHue (c.hue).brighter (0.1f));
        g.setGradientFill (sg);
        g.fillRect (sky);
        for (auto& s : stars)
        {
            const float tw = 0.4f + 0.6f * (0.5f + 0.5f * std::sin (twinkle * 2.0f * s.speed + s.phase));
            g.setColour (c.th.text.withAlpha (0.5f * tw));
            g.fillEllipse (r.getX() + s.x * w, sky.getY() + s.y * sky.getHeight() * 0.7f, 1.6f, 1.6f);
        }

        // sun with cut-out stripes
        const float sr = h * (0.2f + 0.03f * a.bass + 0.03f * c.pulse());
        const Point<float> sc (r.getCentreX(), horizon - sr * 0.45f);
        {
            Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (sky.toNearestInt());
            if (c.glow > 0.01f)
            {
                ColourGradient halo (c.col (0.75f, 0.35f * c.glow), sc, c.col (0.75f, 0.0f), sc.translated (sr * 2.0f, 0), true);
                g.setGradientFill (halo);
                g.fillEllipse (sc.x - sr * 2, sc.y - sr * 2, sr * 4, sr * 4);
            }
            Path sun; sun.addEllipse (sc.x - sr, sc.y - sr, sr * 2, sr * 2);
            for (int i = 0; i < 6; ++i)
            {
                const float y = sc.y - sr * 0.12f + sr * 0.105f * (float) i;
                const float th = sr * (0.012f + 0.011f * (float) i);
                Path cut; cut.addRectangle (sc.x - sr, y, sr * 2, th);
                sun.setUsingNonZeroWinding (false);
                sun.addPath (cut);
            }
            ColourGradient sunG (c.col (1.0f), 0, sc.y - sr, c.col (0.55f), 0, sc.y + sr, false);
            g.setGradientFill (sunG);
            g.fillPath (sun);
        }

        // mountains from the spectrum (two layers)
        for (int layer = 0; layer < 2; ++layer)
        {
            Path m;
            m.startNewSubPath (r.getX(), horizon);
            const int pts = 64;
            const float amp = h * (layer == 0 ? 0.16f : 0.1f);
            for (int i = 0; i <= pts; ++i)
            {
                const float u = (float) i / (float) pts;
                const float t = std::abs (u * 2.0f - 1.0f);
                const float v = a.bandAt (layer == 0 ? t : jmin (1.0f, t * 0.7f + 0.3f));
                const float jag = 0.5f + 0.5f * std::sin ((float) i * (layer == 0 ? 1.7f : 2.9f));
                m.lineTo (r.getX() + u * w, horizon - amp * (0.15f + 0.85f * v) * (0.6f + 0.4f * jag) * (0.3f + t));
            }
            m.lineTo (r.getRight(), horizon);
            m.closeSubPath();
            g.setColour (c.th.bg.withRotatedHue (c.hue).brighter (layer == 0 ? 0.05f : 0.12f));
            g.fillPath (m);
            glowStroke (g, m, c.col (layer == 0 ? 0.35f : 0.15f, layer == 0 ? 0.9f : 0.6f), 1.4f, c.glow * 0.7f);
        }

        // ground + grid
        auto ground = r.withTop (horizon);
        ColourGradient gg (c.th.bg.withRotatedHue (c.hue), 0, horizon, c.th.bg2.withRotatedHue (c.hue).darker (0.3f), 0, r.getBottom(), false);
        g.setGradientFill (gg);
        g.fillRect (ground);

        const auto gridCol = c.col (0.7f);
        const float gh = ground.getHeight();
        const int rows = 14;
        for (int k = 0; k < rows; ++k)
        {
            // lines bunch up toward the horizon and slide toward you as 'scroll' advances
            const float e = ((float) k + scroll) / (float) rows;
            const float y = horizon + gh * std::pow (e, 2.2f);
            if (y > r.getBottom()) continue;
            const float near = (y - horizon) / gh;
            glowLine (g, { r.getX(), y, r.getRight(), y }, gridCol.withMultipliedAlpha (0.15f + 0.85f * near).withMultipliedBrightness (1.0f + c.pulse() * 0.5f),
                      0.8f + 1.6f * near, c.glow * near);
        }
        const int cols = 12 + (int) (c.detail * 16.0f);
        for (int i = -cols; i <= cols; ++i)
        {
            const float xb = r.getCentreX() + (float) i / (float) cols * w * 1.6f;
            glowLine (g, { r.getCentreX() + (float) i / (float) cols * w * 0.04f, horizon, xb, r.getBottom() }, gridCol.withMultipliedAlpha (0.6f), 1.0f, c.glow * 0.6f);
        }
        // horizon glow line
        glowLine (g, { r.getX(), horizon, r.getRight(), horizon }, c.col (0.9f), 1.5f, c.glow);
    }

private:
    struct Star { float x, y, phase, speed; };
    std::array<Star, 90> stars;
    float scroll = 0, twinkle = 0;
};

//==============================================================================
// 15. Ridgelines: stacked spectrum lines with hidden-line fill (album-cover style).
class Ridgelines : public Visual
{
public:
    bool usesTrails() const override { return false; }
    void reset() override { history.clear(); acc = 0; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const int lines = 22 + (int) (c.detail * 30.0f);
        acc += c.dt * c.speed * 16.0f;
        if (history.empty() || acc >= 1.0f)
        {
            acc = jmin (acc, 1.0f) - (history.empty() ? 0.0f : 1.0f);
            std::array<float, pts> row {};
            for (int i = 0; i < pts; ++i)
            {
                const float u = (float) i / (float) (pts - 1);
                const float t = c.mirror ? std::abs (u * 2.0f - 1.0f) : u;
                row[(size_t) i] = std::pow (jlimit (0.0f, 1.0f, (a.bandAt (t) - 0.2f) / 0.8f), 1.3f);
            }
            history.push_front (row);
        }
        while ((int) history.size() > lines) history.pop_back();

        const auto area = r.reduced (r.getWidth() * 0.18f, r.getHeight() * 0.1f);
        const float spacing = area.getHeight() / (float) (lines + 3);
        const float amp = spacing * 7.0f * (1.0f + c.pulse() * 0.25f);
        const auto bg = c.th.bg.withRotatedHue (c.hue);

        // oldest at the top, newest at the bottom (drawn last, in front)
        for (int li = 0; li < (int) history.size(); ++li)
        {
            const int idx = (int) history.size() - 1 - li;
            const auto& row = history[(size_t) idx];
            const float base = area.getY() + spacing * (float) (lines - idx + 2);
            Path line, fill;
            for (int i = 0; i < pts; ++i)
            {
                const float u = (float) i / (float) (pts - 1);
                const float env = std::pow (std::sin (pi * u), 2.0f);
                const float wob = 0.06f * std::sin ((float) i * 1.3f + (float) idx * 0.7f);
                const float x = area.getX() + u * area.getWidth();
                const float y = base - amp * (row[(size_t) i] * env + wob * env * row[(size_t) i]);
                if (i == 0) { line.startNewSubPath (x, y); fill.startNewSubPath (x, base + spacing); fill.lineTo (x, y); }
                else { line.lineTo (x, y); fill.lineTo (x, y); }
            }
            fill.lineTo (area.getRight(), base + spacing);
            fill.closeSubPath();
            g.setColour (bg);
            g.fillPath (fill);
            const float t = 1.0f - (float) idx / (float) jmax (1, lines - 1);
            glowStroke (g, line, c.col (t, 0.5f + 0.5f * t), 1.3f, c.glow * 0.6f);
        }
    }

private:
    static constexpr int pts = 72;
    std::deque<std::array<float, pts>> history;
    float acc = 0;
};

//==============================================================================
// 16. Orbit Scope: the waveform wrapped around a circle.
class OrbitScope : public Visual
{
public:
    void reset() override { rot = 0; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const auto ctr = r.getCentre();
        const float md = jmin (r.getWidth(), r.getHeight());
        rot += c.dt * c.speed * 0.45f;
        const float r0 = md * 0.24f * (1.0f + c.pulse() * 0.12f);
        const float amp = md * 0.2f;
        constexpr int N = Analyzer::scopeSize;
        constexpr int segs = 12;
        const float w = 1.3f + 2.0f * a.level;

        g.setColour (c.th.text.withAlpha (0.08f));
        g.drawEllipse (ctr.x - r0, ctr.y - r0, r0 * 2, r0 * 2, 1.0f);

        const int copies = c.mirror ? 3 : 1;
        for (int cp = 0; cp < copies; ++cp)
            for (int ch = 0; ch < 2; ++ch)
            {
                const float* data = ch == 0 ? a.scopeL : a.scopeR;
                const float off = rot * (ch == 0 ? 1.0f : -0.7f) + (float) cp * twoPi / (float) copies;
                for (int sgi = 0; sgi < segs; ++sgi)
                {
                    Path p;
                    const int i0 = sgi * N / segs, i1 = jmin (N - 1, (sgi + 1) * N / segs);
                    for (int i = i0; i <= i1; ++i)
                    {
                        const float u = (float) i / (float) (N - 1);
                        const float taper = jmin (1.0f, jmin (u, 1.0f - u) * 20.0f);   // closes the loop cleanly
                        const float rr = r0 * (ch == 0 ? 1.0f : 0.82f) + data[i] * amp * taper;
                        const float ang = off + u * twoPi;
                        const Point<float> p0 (ctr.x + std::cos (ang) * rr, ctr.y + std::sin (ang) * rr);
                        if (i == i0) p.startNewSubPath (p0); else p.lineTo (p0);
                    }
                    const float t = (float) sgi / (float) (segs - 1);
                    glowStroke (g, p, c.col (ch == 0 ? t : 1.0f - t, 0.85f), w, c.glow);
                }
            }
        glowDot (g, ctr, md * (0.01f + 0.04f * a.bass * (0.4f + c.react)), c.col (0.5f), c.glow);
    }

private:
    float rot = 0;
};

//==============================================================================
// 17. Lava Lamp: metaballs, sized by the bands.
class LavaLamp : public Visual
{
public:
    bool usesTrails() const override { return false; }
    bool fullBleed() const override { return true; }
    void reset() override { img = {}; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const int W = 110 + (int) (c.detail * 70.0f);
        const int H = jmax (40, (int) ((float) W * r.getHeight() / jmax (1.0f, r.getWidth())));
        if (! img.isValid() || img.getWidth() != W || img.getHeight() != H)
            img = Image (Image::RGB, W, H, false, SoftwareImageType());

        t += c.dt * c.speed * (0.25f + 0.6f * a.level + 0.8f * c.pulse());
        constexpr int K = 9;
        float bx[K], by[K], br2[K];
        const float unit = (float) jmin (W, H);
        for (int k = 0; k < K; ++k)
        {
            const float fk = (float) k;
            const float v = a.bandAt ((fk + 0.5f) / K);
            bx[k] = (float) W * (0.5f + 0.38f * std::sin (t * (0.31f + 0.07f * fk) + fk * 1.7f));
            by[k] = (float) H * (0.5f + 0.4f * std::sin (t * (0.23f + 0.05f * fk) + fk * 2.3f + 1.0f));
            const float rad = unit * (0.07f + 0.13f * v + 0.03f * c.pulse());
            br2[k] = rad * rad;
        }

        Colour lut[256];
        const auto bg = c.th.bg.withRotatedHue (c.hue);
        for (int i = 0; i < 256; ++i)
        {
            const float f = (float) i / 255.0f * 3.0f;     // field strength 0..3
            if (f < 0.6f)      lut[i] = bg.interpolatedWith (c.col (0.15f), f / 0.6f * 0.35f);
            else if (f < 1.0f) lut[i] = bg.interpolatedWith (c.col (0.3f), 0.35f + (f - 0.6f) / 0.4f * 0.65f * (0.4f + 0.6f * c.glow));
            else               lut[i] = c.col (jmin (1.0f, 0.4f + (f - 1.0f) * 0.35f));
        }

        Image::BitmapData bd (img, Image::BitmapData::writeOnly);
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
            {
                const float xx = c.mirror ? (float) W * 0.5f + std::abs ((float) x - (float) W * 0.5f) : (float) x;
                float f = 0.0f;
                for (int k = 0; k < K; ++k)
                {
                    const float dx = xx - bx[k], dy = (float) y - by[k];
                    f += br2[k] / (dx * dx + dy * dy + 1.0f);
                }
                bd.setPixelColour (x, y, lut[jlimit (0, 255, (int) (f / 3.0f * 255.0f))]);
            }

        g.setImageResamplingQuality (Graphics::highResamplingQuality);
        g.drawImage (img, r, RectanglePlacement::stretchToFit);
    }

private:
    Image img;
    float t = 0;
};

//==============================================================================
// 18. Fireworks: rockets on the beat, bursting into sparks with gravity.
class Fireworks : public Visual
{
public:
    bool fullBleed() const override { return true; }
    void reset() override { rockets.clear(); sparks.clear(); acc = 0; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const float md = jmin (r.getWidth(), r.getHeight());
        const float grav = md * 1.6f;          // rockets: a snappy launch, ~0.8 s to burst
        const float sparkGrav = md * 0.22f;    // sparks: a slow drift down
        const float dt = c.dt * c.speed;

        acc += c.dt * (0.2f + 1.2f * a.level * a.level);
        int launches = (int) acc; acc -= (float) launches;
        if (a.beat && c.react > 0.01f) launches += 1 + (c.pulse() > 0.7f ? 1 : 0);
        for (int k = 0; k < launches && rockets.size() < 12; ++k)
        {
            Rocket rk;
            rk.x = r.getX() + r.getWidth() * (0.15f + 0.7f * c.rng.nextFloat());
            rk.y = r.getBottom();
            const float apexH = r.getHeight() * (0.45f + 0.35f * c.rng.nextFloat());
            rk.vy = -std::sqrt (2.0f * grav * apexH);
            rk.vx = (c.rng.nextFloat() - 0.5f) * md * 0.15f;
            rk.t = c.rng.nextFloat();
            rockets.push_back (rk);
            if (c.mirror) { rk.x = r.getRight() - (rk.x - r.getX()); rk.vx = -rk.vx; rockets.push_back (rk); }
        }

        for (auto& rk : rockets)
        {
            const Point<float> prev (rk.x, rk.y);
            rk.vy += grav * dt; rk.x += rk.vx * dt; rk.y += rk.vy * dt;
            glowLine (g, { prev, { rk.x, rk.y } }, c.col (rk.t, 0.9f), 2.0f, c.glow);
            if (rk.vy >= 0.0f) burst (rk, md, c);
        }
        rockets.erase (std::remove_if (rockets.begin(), rockets.end(), [] (const Rocket& rk) { return rk.vy >= 0.0f; }), rockets.end());

        const float drag = std::exp (-dt * 1.4f);
        for (auto& s : sparks)
        {
            const Point<float> prev (s.x, s.y);
            s.vx *= drag; s.vy = s.vy * drag + sparkGrav * dt;
            s.x += s.vx * dt; s.y += s.vy * dt;
            s.life -= c.dt;
            const float f = jlimit (0.0f, 1.0f, s.life / s.maxLife);
            const float tw = (s.twinkle && c.rng.nextFloat() < 0.3f) ? 0.3f : 1.0f;
            glowLine (g, { prev, { s.x, s.y } }, c.col (s.t, f * tw), 1.0f + 1.4f * f, c.glow * f);
        }
        sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const Spark& s) { return s.life <= 0.0f; }), sparks.end());
    }

private:
    struct Rocket { float x, y, vx, vy, t; };
    struct Spark { float x, y, vx, vy, life, maxLife, t; bool twinkle; };

    void burst (const Rocket& rk, float md, const Ctx& c)
    {
        const int n = 50 + (int) (c.detail * 170.0f);
        const float sp = md * (0.18f + 0.25f * c.rng.nextFloat()) * (1.0f + c.pulse() * 0.4f);
        const bool ring = c.rng.nextFloat() < 0.3f;
        const float t2 = frac (rk.t + 0.35f);
        for (int i = 0; i < n && sparks.size() < 5000; ++i)
        {
            const float ang = c.rng.nextFloat() * twoPi;
            const float m = ring ? 1.0f : std::sqrt (c.rng.nextFloat());
            Spark s { rk.x, rk.y, std::cos (ang) * sp * m, std::sin (ang) * sp * m, 0, 0,
                      (i % 3 == 0) ? t2 : rk.t, c.rng.nextFloat() < 0.4f };
            s.maxLife = s.life = 1.2f + 1.3f * c.rng.nextFloat();
            sparks.push_back (s);
        }
    }

    std::vector<Rocket> rockets;
    std::vector<Spark> sparks;
    float acc = 0;
};

//==============================================================================
// 19. Digital Rain: falling glyph streams, faster and brighter with the music.
class DigitalRain : public Visual
{
public:
    bool fullBleed() const override { return true; }
    void reset() override { cols.clear(); }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const int n = 24 + (int) (c.detail * 48.0f);
        const float cw = r.getWidth() / (float) n;
        const int rows = jmax (8, (int) (r.getHeight() / cw));
        if ((int) cols.size() != n)
        {
            cols.assign ((size_t) n, {});
            for (auto& col : cols) respawn (col, rows, c.rng, true);
        }

        static const String glyphs ("0123456789ABCDEF#$%&*+=<>?@:;|/\\[]{}~^");
        g.setFont (Fonts::font (Fonts::get().mono, cw * 1.05f));

        for (int i = 0; i < n; ++i)
        {
            auto& col = cols[(size_t) i];
            const float u = ((float) i + 0.5f) / (float) n;
            const float t = c.mirror ? std::abs (u * 2.0f - 1.0f) : u;
            const float v = a.bandAt (t);
            col.head += c.dt * c.speed * col.speed * (0.35f + 1.6f * a.level + 1.2f * c.pulse());
            if (c.rng.nextFloat() < 0.08f) col.glyph[(size_t) c.rng.nextInt (32)] = (char) glyphs[c.rng.nextInt (glyphs.length())];
            if (col.head - (float) col.length > (float) rows) respawn (col, rows, c.rng, false);

            const int head = (int) col.head;
            for (int k = 0; k < tail && k < col.length; ++k)
            {
                const int row = head - k;
                if (row < 0 || row >= rows) continue;
                const float f = 1.0f - (float) k / (float) tail;
                Colour cc = k == 0 ? c.th.text.interpolatedWith (Colours::white, 0.5f) : c.col (0.3f + 0.7f * v * f, f * (0.35f + 0.65f * v));
                if (k == 0 && a.beat) cc = Colours::white;
                const char ch = col.glyph[(size_t) ((row + i) & 31)];
                const float x = r.getX() + (float) i * cw, y = r.getY() + (float) row * cw;
                if (k == 0 && c.glow > 0.01f)
                {
                    // soft bloom: the head glyph drawn again, offset slightly in each direction
                    g.setColour (c.col (0.7f, 0.22f * c.glow));
                    const auto s1 = String::charToString ((juce_wchar) (uint8) ch);
                    for (auto off : { Point<int> (-1, 0), Point<int> (1, 0), Point<int> (0, -1), Point<int> (0, 1) })
                        g.drawSingleLineText (s1, (int) (x + cw * 0.18f) + off.x, (int) (y + cw * 0.85f) + off.y);
                }
                g.setColour (cc);
                g.drawSingleLineText (String::charToString ((juce_wchar) (uint8) ch), (int) (x + cw * 0.18f), (int) (y + cw * 0.85f));
            }
        }
    }

private:
    static constexpr int tail = 10;
    struct Column { float head = 0, speed = 10; int length = 12; std::array<char, 32> glyph {}; };

    static void respawn (Column& col, int rows, Random& rng, bool anywhere)
    {
        static const char* g = "0123456789ABCDEF#$%&*+=<>?@";
        col.head = anywhere ? rng.nextFloat() * (float) rows : -(float) rng.nextInt (rows / 2 + 1);
        col.speed = 7.0f + 16.0f * rng.nextFloat();
        col.length = 6 + rng.nextInt (22);
        for (auto& ch : col.glyph) ch = g[rng.nextInt (27)];
    }

    std::vector<Column> cols;
};

//==============================================================================
// 20. Ripple Grid: a dot surface rippling outward from every beat.
class RippleGrid : public Visual
{
public:
    void reset() override { ripples.clear(); time = 0; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        time += c.dt * c.speed;
        if (a.beat && c.react > 0.01f)
        {
            if (ripples.size() >= 8) ripples.erase (ripples.begin());
            const bool centre = c.mirror || c.rng.nextFloat() < 0.4f;
            ripples.push_back ({ centre ? 0.5f : 0.15f + 0.7f * c.rng.nextFloat(),
                                 centre ? 0.5f : 0.15f + 0.7f * c.rng.nextFloat(), time, c.pulse() + 0.3f });
        }

        const int cols = 20 + (int) (c.detail * 30.0f);
        const float cell = r.getWidth() / (float) cols;
        const int rows = jmax (6, (int) (r.getHeight() / cell));
        const float oy = r.getY() + (r.getHeight() - cell * (float) (rows - 1)) * 0.5f;
        const float aspect = r.getHeight() / jmax (1.0f, r.getWidth());

        for (int j = 0; j < rows; ++j)
            for (int i = 0; i < cols; ++i)
            {
                const float u = ((float) i + 0.5f) / (float) cols, v = (float) j / (float) jmax (1, rows - 1);
                const float du = u - 0.5f, dv = (v - 0.5f) * aspect;
                const float dist = std::sqrt (du * du + dv * dv) * 1.6f;
                float z = a.bandAt (jmin (1.0f, dist)) * 0.8f * std::sin (dist * 14.0f - time * 3.0f) * 0.5f
                        + a.bandAt (jmin (1.0f, dist)) * 0.5f;
                for (auto& rp : ripples)
                {
                    const float age = time - rp.t0;
                    const float ddu = u - rp.x, ddv = (v - rp.y) * aspect;
                    const float d = std::sqrt (ddu * ddu + ddv * ddv);
                    const float front = age * 0.55f;
                    if (d < front)
                        z += rp.amp * std::sin ((d - front) * 30.0f) * std::exp (-age * 1.6f) * std::exp (-(front - d) * 4.0f);
                }
                const float x = r.getX() + u * r.getWidth();
                const float y = oy + (float) j * cell - z * cell * 1.8f;
                const float sz = cell * (0.12f + 0.28f * jlimit (0.0f, 1.0f, 0.4f + z));
                const float tt = jlimit (0.0f, 1.0f, 0.3f + z * 0.8f);
                if (c.glow > 0.01f && z > 0.3f)
                {
                    g.setColour (c.col (tt, 0.2f * c.glow));
                    g.fillEllipse (x - sz * 2, y - sz * 2, sz * 4, sz * 4);
                }
                g.setColour (c.col (tt, 0.35f + 0.65f * jlimit (0.0f, 1.0f, 0.5f + z)));
                g.fillEllipse (x - sz, y - sz, sz * 2, sz * 2);
            }
        ripples.erase (std::remove_if (ripples.begin(), ripples.end(), [this] (const Ripple& rp) { return time - rp.t0 > 4.0f; }), ripples.end());
    }

private:
    struct Ripple { float x, y, t0, amp; };
    std::vector<Ripple> ripples;
    float time = 0;
};

//==============================================================================
// 21. Flow Field: thousands of threads drifting through a moving current.
class FlowField : public Visual
{
public:
    bool fullBleed() const override { return true; }
    void reset() override { ps.clear(); }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const int want = 300 + (int) (c.detail * 1500.0f);
        while ((int) ps.size() < want) { P p; spawn (p, r, c.rng); ps.push_back (p); }
        if ((int) ps.size() > want) ps.resize ((size_t) want);

        phase += c.dt * c.speed * (0.25f + 0.8f * a.mid + 1.5f * c.pulse());
        const float md = jmin (r.getWidth(), r.getHeight());
        // A swirl (curl) field: it has eddies but no drains, so the threads never
        // bunch up into a single stream.
        const float fa = 6.0f / md * (1.0f + 0.6f * a.bass), fb = fa * 1.3f, fc = fa * 0.6f;
        const float sp = md * (0.06f + 0.45f * a.level + 0.4f * c.pulse()) * c.speed;
        const float cx = r.getCentreX();

        for (auto& p : ps)
        {
            const float x = c.mirror ? cx + std::abs (p.x - cx) : p.x;
            const float y = p.y;
            const float t1 = phase, t2 = -phase * 0.7f, t3 = phase * 0.31f;
            const float dpx = fa * std::cos (x * fa + t1) * std::cos (y * fb + t2) + 0.5f * fc * std::cos ((x + y) * fc + t3);
            const float dpy = -fb * std::sin (x * fa + t1) * std::sin (y * fb + t2) + 0.5f * fc * std::cos ((x + y) * fc + t3);
            float vx = dpy, vy = -dpx;
            const float len = std::sqrt (vx * vx + vy * vy) + 1.0e-6f;
            vx /= len; vy /= len;
            if (c.mirror && p.x < cx) vx = -vx;
            const Point<float> prev (p.x, p.y);
            p.x += vx * sp * c.dt;
            p.y += vy * sp * c.dt;
            p.life -= c.dt;
            if (p.life <= 0 || ! r.contains (p.x, p.y)) { spawn (p, r, c.rng); continue; }
            const float v = a.bandAt (p.t);
            glowLine (g, { prev, { p.x, p.y } }, c.col (p.t, 0.35f + 0.65f * v), 1.0f + v, c.glow * 0.5f);
        }
    }

private:
    struct P { float x = 0, y = 0, t = 0, life = 0; };
    static void spawn (P& p, Rectangle<float> r, Random& rng)
    {
        p.x = r.getX() + rng.nextFloat() * r.getWidth();
        p.y = r.getY() + rng.nextFloat() * r.getHeight();
        p.t = rng.nextFloat();
        p.life = 2.0f + 5.0f * rng.nextFloat();
    }
    std::vector<P> ps;
    float phase = 0;
};

//==============================================================================
// 22. Hypno Spiral: spiral arms whose thickness follows the spectrum.
class HypnoSpiral : public Visual
{
public:
    void reset() override { rot = 0; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const auto ctr = r.getCentre();
        const float md = jmin (r.getWidth(), r.getHeight());
        rot += c.dt * c.speed * (0.6f + 2.2f * a.bass + 1.5f * c.pulse());
        const int arms = 2 + (int) (c.detail * 6.0f);
        constexpr int N = 90;
        const float maxR = md * 0.5f;

        for (int set = 0; set < (c.mirror ? 2 : 1); ++set)
        {
            const float dir = set == 0 ? 1.0f : -1.0f;
            for (int arm = 0; arm < arms; ++arm)
            {
                std::array<Point<float>, N + 1> pts;
                Path whole;
                for (int k = 0; k <= N; ++k)
                {
                    const float u = (float) k / (float) N;
                    const float rad = maxR * std::pow (u, 0.9f) * (1.0f + c.pulse() * 0.05f);
                    const float ang = dir * (rot + u * 7.0f) + (float) arm * twoPi / (float) arms;
                    pts[(size_t) k] = { ctr.x + std::cos (ang) * rad, ctr.y + std::sin (ang) * rad };
                    if (k == 0) whole.startNewSubPath (pts[0]); else whole.lineTo (pts[(size_t) k]);
                }
                // one smooth glow pass for the whole arm...
                if (c.glow > 0.01f)
                {
                    g.setColour (c.col (frac (0.5f + (float) arm * 0.13f), 0.12f * c.glow));
                    g.strokePath (whole, PathStrokeType (md * 0.03f * lineScale * (0.5f + a.level), PathStrokeType::curved, PathStrokeType::rounded));
                }
                // ...then opaque segments whose width and brightness follow the spectrum
                for (int k = 1; k <= N; ++k)
                {
                    const float u = (float) k / (float) N;
                    const float v = a.bandAt (u);
                    const float w = (1.0f + v * md * 0.035f) * (0.3f + u) * lineScale;
                    g.setColour (c.col (frac (u + (float) arm * 0.13f)).withMultipliedBrightness (0.35f + 0.65f * v));
                    g.drawLine ({ pts[(size_t) k - 1], pts[(size_t) k] }, w);
                }
            }
        }
        ColourGradient core (c.col (0.9f, 0.6f + 0.4f * c.pulse()), ctr, c.col (0.9f, 0.0f), ctr.translated (md * 0.08f, 0), true);
        g.setGradientFill (core);
        g.fillEllipse (ctr.x - md * 0.08f, ctr.y - md * 0.08f, md * 0.16f, md * 0.16f);
    }

private:
    float rot = 0;
};

//==============================================================================
// 23. Shape Shifter: nested polygons that change shape on every beat.
class ShapeShifter : public Visual
{
public:
    void reset() override { rot = 0; morph = 0; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const auto ctr = r.getCentre();
        const float md = jmin (r.getWidth(), r.getHeight());
        rot += c.dt * c.speed * (0.3f + a.mid);
        if (a.beat) ++step;
        morph += (1.0f - morph) * (1.0f - std::exp (-c.dt / 0.15f));
        if (a.beat) morph = 0.0f;

        const int layers = 6 + (int) (c.detail * 14.0f);
        for (int i = layers - 1; i >= 0; --i)
        {
            const float t = (float) i / (float) jmax (1, layers - 1);
            const float v = a.bandAt (t);
            const int sides = 3 + ((i + step) % 6);
            const float rad = md * 0.46f * ((float) i + 1.0f) / (float) layers * (1.0f + v * 0.18f + c.pulse() * 0.08f * (1.0f - t));
            const float ang0 = rot * ((i % 2 == 0) ? 1.0f : -1.0f) * (1.0f + t) + morph * pi / (float) sides;
            Path p, star;
            for (int s = 0; s <= sides; ++s)
            {
                const float ang = ang0 + (float) s / (float) sides * twoPi;
                const Point<float> pt (ctr.x + std::cos (ang) * rad, ctr.y + std::sin (ang) * rad);
                if (s == 0) p.startNewSubPath (pt); else p.lineTo (pt);
            }
            p.closeSubPath();
            g.setColour (c.col (t, 0.05f + 0.1f * v));
            g.fillPath (p);
            glowStroke (g, p, c.col (t, 0.5f + 0.5f * v), 1.0f + 2.5f * v, c.glow);
            if (c.mirror)
            {
                for (int s = 0; s <= sides * 2; ++s)
                {
                    const float ang = ang0 + (float) s / (float) (sides * 2) * twoPi;
                    const float rr = (s % 2 == 0) ? rad : rad * (0.5f + 0.3f * v);
                    const Point<float> pt (ctr.x + std::cos (-ang) * rr, ctr.y + std::sin (-ang) * rr);
                    if (s == 0) star.startNewSubPath (pt); else star.lineTo (pt);
                }
                glowStroke (g, star, c.col (1.0f - t, 0.4f), 1.0f, c.glow * 0.5f);
            }
        }
    }

private:
    float rot = 0, morph = 0;
    int step = 0;
};

//==============================================================================
// 24. Lightning: a plasma globe whose tendrils follow the bands, plus bolts on beats.
class Lightning : public Visual
{
public:
    void reset() override { bolts.clear(); }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const auto ctr = r.getCentre();
        const float md = jmin (r.getWidth(), r.getHeight());
        spin += c.dt * c.speed * 0.4f;
        const float globe = md * 0.45f;
        const float core = md * (0.045f + 0.03f * a.bass + 0.03f * c.pulse());

        // glass globe
        g.setColour (c.th.text.withAlpha (0.06f));
        g.drawEllipse (ctr.x - globe, ctr.y - globe, globe * 2, globe * 2, 2.0f);

        // tendrils, one per band group
        const int n = 4 + (int) (c.detail * 10.0f);
        for (int i = 0; i < n; ++i)
        {
            const float t = ((float) i + 0.5f) / (float) n;
            const float v = a.bandAt (c.mirror ? std::abs (t * 2.0f - 1.0f) : t);
            if (v < 0.08f) continue;
            const float ang = spin * (1.0f + 0.3f * std::sin ((float) i)) + (float) i / (float) n * twoPi + 0.4f * std::sin (spin * 2.0f + (float) i);
            const float len = core + (globe - core) * jmin (1.0f, 0.4f + v);
            const Point<float> end (ctr.x + std::cos (ang) * len, ctr.y + std::sin (ang) * len);
            Path p; jag (p, ctr, end, 5, md * 0.05f * (0.3f + v), c.rng, true);
            glowStroke (g, p, c.col (t, 0.4f + 0.6f * v), 1.0f + 1.5f * v, c.glow);
            if (len > globe * 0.95f) glowDot (g, end, 2.0f + 3.0f * v, c.col (t), c.glow);
        }

        // big bolts on beats
        if (a.beat && c.react > 0.01f)
        {
            const int count = 1 + (int) (c.react * 2.0f);
            for (int k = 0; k < count; ++k)
            {
                Bolt b;
                const float ang = c.rng.nextFloat() * twoPi;
                const Point<float> end (ctr.x + std::cos (ang) * md * 0.7f, ctr.y + std::sin (ang) * md * 0.7f);
                jag (b.path, ctr, end, 7, md * 0.12f, c.rng, true);
                for (int br = 0; br < 3; ++br)
                {
                    const Point<float> from = ctr + (end - ctr) * (0.3f + 0.4f * c.rng.nextFloat());
                    const float ba = ang + (c.rng.nextFloat() - 0.5f) * 1.6f;
                    jag (b.path, from, from + Point<float> (std::cos (ba), std::sin (ba)) * md * 0.25f, 5, md * 0.05f, c.rng, true);
                }
                b.t = c.rng.nextFloat();
                bolts.push_back (std::move (b));
            }
        }
        for (auto& b : bolts)
        {
            glowStroke (g, b.path, c.col (b.t, b.life).interpolatedWith (Colours::white, 0.3f * b.life), 1.0f + 2.5f * b.life, c.glow);
            b.life -= c.dt * 3.0f;
        }
        bolts.erase (std::remove_if (bolts.begin(), bolts.end(), [] (const Bolt& b) { return b.life <= 0; }), bolts.end());

        // core
        ColourGradient cg (Colours::white.withAlpha (0.9f), ctr, c.col (0.5f, 0.0f), ctr.translated (core * 2.2f, 0), true);
        cg.addColour (0.3, c.col (0.8f, 0.8f));
        g.setGradientFill (cg);
        g.fillEllipse (ctr.x - core * 2.2f, ctr.y - core * 2.2f, core * 4.4f, core * 4.4f);
    }

private:
    struct Bolt { Path path; float life = 1.0f, t = 0; };

    // Midpoint-displacement zig-zag from a to b, appended to p.
    static void jag (Path& p, Point<float> a, Point<float> b, int depth, float disp, Random& rng, bool start)
    {
        std::vector<Point<float>> pts { a, b };
        for (int d = 0; d < depth; ++d)
        {
            std::vector<Point<float>> next;
            for (size_t i = 0; i + 1 < pts.size(); ++i)
            {
                const auto m = (pts[i] + pts[i + 1]) * 0.5f;
                const auto dir = pts[i + 1] - pts[i];
                const float len = dir.getDistanceFromOrigin();
                const Point<float> nrm (-dir.y / jmax (1.0f, len), dir.x / jmax (1.0f, len));
                next.push_back (pts[i]);
                next.push_back (m + nrm * (rng.nextFloat() - 0.5f) * disp);
            }
            next.push_back (pts.back());
            pts = std::move (next);
            disp *= 0.55f;
        }
        if (start) p.startNewSubPath (pts[0]);
        for (size_t i = 1; i < pts.size(); ++i) p.lineTo (pts[i]);
    }

    std::vector<Bolt> bolts;
    float spin = 0;
};

//==============================================================================
// 25. Disco Floor: a light-up dance floor and a mirror ball.
class DiscoFloor : public Visual
{
public:
    bool usesTrails() const override { return false; }
    bool fullBleed() const override { return true; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const float w = r.getWidth(), h = r.getHeight();
        const float horizon = r.getY() + h * 0.38f;
        ballSpin += c.dt * c.speed * 0.8f;
        if (a.beat || pattern.empty()) newPattern (c.rng, c.mirror);
        flash = a.beat ? 1.0f : flash * std::exp (-c.dt / 0.25f);

        // back wall
        ColourGradient wall (c.th.bg2.withRotatedHue (c.hue), r.getCentreX(), horizon, c.th.bg.withRotatedHue (c.hue), r.getCentreX(), r.getY(), false);
        g.setGradientFill (wall);
        g.fillRect (r.withBottom (horizon));

        // light beams from the ball
        const Point<float> ball (r.getCentreX(), r.getY() + h * 0.2f);
        const float br = jmin (w, h) * 0.08f;
        for (int i = 0; i < 10; ++i)
        {
            const float ang = ballSpin * 0.6f + (float) i / 10.0f * twoPi;
            const float sx = std::cos (ang);
            if (std::sin (ang) < -0.2f) continue;   // beams pointing away
            const Point<float> tip (ball.x + sx * w * 0.8f, r.getBottom());
            Path beam;
            beam.addTriangle (ball, tip.translated (-w * 0.03f, 0), tip.translated (w * 0.03f, 0));
            g.setColour (c.col ((float) i / 10.0f, 0.04f + 0.08f * a.level * c.glow));
            g.fillPath (beam);
        }

        // floor tiles in perspective
        constexpr int rows = 8, cols = 10;
        auto yAt = [&] (float e) { return horizon + (r.getBottom() - horizon) * std::pow (e, 1.6f); };
        auto xAt = [&] (float cIdx, float yf) { return r.getCentreX() + (cIdx / cols - 0.5f) * w * (0.55f + 1.3f * yf); };
        for (int j = 0; j < rows; ++j)
        {
            const float e0 = (float) j / rows, e1 = (float) (j + 1) / rows;
            const float y0 = yAt (e0), y1 = yAt (e1);
            const float f0 = (y0 - horizon) / (r.getBottom() - horizon), f1 = (y1 - horizon) / (r.getBottom() - horizon);
            for (int i = 0; i < cols; ++i)
            {
                const int idx = j * cols + i;
                const float bt = (float) ((i + j * 3) % cols) / (float) (cols - 1);
                const float v = a.bandAt (bt);
                const float lit = jlimit (0.0f, 1.0f, (pattern[(size_t) idx] ? 0.55f + 0.45f * flash : 0.0f) + v * 0.6f - 0.15f);
                Path tile;
                const float gap = 0.04f;
                tile.startNewSubPath (xAt ((float) i + gap, f0), y0 + (y1 - y0) * gap);
                tile.lineTo (xAt ((float) i + 1 - gap, f0), y0 + (y1 - y0) * gap);
                tile.lineTo (xAt ((float) i + 1 - gap, f1), y1 - (y1 - y0) * gap);
                tile.lineTo (xAt ((float) i + gap, f1), y1 - (y1 - y0) * gap);
                tile.closeSubPath();
                const auto colr = c.col (frac (bt + tileHue[(size_t) idx]));
                g.setColour (colr.withMultipliedBrightness (0.15f + 0.85f * lit).withMultipliedAlpha (0.35f + 0.65f * lit));
                g.fillPath (tile);
                if (lit > 0.6f && c.glow > 0.01f)
                {
                    g.setColour (colr.withAlpha (0.25f * c.glow * lit));
                    g.strokePath (tile, PathStrokeType (3.0f));
                }
            }
        }

        // mirror ball
        g.setColour (Colours::grey.withAlpha (0.5f));
        g.drawLine (ball.x, r.getY(), ball.x, ball.y - br, 1.5f);
        {
            Graphics::ScopedSaveState ss (g);
            Path sphere; sphere.addEllipse (ball.x - br, ball.y - br, br * 2, br * 2);
            g.reduceClipRegion (sphere);
            g.setColour (Colour (0xff2a2a30));
            g.fillPath (sphere);
            const int fr = 10;
            for (int j = 0; j < fr; ++j)
                for (int i = 0; i < fr * 2; ++i)
                {
                    const float lat = pi * ((float) j + 0.5f) / fr - pi * 0.5f;
                    const float lon = (float) i / (fr * 2) * twoPi + ballSpin;
                    const float cz = std::cos (lat) * std::cos (lon);
                    if (cz < 0) continue;
                    const float px = ball.x + br * std::cos (lat) * std::sin (lon);
                    const float py = ball.y + br * std::sin (lat);
                    const float sz = br * 0.16f * (0.4f + 0.6f * cz);
                    const float sparkle = (std::sin ((float) (i * 7 + j * 13) + ballSpin * 9.0f) > 0.92f) ? 1.0f : 0.0f;
                    g.setColour (Colour (0xffc8c8d0).withMultipliedBrightness (0.4f + 0.6f * cz).interpolatedWith (Colours::white, sparkle));
                    g.fillRect (px - sz * 0.5f, py - sz * 0.5f, sz, sz);
                }
        }
        // sparkles thrown around the room
        Random sr ((int64) (ballSpin * 3.0f));
        const int sparks = 20 + (int) (c.detail * 40.0f);
        for (int i = 0; i < sparks; ++i)
        {
            const float x = r.getX() + sr.nextFloat() * w, y = r.getY() + sr.nextFloat() * h;
            const float s = (1.0f + 2.5f * sr.nextFloat()) * (0.5f + flash);
            glowDot (g, { x, y }, s, c.col (sr.nextFloat(), 0.4f + 0.5f * a.treble), c.glow);
        }
    }

private:
    void newPattern (Random& rng, bool mirror)
    {
        constexpr int rows = 8, cols = 10;
        pattern.assign (rows * cols, false);
        tileHue.resize (rows * cols);
        const int kind = rng.nextInt (4);
        for (int j = 0; j < rows; ++j)
            for (int i = 0; i < cols; ++i)
            {
                const int ii = mirror ? jmin (i, cols - 1 - i) : i;
                bool on = false;
                switch (kind)
                {
                    case 0:  on = ((ii + j + flip) % 2) == 0; break;              // checkerboard
                    case 1:  on = ((ii + j + flip) % 3) == 0; break;              // diagonals
                    case 2:  on = (j % 2 == flip % 2) != (ii % 2 == 0); break;    // stripes
                    default: on = rng.nextFloat() < 0.4f; break;                  // random
                }
                pattern[(size_t) (j * cols + i)] = on;
                tileHue[(size_t) (j * cols + i)] = rng.nextFloat() * 0.3f;
            }
        ++flip;
    }

    std::vector<bool> pattern;
    std::vector<float> tileHue;
    int flip = 0;
    float ballSpin = 0, flash = 0;
};

//==============================================================================
// 26. VFD Analyzer: a vacuum-fluorescent spectrum display from a 70s/80s deck.
class VFDAnalyzer : public Visual
{
public:
    bool usesTrails() const override { return false; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        auto& f = Fonts::get();
        const int bandsN = 12 + 4 * (int) (c.detail * 5.0f);    // 12..32
        constexpr int segs = 22;

        // bezel and glass
        auto panel = r.reduced (r.getWidth() * 0.06f, r.getHeight() * 0.08f);
        g.setColour (Colour (0xff0b0b0d));
        g.fillRoundedRectangle (panel.expanded (10), 10.0f);
        g.setColour (Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (panel.expanded (10), 10.0f, 1.2f);
        ColourGradient glass (Colour (0xff050807), panel.getCentreX(), panel.getY(), Colour (0xff0c1210), panel.getCentreX(), panel.getBottom(), false);
        g.setGradientFill (glass);
        g.fillRoundedRectangle (panel, 6.0f);

        const float labelH = jmax (12.0f, panel.getHeight() * 0.07f);
        auto disp = panel.reduced (panel.getWidth() * 0.06f, labelH * 1.2f).withTrimmedTop (labelH * 1.0f);
        disp.removeFromLeft (labelH * 1.6f);
        const float colW = disp.getWidth() / (float) bandsN;
        const float segH = disp.getHeight() / (float) segs;

        // dB scale on the left
        g.setFont (Fonts::font (f.mono, labelH * 0.7f));
        for (int db = 0; db >= -60; db -= 12)
        {
            const float y = disp.getY() + disp.getHeight() * (float) -db / 72.0f;
            g.setColour (c.col (0.5f, 0.45f));
            g.drawText (String (db), Rectangle<float> (disp.getX() - labelH * 2.2f, y - labelH * 0.5f, labelH * 1.9f, labelH), Justification::centredRight);
        }

        for (int i = 0; i < bandsN; ++i)
        {
            const float u = ((float) i + 0.5f) / (float) bandsN;
            const float t = c.mirror ? std::abs (u * 2.0f - 1.0f) : u;
            const float v = a.bandAt (t) * (1.0f + c.pulse() * 0.1f);
            const int lit = roundToInt (v * segs);
            const int pk = jmin (segs - 1, roundToInt (a.peakAt (t) * segs));
            const float x = disp.getX() + (float) i * colW + colW * 0.12f;
            for (int s = 0; s < segs; ++s)
            {
                const float y = disp.getBottom() - (float) (s + 1) * segH + segH * 0.18f;
                const Rectangle<float> seg (x, y, colW * 0.76f, segH * 0.64f);
                const auto colr = c.col ((float) s / (segs - 1));
                if (s < lit || s == pk)
                {
                    if (c.glow > 0.01f)
                    {
                        g.setColour (colr.withAlpha (0.22f * c.glow));
                        g.fillRect (seg.expanded (segH * 0.3f));
                    }
                    g.setColour (s == pk && s >= lit ? colr.brighter (0.5f) : colr);
                    g.fillRect (seg);
                }
                else
                {
                    g.setColour (colr.withAlpha (0.07f));   // unlit phosphor ghost
                    g.fillRect (seg);
                }
            }
            // frequency labels under the columns that land near a standard band
            const float freq = 30.0f * std::pow (18000.0f / 30.0f, t);
            for (float lf : { 31.5f, 63.0f, 125.0f, 250.0f, 500.0f, 1000.0f, 2000.0f, 4000.0f, 8000.0f, 16000.0f })
            {
                const float spacing = std::pow (18000.0f / 30.0f, 1.0f / (float) bandsN);
                if (freq / lf < std::sqrt (spacing) && lf / freq <= std::sqrt (spacing))
                {
                    g.setColour (c.col (0.5f, 0.6f));
                    g.setFont (Fonts::font (f.mono, labelH * 0.62f));
                    const String s = lf >= 1000.0f ? String ((int) (lf / 1000.0f)) + "k" : String ((int) lf);
                    g.drawText (s, Rectangle<float> (x - colW * 0.2f, disp.getBottom() + labelH * 0.2f, colW * 1.2f, labelH), Justification::centredTop);
                }
            }
        }

        // mesh grid over the glass
        g.setColour (Colours::black.withAlpha (0.25f));
        for (float y = disp.getY(); y < disp.getBottom(); y += 3.0f) g.fillRect (disp.getX(), y, disp.getWidth(), 1.0f);

        // legends
        g.setFont (Fonts::font (f.bold, labelH * 0.7f, 0.25f));
        g.setColour (c.col (0.6f, 0.75f));
        g.drawText ("SPECTRUM ANALYZER", Rectangle<float> (panel.getX() + 14, panel.getY() + 4, panel.getWidth() * 0.5f, labelH), Justification::centredLeft);
        g.drawText (c.mirror ? "L+R  MIRROR" : "L+R  PEAK HOLD", Rectangle<float> (panel.getCentreX(), panel.getY() + 4, panel.getWidth() * 0.5f - 14, labelH), Justification::centredRight);

        // glass sheen
        ColourGradient sheen (Colours::white.withAlpha (0.06f), panel.getX(), panel.getY(), Colours::white.withAlpha (0.0f), panel.getX(), panel.getCentreY(), false);
        g.setGradientFill (sheen);
        g.fillRoundedRectangle (panel.withHeight (panel.getHeight() * 0.5f), 6.0f);
    }
};

//==============================================================================
// 27. Ribbons: tapered sine ribbons, one per band group.
class Ribbons : public Visual
{
public:
    void reset() override { std::fill (std::begin (phase), std::end (phase), 0.0f); }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const int n = 3 + (int) (c.detail * 7.0f);
        const float cy = r.getCentreY();
        const int pts = 160;
        for (int i = 0; i < n; ++i)
        {
            const float t = ((float) i + 0.5f) / (float) n;
            const float v = a.bandAt (t);
            phase[i] += c.dt * c.speed * (1.0f + (float) i * 0.35f) * (1.0f + a.level);
            const float amp = r.getHeight() * 0.42f * (0.08f + v) * (1.0f + c.pulse() * 0.3f);
            const float k = 1.0f + (float) i * 0.55f;
            for (int side = 0; side < (c.mirror ? 2 : 1); ++side)
            {
                const float sgn = side == 0 ? 1.0f : -1.0f;
                Path line, fill;
                fill.startNewSubPath (r.getX(), cy);
                for (int p = 0; p <= pts; ++p)
                {
                    const float u = (float) p / (float) pts;
                    const float env = std::pow (std::sin (pi * u), 2.0f);
                    const float y = cy + sgn * amp * env * std::sin (u * twoPi * k + phase[i] + (float) i);
                    const float x = r.getX() + u * r.getWidth();
                    if (p == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
                    fill.lineTo (x, y);
                }
                fill.lineTo (r.getRight(), cy);
                fill.closeSubPath();
                g.setColour (c.col (t, 0.08f + 0.12f * v));
                g.fillPath (fill);
                glowStroke (g, line, c.col (t, 0.6f + 0.4f * v), 1.2f + 2.0f * v, c.glow);
            }
        }
        g.setColour (c.th.text.withAlpha (0.1f));
        g.fillRect (r.getX(), cy, r.getWidth(), 1.0f);
    }

private:
    float phase[16] {};
};

//==============================================================================
// 28. Galaxy: a tilted spiral galaxy; inner stars ride the bass, outer the treble.
class Galaxy : public Visual
{
public:
    bool fullBleed() const override { return true; }

    Galaxy()
    {
        Random rr (2024);
        for (auto& s : stars)
        {
            s.rad = std::pow (rr.nextFloat(), 0.7f);
            s.arm = rr.nextInt (3);
            s.off = (rr.nextFloat() - 0.5f) * 0.6f * (1.2f - s.rad);
            s.size = 0.6f + 1.8f * rr.nextFloat() * rr.nextFloat();
            s.tw = rr.nextFloat() * twoPi;
        }
    }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        const auto ctr = r.getCentre();
        const float md = jmin (r.getWidth(), r.getHeight()) * 0.48f;
        rot += c.dt * c.speed * (0.12f + 0.4f * a.level);
        const float tilt = c.mirror ? 1.0f : 0.55f;
        const int n = 600 + (int) (c.detail * 2400.0f);

        ColourGradient haze (c.col (0.5f, 0.12f + 0.1f * a.level), ctr, c.col (0.5f, 0.0f), ctr.translated (md, 0), true);
        g.setGradientFill (haze);
        g.fillEllipse (ctr.x - md, ctr.y - md * tilt, md * 2, md * 2 * tilt);

        for (int i = 0; i < n; ++i)
        {
            const auto& s = stars[(size_t) i];
            const float ang = (float) s.arm * twoPi / 3.0f + s.rad * 5.5f + s.off + rot / (0.25f + s.rad);
            const float x = std::cos (ang) * s.rad * md, y = std::sin (ang) * s.rad * md * tilt;
            const float v = a.bandAt (s.rad);
            const float tw = 0.7f + 0.3f * std::sin (s.tw + rot * 20.0f);
            const float alpha = (0.25f + 0.75f * v) * tw;
            const float sz = s.size * (1.0f + v * 1.2f + c.pulse() * 0.5f * (1.0f - s.rad));
            const auto col = c.col (s.rad, alpha);
            if (sz < 1.6f) { g.setColour (col); g.fillRect (ctr.x + x, ctr.y + y, sz, sz); }
            else glowDot (g, { ctr.x + x, ctr.y + y }, sz * 0.6f, col, c.glow * 0.5f);
        }

        const float cr = md * (0.08f + 0.05f * a.bass + 0.05f * c.pulse());
        ColourGradient core (Colours::white.withAlpha (0.85f), ctr, c.col (0.1f, 0.0f), ctr.translated (cr, 0), true);
        core.addColour (0.35, c.col (0.15f, 0.6f));
        g.setGradientFill (core);
        g.fillEllipse (ctr.x - cr, ctr.y - cr * tilt, cr * 2, cr * 2 * tilt);
    }

private:
    struct S { float rad, off, size, tw; int arm; };
    std::array<S, 3000> stars;
    float rot = 0;
};

//==============================================================================
std::unique_ptr<Visual> createVisual2 (int index)
{
    switch (index)
    {
        case 12: return std::make_unique<PulseRings>();
        case 13: return std::make_unique<RetroHorizon>();
        case 14: return std::make_unique<Ridgelines>();
        case 15: return std::make_unique<OrbitScope>();
        case 16: return std::make_unique<LavaLamp>();
        case 17: return std::make_unique<Fireworks>();
        case 18: return std::make_unique<DigitalRain>();
        case 19: return std::make_unique<RippleGrid>();
        case 20: return std::make_unique<FlowField>();
        case 21: return std::make_unique<HypnoSpiral>();
        case 22: return std::make_unique<ShapeShifter>();
        case 23: return std::make_unique<Lightning>();
        case 24: return std::make_unique<DiscoFloor>();
        case 25: return std::make_unique<VFDAnalyzer>();
        case 26: return std::make_unique<Ribbons>();
        default: return std::make_unique<Galaxy>();
    }
}

} // namespace vq
