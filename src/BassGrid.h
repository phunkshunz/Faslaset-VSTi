#pragma once

#include "Gui.h"
#include "PluginProcessor.h"

// Basgången: 8 slag × 3 basnoter (16-del 2, 3 och 4 efter kicken).
//   klick        = noten på/av
//   dra upp/ner  = byt ton i skalan
//   dubbelklick  = tillbaka till grundtonen
//   högerklick   = oktav och betoning
class BassGrid : public juce::Component
{
public:
    explicit BassGrid (FaslasetProcessor& p) : proc (p) {}

    void refresh()
    {
        const int beat = proc.uiPlaying.load() ? proc.playingBeat() : -1;
        uint32_t h = (uint32_t) beat * 2654435761u;
        for (int b = 0; b < fl::kMaxBeats; ++b)
            for (int s = 0; s < fl::kSlots; ++s)
                h = h * 31u + proc.pattern.get (b, s).pack();
        h ^= (uint32_t) (proc.get (fl::PBTon) * 7.0f + proc.get (fl::PBSkala) * 131.0f + proc.get (fl::PBSlag) * 1031.0f + proc.get (fl::PLage) * 7919.0f);
        if (h != lastHash) { lastHash = h; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        using namespace ArmColours;
        const int beats = juce::roundToInt (proc.get (fl::PBSlag));
        const bool slap = proc.get (fl::PLage) > 0.5f;
        const int root = juce::roundToInt (proc.get (fl::PBTon));
        const int scale = juce::roundToInt (proc.get (fl::PBSkala));
        const int playing = proc.uiPlaying.load() ? proc.playingBeat() : -1;

        for (int b = 0; b < fl::kMaxBeats; ++b)
        {
            auto col = column (b);
            const bool active = b < beats;

            // slagnummer + kickmarkering
            auto head = col.removeFromTop (18);
            g.setColour (b == playing && active ? kick : (active ? muted : line2));
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText ("K" + juce::String (b + 1), head, juce::Justification::centred);

            for (int s = 0; s < fl::kSlots; ++s)
            {
                auto r = cellRect (b, s).toFloat();
                const auto c = proc.pattern.get (b, s);
                const bool usable = active && (! slap || s == 0);
                g.setColour (usable ? raised : panel);
                g.fillRoundedRectangle (r, 4.0f);
                if (usable && c.on)
                {
                    const float a = 0.55f + 0.45f * (float) c.vel / 127.0f;
                    g.setColour (bass.withAlpha (a * (c.degree == 0 && c.oct == 0 ? 0.75f : 1.0f)));
                    g.fillRoundedRectangle (r.reduced (1.5f), 3.5f);
                    const int note = root + fl::degreeToSemis (scale, c.degree) + 12 * c.oct;
                    g.setColour (ground);
                    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                    g.drawText (fl::noteName (note), r, juce::Justification::centred);
                    if (c.oct != 0)
                    {
                        g.setFont (juce::FontOptions (9.0f, juce::Font::bold));
                        g.drawText (c.oct > 0 ? "+8" : "-8", r.reduced (4.0f, 2.0f), juce::Justification::topRight);
                    }
                }
                else if (usable)
                {
                    g.setColour (line2);
                    g.fillEllipse (r.getCentreX() - 2.0f, r.getCentreY() - 2.0f, 4.0f, 4.0f);
                }
                g.setColour (b == playing && active ? kick.withAlpha (0.7f) : line);
                g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
            }
        }

        if (slap)
        {
            g.setColour (muted);
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (juce::String::fromUTF8 ("SLÄP: en lång basnot per slag (översta raden)"),
                        getLocalBounds().removeFromBottom (14), juce::Justification::centredLeft);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        hit = cellAt (e.getPosition());
        dragged = false;
        if (hit.x < 0) return;
        proc.pushUndo();
        startCell = proc.pattern.get (hit.x, hit.y);
        if (e.mods.isPopupMenu()) { showMenu(); return; }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (hit.x < 0 || e.mods.isPopupMenu()) return;
        const int steps = -e.getDistanceFromDragStartY() / 12;
        if (steps == 0 && ! dragged) return;
        dragged = true;
        auto c = startCell;
        c.on = true;
        c.degree = juce::jlimit (-7, 14, startCell.degree + steps);
        set (c);
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (hit.x < 0 || e.mods.isPopupMenu() || dragged) return;
        auto c = startCell;
        c.on = ! c.on;
        set (c);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto h = cellAt (e.getPosition());
        if (h.x < 0) return;
        fl::Cell c;
        c.on = true;
        set (c, h);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const auto h = cellAt (e.getPosition());
        if (h.x < 0 || std::abs (w.deltaY) < 0.01f) return;
        auto c = proc.pattern.get (h.x, h.y);
        c.on = true;
        c.degree = juce::jlimit (-7, 14, c.degree + (w.deltaY > 0 ? 1 : -1));
        set (c, h);
    }

private:
    juce::Rectangle<int> column (int b) const
    {
        const int w = getWidth() / fl::kMaxBeats;
        return { b * w, 0, w, getHeight() - 16 };
    }

    juce::Rectangle<int> cellRect (int b, int s) const
    {
        auto col = column (b).withTrimmedTop (18).reduced (3, 0);
        const int h = col.getHeight() / fl::kSlots;
        return { col.getX(), col.getY() + s * h + 2, col.getWidth(), h - 4 };
    }

    juce::Point<int> cellAt (juce::Point<int> pos) const
    {
        const bool slap = proc.get (fl::PLage) > 0.5f;
        const int beats = juce::roundToInt (proc.get (fl::PBSlag));
        for (int b = 0; b < beats; ++b)
            for (int s = 0; s < (slap ? 1 : fl::kSlots); ++s)
                if (cellRect (b, s).contains (pos)) return { b, s };
        return { -1, -1 };
    }

    void set (const fl::Cell& c, juce::Point<int> where = { -1, -1 })
    {
        if (where.x < 0) where = hit;
        proc.pattern.set (where.x, where.y, c);
        // mönstret upprepas: slag efter längden följer med
        const int beats = juce::roundToInt (proc.get (fl::PBSlag));
        for (int b = where.x + beats; b < fl::kMaxBeats; b += beats)
            proc.pattern.set (b, where.y, c);
        proc.edited();
        lastHash = 0;
        repaint();
    }

    void showMenu()
    {
        juce::PopupMenu m;
        const auto c = startCell;
        m.addSectionHeader ("OKTAV");
        m.addItem (1, "-1 OKTAV", true, c.oct == -1);
        m.addItem (2, "NORMAL", true, c.oct == 0);
        m.addItem (3, "+1 OKTAV", true, c.oct == 1);
        m.addSectionHeader ("BETONING");
        m.addItem (4, "SVAG", true, c.vel < 90);
        m.addItem (5, "MELLAN", true, c.vel >= 90 && c.vel < 115);
        m.addItem (6, "STARK", true, c.vel >= 115);
        m.addSeparator();
        m.addItem (7, "GRUNDTON");
        m.addItem (8, juce::String::fromUTF8 ("TYST"));
        const auto where = hit;
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this, where, c] (int r) {
            if (r <= 0) return;
            auto n = c;
            n.on = true;
            if (r <= 3) n.oct = r - 2;
            else if (r == 4) n.vel = 70;
            else if (r == 5) n.vel = 100;
            else if (r == 6) n.vel = 127;
            else if (r == 7) { n.degree = 0; n.oct = 0; }
            else if (r == 8) n.on = false;
            set (n, where);
        });
    }

    FaslasetProcessor& proc;
    juce::Point<int> hit { -1, -1 };
    fl::Cell startCell;
    bool dragged = false;
    uint32_t lastHash = 0;
};
