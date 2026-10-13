// VisionQuest v1.2 modes (index 28+).
#include "ui/Visuals.h"
#include "ui/Fonts.h"
#include "ui/VisualHelpers.h"

using namespace juce;

namespace vq
{

using namespace detail;

//==============================================================================
// 29. Starship Console: a 24th-century bridge display. Elbow frames, pill bars,
// numbered side blocks, scrolling data columns, a segmented spectrum, a scope,
// level bars and status buttons. Looks its best with the Starship Ops theme,
// but takes its colours from whichever theme is on.
class StarshipConsole : public Visual
{
public:
    bool usesTrails() const override { return false; }

    StarshipConsole()
    {
        Random rr (4747);
        for (auto& row : cascade)
            for (auto& cell : row) cell = randomDigits (rr, 2 + rr.nextInt (4));
        for (auto& l : blockLabels) l = String (rr.nextInt (90) + 10) + "-" + String (rr.nextInt (9000) + 1000);
    }

    void reset() override { flashBlock = 0; highlightTimer = 0; }

    void render (Graphics& g, Rectangle<float> r, const Ctx& c) override
    {
        const auto& a = c.a;
        auto& f = Fonts::get();
        const auto bg = c.th.bg.withRotatedHue (c.hue);
        g.setColour (bg);
        g.fillRect (c.full);

        // Six palette slots. With Starship Ops: periwinkle, lavender, rust, orange, peach, gold.
        auto P = [&c] (int k, float alpha = 1.0f) { return c.col ((float) (k % 6) / 5.0f, alpha); };

        const float H = r.getHeight(), W = r.getWidth();
        const float pad = jmax (6.0f, H * 0.03f);
        const float x0 = r.getX() + pad, x1 = r.getRight() - pad;
        const float y0 = r.getY() + pad, y1 = r.getBottom() - pad;
        const float sideW = jlimit (60.0f, 220.0f, W * 0.13f);
        const float barT = jmax (8.0f, H * 0.036f);
        const float gap = jmax (3.0f, H * 0.008f);
        const float headerH = (y1 - y0) * 0.27f;
        const float elbowReach = x0 + sideW + jmax (30.0f, W * 0.08f);

        //------------------------------------------------------------------ beat bookkeeping
        if (a.beat)
        {
            flashBlock = (flashBlock + 1) % numBlocks;
            highlightRow = c.rng.nextInt (cascadeRows);
            highlightCol = c.rng.nextInt (maxCols);
            highlightTimer = 0.6f;
        }
        highlightTimer = jmax (0.0f, highlightTimer - c.dt);
        const float flash = c.pulse();

        //------------------------------------------------------------------ header frame
        const float hy0 = y0, hy1 = y0 + headerH;
        const float topBlockH = (headerH - barT) * 0.38f;
        sideBlock (g, { x0, hy0, sideW, topBlockH }, P (4), "VQ-" + blockLabels[7].fromFirstOccurrenceOf ("-", false, false), f);
        g.setColour (P (3));
        g.fillPath (elbow (x0, hy0 + topBlockH + gap, hy1, sideW, barT, elbowReach, true));
        barSegments (g, elbowReach + gap, hy1 - barT, x1, barT, gap, { 0.12f, 0.46f, 0.18f, 0.24f }, { 1, 5, 0, 3 }, P, flash);

        //------------------------------------------------------------------ main frame
        const float my0 = hy1 + gap * 2.5f, my1 = y1;
        const float elbowLen = jmax (barT * 2.5f, (my1 - my0) * 0.2f);
        g.setColour (P (1));
        g.fillPath (elbow (x0, my0, my0 + elbowLen, sideW, barT, elbowReach, false));
        barSegments (g, elbowReach + gap, my0, x1, barT, gap, { 0.3f, 0.1f, 0.4f, 0.2f }, { 3, 2, 4, 0 }, P, 0.0f);
        g.setColour (P (0));
        g.fillPath (elbowBottom (x0, my1 - elbowLen, my1, sideW, barT, elbowReach));
        barSegments (g, elbowReach + gap, my1 - barT, x1, barT, gap, { 0.22f, 0.5f, 0.28f }, { 5, 1, 3 }, P, 0.0f);

        // numbered blocks between the elbows; brightness follows the bands, one flashes per beat
        {
            const float by0 = my0 + elbowLen + gap, by1 = my1 - elbowLen - gap;
            static const float weights[numBlocks] = { 0.8f, 1.4f, 0.6f, 1.0f, 0.7f };
            static const int cols[numBlocks] = { 4, 2, 3, 5, 1 };
            float total = 0; for (float w : weights) total += w;
            const float avail = (by1 - by0) - gap * (float) (numBlocks - 1);
            float y = by0;
            for (int i = 0; i < numBlocks; ++i)
            {
                const float h = avail * weights[i] / total;
                if (h < 4.0f) break;
                const float energy = a.bandAt (((float) i + 0.5f) / (float) numBlocks);
                auto col = P (cols[i]).withMultipliedBrightness (0.78f + 0.22f * jmin (1.0f, energy * 1.4f));
                if (i == flashBlock) col = col.interpolatedWith (Colours::white, 0.6f * flash);
                sideBlock (g, { x0, y, sideW, h }, col, blockLabels[(size_t) i], f);
                y += h + gap;
            }
        }

        //------------------------------------------------------------------ header content
        const float cx0 = x0 + sideW + gap * 4.0f;
        auto headerArea = Rectangle<float> (cx0, hy0, x1 - cx0, (hy1 - barT - gap * 2.0f) - hy0);
        {
            const float titleH = jlimit (14.0f, 90.0f, headerArea.getHeight() * 0.46f);
            auto titleBox = headerArea.withHeight (titleH * 1.1f);
            g.setFont (Fonts::font (f.condensed, titleH, 0.04f));
            g.setColour (P (3));
            g.drawText ("SPECTRAL ANALYSIS", titleBox, Justification::topRight);

            // status line under the title
            const float dbL = a.vuL, dbR = a.vuR;
            const String status = "LEVEL " + fmtDb (jmax (dbL, dbR)) + "   BEATS " + String (a.beatCount % 10000).paddedLeft ('0', 4)
                                + "   BANDS " + String (Analyzer::numBands).paddedLeft ('0', 3);
            g.setFont (Fonts::font (f.condensed, titleH * 0.36f, 0.08f));
            g.setColour (P (1));
            g.drawText (status, headerArea.withTrimmedTop (titleH * 1.12f).withHeight (titleH * 0.42f), Justification::topRight);

            // scrolling data columns on the left
            const int ncols = jlimit (4, maxCols, 4 + (int) (c.detail * (float) (maxCols - 4)));
            const auto cascadeArea = headerArea.withWidth (headerArea.getWidth() * 0.5f);
            const float rowH = cascadeArea.getHeight() / (float) cascadeRows;
            const float colW = cascadeArea.getWidth() / (float) ncols;
            const float pChange = c.dt * c.speed * (1.0f + 10.0f * a.level);
            g.setFont (Fonts::font (f.condensed, jmin (rowH * 0.82f, colW * 0.36f), 0.06f));
            static const int colColours[maxCols] = { 3, 4, 1, 0, 4, 3, 5, 1, 2, 4 };
            for (int rr = 0; rr < cascadeRows; ++rr)
                for (int cc = 0; cc < ncols; ++cc)
                {
                    auto& cell = cascade[(size_t) rr][(size_t) cc];
                    if (c.rng.nextFloat() < pChange) cell = randomDigits (c.rng, cell.length());
                    const bool hi = highlightTimer > 0.0f && rr == highlightRow && cc == highlightCol % ncols;
                    const Rectangle<float> box (cascadeArea.getX() + (float) cc * colW, cascadeArea.getY() + (float) rr * rowH, colW * 0.92f, rowH);
                    if (hi)
                    {
                        g.setColour (P (4));
                        g.fillRect (box.reduced (0, rowH * 0.08f));
                        g.setColour (bg);
                    }
                    else
                        g.setColour (P (colColours[cc], rr == 1 || rr == 3 ? 0.75f : 1.0f));
                    g.drawText (cell, box, Justification::centredRight);
                }
        }

        //------------------------------------------------------------------ main content
        auto main = Rectangle<float> (cx0, my0 + barT + gap * 3.0f, x1 - cx0, (my1 - barT - gap * 3.0f) - (my0 + barT + gap * 3.0f));
        if (main.getHeight() < 20.0f) return;
        auto spec = main.removeFromLeft (main.getWidth() * 0.58f);
        main.removeFromLeft (gap * 5.0f);
        drawSpectrum (g, spec, c, P, gap);
        drawSidePanel (g, main, c, P, gap, bg);
    }

private:
    static constexpr int numBlocks = 5, cascadeRows = 4, maxCols = 10;

    //------------------------------------------------------------------ shapes
    // Sidebar that sweeps into a horizontal bar. bottom=true: the bar runs
    // along the bottom (header style); false: along the top (main frame).
    static Path elbow (float x0, float yTop, float yBot, float sideW, float barT, float barEndX, bool bottom)
    {
        const float ro = jmin (sideW * 0.55f, (yBot - yTop) * 0.7f);
        const float ri = barT * 1.1f;
        Path p;
        if (bottom)
        {
            p.startNewSubPath (x0, yTop);
            p.lineTo (x0 + sideW, yTop);
            p.lineTo (x0 + sideW, yBot - barT - ri);
            p.quadraticTo (x0 + sideW, yBot - barT, x0 + sideW + ri, yBot - barT);
            p.lineTo (barEndX, yBot - barT);
            p.lineTo (barEndX, yBot);
            p.lineTo (x0 + ro, yBot);
            p.quadraticTo (x0, yBot, x0, yBot - ro);
        }
        else
        {
            p.startNewSubPath (x0, yBot);
            p.lineTo (x0, yTop + ro);
            p.quadraticTo (x0, yTop, x0 + ro, yTop);
            p.lineTo (barEndX, yTop);
            p.lineTo (barEndX, yTop + barT);
            p.lineTo (x0 + sideW + ri, yTop + barT);
            p.quadraticTo (x0 + sideW, yTop + barT, x0 + sideW, yTop + barT + ri);
            p.lineTo (x0 + sideW, yBot);
        }
        p.closeSubPath();
        return p;
    }

    // Bottom-left elbow of the main frame: sidebar coming down into a bar along the bottom.
    static Path elbowBottom (float x0, float yTop, float yBot, float sideW, float barT, float barEndX)
    {
        return elbow (x0, yTop, yBot, sideW, barT, barEndX, true);
    }

    // A run of coloured bar segments with black gaps, ending in a rounded cap.
    template <typename PalFn>
    static void barSegments (Graphics& g, float xStart, float y, float xEnd, float h, float gap,
                             std::initializer_list<float> widths, std::initializer_list<int> colours, PalFn&& P, float flash)
    {
        const int n = (int) widths.size();
        const float avail = (xEnd - xStart) - gap * (float) (n - 1);
        if (avail <= 0.0f) return;
        float x = xStart;
        auto wi = widths.begin();
        auto ci = colours.begin();
        for (int i = 0; i < n; ++i, ++wi, ++ci)
        {
            const float w = avail * *wi;
            auto col = P (*ci);
            if (flash > 0.01f && i == 1) col = col.interpolatedWith (P (2), jmin (1.0f, flash * 1.5f));
            g.setColour (col);
            Path seg;
            const bool last = i == n - 1;
            seg.addRoundedRectangle (x, y, w, h, h * 0.5f, h * 0.5f, false, last, false, last);
            g.fillPath (seg);
            x += w + gap;
        }
    }

    // A side block with its number in the lower right corner, in background colour.
    static void sideBlock (Graphics& g, Rectangle<float> b, Colour col, const String& label, Fonts& f)
    {
        g.setColour (col);
        g.fillRect (b);
        const float th = jlimit (8.0f, 22.0f, jmin (b.getHeight() * 0.42f, b.getWidth() * 0.2f));
        if (b.getHeight() < th * 1.2f) return;
        g.setFont (Fonts::font (f.condensed, th, 0.05f));
        g.setColour (Colours::black.withAlpha (0.92f));
        g.drawText (label, b.reduced (b.getWidth() * 0.07f, th * 0.15f), Justification::bottomRight);
    }

    //------------------------------------------------------------------ spectrum
    template <typename PalFn>
    void drawSpectrum (Graphics& g, Rectangle<float> area, const Ctx& c, PalFn&& P, float gap)
    {
        const auto& a = c.a;
        auto& f = Fonts::get();
        const float labelH = jlimit (10.0f, 22.0f, area.getHeight() * 0.07f);
        auto labels = area.removeFromBottom (labelH * 1.3f);

        // caption
        g.setFont (Fonts::font (f.condensed, labelH * 0.95f, 0.08f));
        g.setColour (P (1));
        g.drawText (c.mirror ? "FREQUENCY  //  SYMMETRIC" : "FREQUENCY  DOMAIN", area.removeFromTop (labelH * 1.3f), Justification::topLeft);

        const int ncol = 16 + (int) (c.detail * 24.0f);
        constexpr int S = 18;
        const float colW = area.getWidth() / (float) ncol;
        const float segH = area.getHeight() / (float) S;
        const float cgap = jmax (1.5f, colW * 0.16f), sgap = jmax (1.5f, segH * 0.22f);

        for (int i = 0; i < ncol; ++i)
        {
            const float u = ((float) i + 0.5f) / (float) ncol;
            const float t = c.mirror ? std::abs (u * 2.0f - 1.0f) : u;
            const float v = jmin (1.0f, a.bandAt (t) * (1.0f + c.pulse() * 0.12f));
            const int lit = roundToInt (v * S);
            const int pk = jmin (S - 1, roundToInt (a.peakAt (t) * S));
            const float x = area.getX() + (float) i * colW;
            for (int s = 0; s < S; ++s)
            {
                const float fr = (float) s / (float) (S - 1);
                const auto col = fr < 0.56f ? P (0) : (fr < 0.82f ? P (3) : P (2));
                const Rectangle<float> seg (x, area.getBottom() - (float) (s + 1) * segH + sgap * 0.5f, colW - cgap, segH - sgap);
                if (s < lit)
                {
                    if (c.glow > 0.01f)
                    {
                        g.setColour (col.withAlpha (0.12f * c.glow));
                        g.fillRect (seg.expanded (cgap * 0.6f, sgap * 0.6f));
                    }
                    g.setColour (col);
                }
                else if (s == pk && pk > 0)
                    g.setColour (P (4));
                else
                    g.setColour (col.withAlpha (0.1f));
                g.fillRoundedRectangle (seg, jmin (2.5f, seg.getHeight() * 0.3f));
            }
        }

        // frequency marks
        g.setFont (Fonts::font (f.condensed, labelH, 0.05f));
        for (float lf : { 60.0f, 250.0f, 1000.0f, 4000.0f, 16000.0f })
        {
            if (c.mirror && lf < 100.0f) continue;   // the two 60s would collide in the middle
            const float t = std::log (lf / 30.0f) / std::log (600.0f);
            for (int side = 0; side < (c.mirror ? 2 : 1); ++side)
            {
                const float u = c.mirror ? (side == 0 ? 0.5f - t * 0.5f : 0.5f + t * 0.5f) : t;
                const float x = area.getX() + u * area.getWidth();
                const String s = lf >= 1000.0f ? String ((int) (lf / 1000.0f)) + "K" : String ((int) lf);
                g.setColour (P (4, 0.85f));
                g.drawText (s, Rectangle<float> (x - 30, labels.getY() + gap * 0.5f, 60, labelH), Justification::centredTop);
                g.setColour (P (1, 0.5f));
                g.fillRect (x - 0.5f, labels.getY() - gap * 0.2f, 1.0f, gap);
            }
        }
    }

    //------------------------------------------------------------------ right-hand panel
    template <typename PalFn>
    void drawSidePanel (Graphics& g, Rectangle<float> area, const Ctx& c, PalFn&& P, float gap, Colour bg)
    {
        const auto& a = c.a;
        auto& f = Fonts::get();
        if (area.getWidth() < 40.0f) return;
        const float labelH = jlimit (10.0f, 22.0f, area.getHeight() * 0.07f);

        // scope in a bracket frame
        auto scope = area.removeFromTop (area.getHeight() * 0.44f);
        g.setFont (Fonts::font (f.condensed, labelH * 0.95f, 0.08f));
        g.setColour (P (1));
        g.drawText ("WAVEFORM", scope.removeFromTop (labelH * 1.3f), Justification::topRight);
        {
            const float bw = jmax (5.0f, scope.getWidth() * 0.025f);
            for (int side = 0; side < 2; ++side)
            {
                const float x = side == 0 ? scope.getX() : scope.getRight() - bw;
                Path br;
                br.addRoundedRectangle (x, scope.getY(), bw, scope.getHeight(), bw * 0.5f, bw * 0.5f,
                                        side == 0, side == 1, side == 0, side == 1);
                g.setColour (P (side == 0 ? 0 : 1));
                g.fillPath (br);
            }
            auto inner = scope.reduced (bw * 2.2f, gap);
            g.setColour (P (1, 0.18f));
            for (int k = 1; k < 4; ++k) g.fillRect (inner.getX(), inner.getY() + inner.getHeight() * (float) k / 4.0f, inner.getWidth(), 1.0f);
            Path wave;
            const int pts = jlimit (32, 300, (int) inner.getWidth());
            float peak = 1.0e-4f;
            for (int i = 0; i < Analyzer::scopeSize / 2; ++i) peak = jmax (peak, std::abs (0.5f * (a.scopeL[i] + a.scopeR[i])));
            scopeGain += (jlimit (1.0f, 12.0f, 0.8f / peak) - scopeGain) * (1.0f - std::exp (-c.dt / 0.5f));
            for (int i = 0; i < pts; ++i)
            {
                const int si = i * (Analyzer::scopeSize / 2 - 1) / (pts - 1);
                const float v = 0.5f * (a.scopeL[si] + a.scopeR[si]);
                const float x = inner.getX() + inner.getWidth() * (float) i / (float) (pts - 1);
                const float y = inner.getCentreY() - jlimit (-1.0f, 1.0f, v * scopeGain) * inner.getHeight() * 0.46f;
                if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
            }
            glowStroke (g, wave, P (4), 1.6f, c.glow * 0.6f);
        }
        area.removeFromTop (gap * 3.0f);

        // L / R level bars
        auto levels = area.removeFromTop (area.getHeight() * 0.42f);
        {
            const float rowH = levels.getHeight() / 2.0f;
            for (int ch = 0; ch < 2; ++ch)
            {
                auto row = levels.removeFromTop (rowH).reduced (0, rowH * 0.16f);
                const float db = ch == 0 ? a.vuL : a.vuR;
                const float fill = jlimit (0.0f, 1.0f, (db + 48.0f) / 48.0f);
                auto tag = row.removeFromLeft (jmax (24.0f, row.getHeight() * 1.3f));
                Path tp; tp.addRoundedRectangle (tag.getX(), tag.getY(), tag.getWidth(), tag.getHeight(), tag.getHeight() * 0.5f, tag.getHeight() * 0.5f, true, false, true, false);
                g.setColour (P (ch == 0 ? 0 : 1));
                g.fillPath (tp);
                g.setColour (bg);
                g.setFont (Fonts::font (f.condensed, tag.getHeight() * 0.7f));
                g.drawText (ch == 0 ? "L" : "R", tag.withTrimmedLeft (tag.getWidth() * 0.25f), Justification::centred);
                row.removeFromLeft (gap);
                auto readout = row.removeFromRight (jmax (40.0f, row.getHeight() * 2.2f));
                g.setColour (P (3, 0.15f));
                g.fillRect (row);
                g.setColour (db > -3.0f ? P (2) : P (3));
                g.fillRect (row.withWidth (row.getWidth() * fill));
                g.setColour (P (4));
                g.setFont (Fonts::font (f.condensed, readout.getHeight() * 0.8f, 0.04f));
                g.drawText (fmtDb (db), readout, Justification::centredRight);
            }
        }
        area.removeFromTop (gap * 2.0f);

        // status buttons
        {
            const struct { const char* name; float value; float threshold; int colour; } btn[4] {
                { "BASS",   a.bass,       0.45f, 3 },
                { "MID",    a.mid,        0.40f, 4 },
                { "HIGH",   a.treble,     0.35f, 0 },
                { "BEAT",   a.beatPulse,  0.35f, 2 },
            };
            const float bh = jmin (area.getHeight() * 0.8f, labelH * 2.4f);
            auto row = area.withHeight (bh);
            const float bw = (row.getWidth() - gap * 3.0f) / 4.0f;
            g.setFont (Fonts::font (f.condensed, bh * 0.42f, 0.06f));
            for (int i = 0; i < 4; ++i)
            {
                const Rectangle<float> b (row.getX() + (float) i * (bw + gap), row.getY(), bw, bh);
                const bool on = btn[i].value > btn[i].threshold;
                g.setColour (on ? P (btn[i].colour) : P (btn[i].colour, 0.22f));
                g.fillRoundedRectangle (b, bh * 0.5f);
                g.setColour (on ? bg : P (btn[i].colour, 0.8f));
                g.drawText (btn[i].name, b.reduced (bh * 0.35f, 0), Justification::centredRight);
            }
        }
    }

    static String fmtDb (float db)
    {
        if (db < -60.0f) return "-INF";
        return (db >= 0 ? "+" : "") + String (db, 1);
    }

    static String randomDigits (Random& rr, int len)
    {
        String s;
        for (int i = 0; i < len; ++i) s << String (rr.nextInt (10));
        return s;
    }

    std::array<std::array<String, maxCols>, cascadeRows> cascade;
    std::array<String, 8> blockLabels;
    int flashBlock = 0, highlightRow = 0, highlightCol = 0;
    float highlightTimer = 0, scopeGain = 1.0f;
};

//==============================================================================
std::unique_ptr<Visual> createVisual3 (int)
{
    return std::make_unique<StarshipConsole>();
}

} // namespace vq
