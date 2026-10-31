#pragma once

#include "Gui.h"
#include "Engine.h"

// Oscilloskopet. Ett svep per slag, triggat på kickens start, så bilden står still.
// Kick i syragrönt, bas i magenta, och en cyan markering där basen startar (fas 0).
class Scope : public juce::Component
{
public:
    static constexpr int kBins = 640;

    // GUI-tråden: töm FIFO:n och sortera in ramarna i svepet.
    void pull (fl::ScopeFifo& fifo, double samplesPerBeat)
    {
        samplesPerBeat = juce::jlimit (1000.0, 200000.0, samplesPerBeat);
        bool any = false;
        for (int guard = 0; guard < 8; ++guard)
        {
            const uint32_t n = fifo.pop (frames.data(), (uint32_t) frames.size());
            if (n == 0) break;
            any = true;
            for (uint32_t i = 0; i < n; ++i)
            {
                const auto& f = frames[i];
                if (f.flags & fl::kKickStart)
                {
                    if (idx > 0) shown = cur;
                    cur = {};
                    idx = 0;
                }
                const int b = (int) ((double) idx * kBins / samplesPerBeat);
                if (b >= 0 && b < kBins)
                {
                    auto& bin = cur[(size_t) b];
                    bin.kmin = juce::jmin (bin.kmin, f.kick);
                    bin.kmax = juce::jmax (bin.kmax, f.kick);
                    bin.bmin = juce::jmin (bin.bmin, f.bass);
                    bin.bmax = juce::jmax (bin.bmax, f.bass);
                    if (f.flags & fl::kBassStart) bin.bassStart = true;
                    bin.used = true;
                }
                ++idx;
            }
        }
        if (any) repaint();
    }

    void setLabels (bool spikrakt) { spik = spikrakt; repaint(); }

    void paint (juce::Graphics& g) override
    {
        using namespace ArmColours;
        auto r = getLocalBounds().toFloat();
        g.setColour (ground);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (line);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

        auto plot = r.reduced (10.0f, 22.0f);
        const float midY = plot.getCentreY();
        const float half = plot.getHeight() * 0.5f;

        // rutnät: fyra 16-delar
        for (int s = 0; s <= 4; ++s)
        {
            const float x = plot.getX() + plot.getWidth() * s / 4.0f;
            g.setColour (s == 0 || s == 4 ? line2 : line);
            g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
            if (s < 4)
            {
                g.setColour (muted);
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
                const char* lab = spik ? (s == 0 ? "KICK" : "BAS") : (s == 0 ? "KICK" : "");
                g.drawText (juce::String::fromUTF8 (lab), juce::Rectangle<float> (x + 6.0f, r.getY() + 4.0f, 60.0f, 14.0f), juce::Justification::centredLeft);
            }
        }
        g.setColour (line);
        g.drawHorizontalLine ((int) midY, plot.getX(), plot.getRight());

        const auto& d = hasShown() ? shown : cur;
        const float bw = plot.getWidth() / (float) kBins;

        auto drawLane = [&] (bool isKick, juce::Colour c) {
            g.setColour (c);
            for (int i = 0; i < kBins; ++i)
            {
                const auto& b = d[(size_t) i];
                if (! b.used) continue;
                const float lo = isKick ? b.kmin : b.bmin;
                const float hi = isKick ? b.kmax : b.bmax;
                if (lo == 0.0f && hi == 0.0f) continue;
                const float x = plot.getX() + i * bw;
                const float y1 = midY - juce::jlimit (-1.0f, 1.0f, hi) * half;
                const float y2 = midY - juce::jlimit (-1.0f, 1.0f, lo) * half;
                g.fillRect (x, y1, juce::jmax (1.0f, bw), juce::jmax (1.0f, y2 - y1));
            }
        };
        drawLane (false, bass);
        drawLane (true, kick);

        // fas 0-markeringar
        for (int i = 0; i < kBins; ++i)
            if (d[(size_t) i].bassStart)
            {
                const float x = plot.getX() + i * bw;
                g.setColour (concrete.withAlpha (0.55f));
                g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
                juce::Path tri;
                tri.addTriangle (x - 4.0f, plot.getBottom() + 3.0f, x + 4.0f, plot.getBottom() + 3.0f, x, plot.getBottom() - 3.0f);
                g.setColour (concrete);
                g.fillPath (tri);
            }

        // teckenförklaring
        auto leg = juce::Rectangle<float> (r.getRight() - 200.0f, r.getBottom() - 18.0f, 190.0f, 14.0f);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.setColour (kick);     g.drawText ("KICK", leg.removeFromLeft (44), juce::Justification::centredLeft);
        g.setColour (bass);     g.drawText ("BAS", leg.removeFromLeft (38), juce::Justification::centredLeft);
        g.setColour (concrete); g.drawText ("FAS 0", leg, juce::Justification::centredLeft);
    }

private:
    struct Bin
    {
        float kmin = 0.0f, kmax = 0.0f, bmin = 0.0f, bmax = 0.0f;
        bool bassStart = false, used = false;
    };
    using Sweep = std::array<Bin, kBins>;

    bool hasShown() const
    {
        for (auto& b : shown) if (b.used) return true;
        return false;
    }

    std::array<fl::ScopeFrame, 4096> frames {};
    Sweep cur {}, shown {};
    int64_t idx = 0;
    bool spik = true;
};
