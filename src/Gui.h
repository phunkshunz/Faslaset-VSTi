// =====================================================================
//  MASKINHALLEN : MOTSTÅND — DOCKLANDS look and custom widgets (step 4b)
//  Rusty shipyard steel, rivets, scraped paint, fluorescent-tube green.
// =====================================================================
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <functional>

namespace dock {

// ---------------------------------------------------------------------
//  Palette
// ---------------------------------------------------------------------
namespace C {
const juce::Colour night    { 0xff100e0c };   // deepest shadow
const juce::Colour iron     { 0xff221e1a };   // panel steel
const juce::Colour iron2    { 0xff2d2823 };
const juce::Colour steel    { 0xff4a443d };
const juce::Colour steelHi  { 0xff7a7268 };
const juce::Colour rust     { 0xff8c4a20 };
const juce::Colour rustDk   { 0xff5a2c12 };
const juce::Colour rustHi   { 0xffb8652c };
const juce::Colour paper    { 0xffe8e0cc };   // stencil paint
const juce::Colour paperDim { 0xff9a9282 };
const juce::Colour tube     { 0xff9cff6a };   // fluorescent tube green
const juce::Colour tubeDim  { 0xff3a6a2a };
const juce::Colour tubeDeep { 0xff16260f };
const juce::Colour alarm    { 0xffff5a30 };   // Smog red
const juce::Colour hazard   { 0xffe8b830 };
}

inline juce::String u (const char* s) { return juce::String::fromUTF8 (s); }

inline juce::Font stencil (float size, bool bold = true)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain))
        .withExtraKerningFactor (0.08f);
}

// ---------------------------------------------------------------------
//  Painting helpers
// ---------------------------------------------------------------------
inline void rivet (juce::Graphics& g, float cx, float cy, float r = 3.2f)
{
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (cx - r + 0.8f, cy - r + 1.2f, r * 2.f, r * 2.f);
    juce::ColourGradient cg (C::steelHi, cx - r * 0.6f, cy - r * 0.6f, C::iron, cx + r, cy + r, true);
    g.setGradientFill (cg);
    g.fillEllipse (cx - r, cy - r, r * 2.f, r * 2.f);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.fillEllipse (cx - r * 0.55f, cy - r * 0.6f, r * 0.6f, r * 0.5f);
}

// a riveted steel plate with a stencilled title
inline void plate (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, bool hazardStripe = false)
{
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (2.f, 3.f), 4.f);
    juce::ColourGradient cg (C::iron2.withAlpha (0.72f), r.getX(), r.getY(), C::iron.withAlpha (0.84f), r.getX(), r.getBottom(), false);
    g.setGradientFill (cg);
    g.fillRoundedRectangle (r, 4.f);
    // rust creeping in from the bottom edge and the corners
    {
        juce::ColourGradient rg (C::rust.withAlpha (0.0f), r.getX(), r.getBottom() - 40.f, C::rust.withAlpha (0.22f), r.getX(), r.getBottom(), false);
        g.setGradientFill (rg); g.fillRoundedRectangle (r.withTrimmedTop (r.getHeight() - 40.f), 4.f);
        juce::Random rnd ((juce::int64) (r.getX() * 31 + r.getY() * 7 + r.getWidth()));
        g.saveState(); g.reduceClipRegion (r.toNearestInt());
        for (int i = 0; i < 14; ++i)
        {
            const bool left = rnd.nextBool();
            const float x = left ? r.getX() + rnd.nextFloat() * 50.f : r.getRight() - rnd.nextFloat() * 50.f;
            const float y = rnd.nextBool() ? r.getY() + rnd.nextFloat() * 30.f : r.getBottom() - rnd.nextFloat() * 30.f;
            const float rr = 6.f + rnd.nextFloat() * 22.f;
            const auto col = (rnd.nextBool() ? C::rust : C::rustHi).withAlpha (0.10f + rnd.nextFloat() * 0.14f);
            juce::ColourGradient cg2 (col, x, y, col.withAlpha (0.f), x + rr, y, true);
            g.setGradientFill (cg2); g.fillEllipse (x - rr, y - rr, rr * 2.f, rr * 1.5f);
        }
        g.restoreState();
    }
    g.setColour (C::steelHi.withAlpha (0.35f));
    g.drawLine (r.getX() + 4.f, r.getY() + 0.5f, r.getRight() - 4.f, r.getY() + 0.5f, 1.f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r, 4.f, 1.2f);

    for (auto p : { r.getTopLeft().translated (8, 8), r.getTopRight().translated (-8, 8),
                    r.getBottomLeft().translated (8, -8), r.getBottomRight().translated (-8, -8) })
        rivet (g, p.x, p.y);

    if (hazardStripe)
    {
        auto strip = juce::Rectangle<float> (r.getRight() - 70.f, r.getY() + 5.f, 52.f, 7.f);
        g.saveState(); g.reduceClipRegion (strip.toNearestInt());
        for (float x = strip.getX() - 10.f; x < strip.getRight() + 10.f; x += 10.f)
        {
            juce::Path p; p.addQuadrilateral (x, strip.getBottom(), x + 5.f, strip.getBottom(), x + 12.f, strip.getY(), x + 7.f, strip.getY());
            g.setColour (C::hazard.withAlpha (0.85f)); g.fillPath (p);
        }
        g.restoreState();
    }
    if (title.isNotEmpty())
    {
        g.setFont (stencil (13.f));
        g.setColour (C::paper.withAlpha (0.88f));
        g.drawText (title, r.reduced (20.f, 6.f).withHeight (16.f), juce::Justification::topLeft, false);
    }
}

// embossed Dymo tape label
inline void dymo (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, juce::Colour tape = juce::Colour (0xff141414))
{
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (1.f, 1.5f), 2.5f);
    g.setColour (tape);
    g.fillRoundedRectangle (r, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.45f), 2.5f);
    g.setFont (stencil (r.getHeight() * 0.58f));
    const auto ri = r.reduced (4.f, 0.f).toNearestInt();
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawFittedText (text, ri.translated (0, 1), juce::Justification::centred, 1, 0.6f);
    g.setColour (C::paper);
    g.drawFittedText (text, ri, juce::Justification::centred, 1, 0.6f);
}

// soft fluorescent glow around a shape colour
inline void glowDot (juce::Graphics& g, float cx, float cy, float r, juce::Colour c, float amount)
{
    if (amount <= 0.01f) return;
    juce::ColourGradient cg (c.withAlpha (0.45f * amount), cx, cy, c.withAlpha (0.f), cx + r * 2.6f, cy, true);
    g.setGradientFill (cg);
    g.fillEllipse (cx - r * 2.6f, cy - r * 2.6f, r * 5.2f, r * 5.2f);
}

// ---------------------------------------------------------------------
//  The rusty shipyard wall, generated once (deterministic)
// ---------------------------------------------------------------------
inline juce::Image makeRustWall (int w, int h)
{
    juce::Image img (juce::Image::RGB, w, h, true);
    juce::Graphics g (img);
    juce::Random rnd (0xD0C7A1D5);

    juce::ColourGradient base (juce::Colour (0xff2a241e), 0, 0, juce::Colour (0xff15120f), (float) w, (float) h, false);
    g.setGradientFill (base);
    g.fillAll();

    // rust blooms
    for (int i = 0; i < 260; ++i)
    {
        const float x = rnd.nextFloat() * w, y = rnd.nextFloat() * h;
        const float r = 8.f + rnd.nextFloat() * 70.f;
        const auto col = (rnd.nextFloat() < 0.7f ? C::rust : C::rustDk).withAlpha (0.06f + rnd.nextFloat() * 0.12f);
        juce::ColourGradient cg (col, x, y, col.withAlpha (0.f), x + r, y, true);
        g.setGradientFill (cg);
        g.fillEllipse (x - r, y - r, r * 2.f, r * 1.6f);
    }
    // rust runs (streaks from the top, like water damage)
    for (int i = 0; i < 70; ++i)
    {
        const float x = rnd.nextFloat() * w, y = rnd.nextFloat() * h * 0.6f, len = 30.f + rnd.nextFloat() * 160.f;
        juce::ColourGradient cg (C::rustHi.withAlpha (0.16f), x, y, C::rustHi.withAlpha (0.f), x, y + len, false);
        g.setGradientFill (cg);
        g.fillRect (x, y, 1.f + rnd.nextFloat() * 2.5f, len);
    }
    // speckle
    for (int i = 0; i < 9000; ++i)
    {
        const float x = rnd.nextFloat() * w, y = rnd.nextFloat() * h;
        g.setColour ((rnd.nextBool() ? C::rustHi : juce::Colours::black).withAlpha (0.08f + rnd.nextFloat() * 0.18f));
        g.fillRect (x, y, 1.f, 1.f);
    }
    // scratches
    for (int i = 0; i < 140; ++i)
    {
        const float x = rnd.nextFloat() * w, y = rnd.nextFloat() * h;
        const float a = rnd.nextFloat() * 6.283f, l = 10.f + rnd.nextFloat() * 60.f;
        g.setColour (juce::Colours::white.withAlpha (0.03f + rnd.nextFloat() * 0.05f));
        g.drawLine (x, y, x + std::cos (a) * l, y + std::sin (a) * l, 0.6f);
    }
    // plate seams with rivet rows
    for (int sx : { w / 3, 2 * w / 3 })
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRect ((float) sx, 0.f, 2.f, (float) h);
        g.setColour (C::steelHi.withAlpha (0.10f)); g.fillRect ((float) sx + 2.f, 0.f, 1.f, (float) h);
        for (int y = 14; y < h; y += 46) rivet (g, (float) sx - 6.f, (float) y, 2.6f);
    }
    // vignette
    juce::ColourGradient vig (juce::Colours::transparentBlack, w * 0.5f, h * 0.45f, juce::Colours::black.withAlpha (0.55f), 0.f, 0.f, true);
    g.setGradientFill (vig);
    g.fillAll();
    return img;
}

// ---------------------------------------------------------------------
//  LookAndFeel
// ---------------------------------------------------------------------
class DockLnF : public juce::LookAndFeel_V4
{
public:
    DockLnF()
    {
        setColour (juce::PopupMenu::backgroundColourId, C::iron);
        setColour (juce::PopupMenu::textColourId, C::paper);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, C::tubeDim);
        setColour (juce::PopupMenu::highlightedTextColourId, C::tube);
        setColour (juce::ComboBox::textColourId, C::tube);
        setColour (juce::Slider::textBoxTextColourId, C::tube);
        setColour (juce::Slider::textBoxBackgroundColourId, C::night);
        setColour (juce::Slider::textBoxOutlineColourId, C::steel);
        setColour (juce::Slider::trackColourId, C::tubeDeep);
        setColour (juce::Slider::backgroundColourId, C::night);
        setColour (juce::Label::textColourId, C::paper);
        setColour (juce::TooltipWindow::backgroundColourId, C::iron);
        setColour (juce::TooltipWindow::textColourId, C::paper);
        setColour (juce::TooltipWindow::outlineColourId, C::tubeDim);
    }

    // knurled steel knob with a green tube indicator
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                           float a0, float a1, juce::Slider& s) override
    {
        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.f);
        const float r = std::min (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const float cx = bounds.getCentreX(), cy = bounds.getCentreY();
        const float ang = a0 + pos * (a1 - a0);
        const bool hot = s.isMouseOverOrDragging();
        const auto lit = s.getProperties().contains ("doom") ? C::alarm : C::tube;   // the UNDERGÅNG knob burns red

        // track
        juce::Path track; track.addCentredArc (cx, cy, r - 1.f, r - 1.f, 0.f, a0, a1, true);
        g.setColour (C::night); g.strokePath (track, juce::PathStrokeType (2.6f));
        juce::Path val; val.addCentredArc (cx, cy, r - 1.f, r - 1.f, 0.f, a0, ang, true);
        g.setColour (lit.withAlpha (0.25f)); g.strokePath (val, juce::PathStrokeType (5.f));
        g.setColour (lit); g.strokePath (val, juce::PathStrokeType (2.f));

        // body
        const float kr = r * 0.74f;
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (cx - kr + 1.f, cy - kr + 2.f, kr * 2.f, kr * 2.f);
        juce::ColourGradient body (C::steelHi, cx - kr * 0.7f, cy - kr * 0.8f, C::iron, cx + kr, cy + kr, true);
        g.setGradientFill (body); g.fillEllipse (cx - kr, cy - kr, kr * 2.f, kr * 2.f);
        // knurling
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        for (int i = 0; i < 28; ++i)
        {
            const float a = (float) i / 28.f * juce::MathConstants<float>::twoPi;
            g.drawLine (cx + std::cos (a) * kr * 0.84f, cy + std::sin (a) * kr * 0.84f, cx + std::cos (a) * kr, cy + std::sin (a) * kr, 0.8f);
        }
        // cap
        const float cr = kr * 0.66f;
        juce::ColourGradient cap (C::steel.brighter (hot ? 0.35f : 0.15f), cx, cy - cr, C::iron2, cx, cy + cr, false);
        g.setGradientFill (cap); g.fillEllipse (cx - cr, cy - cr, cr * 2.f, cr * 2.f);
        // indicator
        const float ix = cx + std::sin (ang) * kr * 0.9f, iy = cy - std::cos (ang) * kr * 0.9f;
        const float jx = cx + std::sin (ang) * kr * 0.3f, jy = cy - std::cos (ang) * kr * 0.3f;
        g.setColour (lit.withAlpha (0.35f)); g.drawLine (jx, jy, ix, iy, 4.f);
        g.setColour (lit); g.drawLine (jx, jy, ix, iy, 1.8f);
    }

    // lamp-style toggles (TYST, MIDI UT, DAW/EGET)
    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.5f);
        const bool on = b.getToggleState();
        const auto lit = b.findColour (juce::ToggleButton::tickColourId);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (0.f, 1.f), 3.f);
        g.setColour (on ? lit.withAlpha (0.22f) : C::night); g.fillRoundedRectangle (r, 3.f);
        g.setColour (on ? lit : (over ? C::steelHi : C::steel)); g.drawRoundedRectangle (r, 3.f, 1.f);
        if (on) { g.setColour (lit.withAlpha (0.12f)); g.fillRoundedRectangle (r.expanded (2.f), 4.f); }
        g.setFont (stencil (std::min (11.f, r.getHeight() * 0.6f)));
        g.setColour (on ? lit : C::paperDim);
        g.drawText (b.getButtonText(), r, juce::Justification::centred, false);
    }

    // stamped steel buttons (dice, SV/EN)
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.f);
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (0.f, down ? 0.5f : 1.5f), 3.f);
        juce::ColourGradient cg (C::steel.brighter (over ? 0.25f : 0.f), r.getX(), r.getY(), C::iron2, r.getX(), r.getBottom(), false);
        g.setGradientFill (cg); g.fillRoundedRectangle (r.translated (0.f, down ? 1.f : 0.f), 3.f);
        g.setColour (C::steelHi.withAlpha (0.4f)); g.drawRoundedRectangle (r, 3.f, 0.8f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        g.setFont (stencil (std::min (12.f, b.getHeight() * 0.55f)));
        g.setColour (b.getToggleState() ? C::tube : C::paper);
        g.drawText (b.getButtonText(), b.getLocalBounds().translated (0, down ? 1 : 0), juce::Justification::centred, false);
    }

    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0.f, 0.f, (float) w, (float) h).reduced (1.f);
        g.setColour (C::night); g.fillRoundedRectangle (r, 3.f);
        g.setColour (C::steel); g.drawRoundedRectangle (r, 3.f, 1.f);
        juce::Path tri; const float tx = r.getRight() - 9.f, ty = r.getCentreY();
        tri.addTriangle (tx - 3.5f, ty - 2.f, tx + 3.5f, ty - 2.f, tx, ty + 2.5f);
        g.setColour (C::tubeDim.brighter (0.4f)); g.fillPath (tri);
    }
    juce::Font getComboBoxFont (juce::ComboBox& b) override { return stencil (std::min (11.5f, b.getHeight() * 0.62f)); }
    void positionComboBoxText (juce::ComboBox& b, juce::Label& l) override
    {
        l.setBounds (4, 0, b.getWidth() - 16, b.getHeight());
        l.setFont (getComboBoxFont (b));
        l.setJustificationType (juce::Justification::centredLeft);
    }
    juce::Font getPopupMenuFont() override { return stencil (13.f); }
    juce::Font getLabelFont (juce::Label&) override { return stencil (11.f); }
};

// ---------------------------------------------------------------------
//  STROBE RING — Technics-style strobe dots around the BPM readout.
//  Dots stand still while the machine holds phase; they crawl as the
//  slip changes, exactly like a platter that's off-pitch.
// ---------------------------------------------------------------------
class StrobeRing : public juce::Component
{
public:
    void update (float slipMs, float bpm, bool running)
    {
        // rotation follows the CHANGE of slip (that's what a strobe shows)
        const float d = slipMs - lastSlip; lastSlip = slipMs;
        if (running) angle += d * 0.9f;
        slip = slipMs; this->bpm = bpm; this->running = running;
        repaint();
    }
    juce::String inPhase = "I FAS", ahead = u ("FÖRE"), behind = "EFTER";

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (4.f);
        const float cx = b.getCentreX(), cy = b.getCentreY(), R = std::min (b.getWidth(), b.getHeight()) * 0.5f;

        // platter
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (cx - R + 2, cy - R + 3, R * 2, R * 2);
        juce::ColourGradient pg (C::steel, cx - R * 0.5f, cy - R * 0.7f, C::night, cx + R, cy + R, true);
        g.setGradientFill (pg); g.fillEllipse (cx - R, cy - R, R * 2, R * 2);
        g.setColour (C::night); g.drawEllipse (cx - R, cy - R, R * 2, R * 2, 1.5f);

        // four rows of strobe dots, like the platter edge
        const float sizes[3] = { 2.5f, 2.2f, 1.9f };
        const int   counts[3] = { 48, 48, 48 };
        const float glow = running ? 1.f : 0.35f;
        for (int row = 0; row < 3; ++row)
        {
            const float rr = R - 8.f - row * 10.f;
            for (int i = 0; i < counts[row]; ++i)
            {
                const float stagger = (row % 2) * 0.5f;
                const float a = angle * (1.f + row * 0.18f) + ((float) i + stagger) / counts[row] * juce::MathConstants<float>::twoPi;
                const float x = cx + std::cos (a) * rr, y = cy + std::sin (a) * rr;
                g.setColour (C::tube.withAlpha (0.35f + 0.55f * glow));
                g.fillEllipse (x - sizes[row], y - sizes[row], sizes[row] * 2, sizes[row] * 2);
            }
        }
        // centre readout
        const float ir = R - 36.f;
        g.setColour (C::night); g.fillEllipse (cx - ir, cy - ir, ir * 2, ir * 2);
        glowDot (g, cx, cy, ir * 0.5f, C::tube, 0.35f * glow);
        g.setColour (C::steel); g.drawEllipse (cx - ir, cy - ir, ir * 2, ir * 2, 1.f);
        g.setFont (stencil (ir * 0.46f));
        g.setColour (C::tube);
        g.drawText (juce::String (bpm, 2), juce::Rectangle<float> (cx - ir, cy - ir * 0.55f, ir * 2, ir * 0.62f), juce::Justification::centred, false);
        g.setFont (stencil (ir * 0.2f));
        g.setColour (C::paperDim);
        g.drawText ("BPM", juce::Rectangle<float> (cx - ir, cy + ir * 0.06f, ir * 2, ir * 0.26f), juce::Justification::centred, false);
        const bool locked = std::abs (slip) < 2.f;
        g.setColour (locked ? C::tube : C::alarm);
        const juce::String s = locked ? inPhase : (juce::String (juce::roundToInt (std::abs (slip))) + " MS " + (slip > 0 ? behind : ahead));
        g.drawText (s, juce::Rectangle<float> (cx - ir, cy + ir * 0.34f, ir * 2, ir * 0.26f), juce::Justification::centred, false);
    }
private:
    float angle = 0.f, lastSlip = 0.f, slip = 0.f, bpm = 133.f; bool running = false;
};

// ---------------------------------------------------------------------
//  JOG WHEEL — grab and push the machine back into phase (NUDGE).
//  Drag right = push forward, left = hold back. Springs to 0 on release.
// ---------------------------------------------------------------------
class JogWheel : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    explicit JogWheel (juce::RangedAudioParameter& p) : param (p) {}
    juce::String label = "NUDGE";

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (4.f);
        b.removeFromBottom (16.f);
        const float cx = b.getCentreX(), cy = b.getCentreY(), R = std::min (b.getWidth(), b.getHeight()) * 0.5f;
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillEllipse (cx - R + 2, cy - R + 3, R * 2, R * 2);
        juce::ColourGradient wg (C::steelHi, cx - R * 0.6f, cy - R * 0.8f, C::iron, cx + R, cy + R, true);
        g.setGradientFill (wg); g.fillEllipse (cx - R, cy - R, R * 2, R * 2);
        // grip ridges, turning with the wheel
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        for (int i = 0; i < 48; ++i)
        {
            const float a = rot + (float) i / 48.f * juce::MathConstants<float>::twoPi;
            g.drawLine (cx + std::cos (a) * R * 0.86f, cy + std::sin (a) * R * 0.86f, cx + std::cos (a) * R * 0.98f, cy + std::sin (a) * R * 0.98f, 1.f);
        }
        const float ir = R * 0.62f;
        juce::ColourGradient ig (C::iron2, cx, cy - ir, C::night, cx, cy + ir, false);
        g.setGradientFill (ig); g.fillEllipse (cx - ir, cy - ir, ir * 2, ir * 2);
        // finger dimple
        const float dx = cx + std::cos (rot - 1.2f) * ir * 0.62f, dy = cy + std::sin (rot - 1.2f) * ir * 0.62f;
        g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillEllipse (dx - 7, dy - 7, 14, 14);
        g.setColour (C::steel); g.drawEllipse (dx - 7, dy - 7, 14, 14, 1.f);
        // push amount arc
        const float v = param.convertFrom0to1 (param.getValue());
        if (std::abs (v) > 0.01f)
        {
            juce::Path arc; arc.addCentredArc (cx, cy, R + 3.f, R + 3.f, 0.f, 0.f, v * 1.4f, true);
            g.setColour ((v > 0 ? C::tube : C::alarm).withAlpha (0.9f)); g.strokePath (arc, juce::PathStrokeType (3.f));
        }
        g.setFont (stencil (11.f)); g.setColour (C::paper);
        g.drawText (label, getLocalBounds().removeFromBottom (16), juce::Justification::centred, false);
    }
    void mouseDown (const juce::MouseEvent& e) override { startX = e.position.x; param.beginChangeGesture(); stopTimer(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float v = juce::jlimit (-1.f, 1.f, (e.position.x - startX) / 60.f);
        param.setValueNotifyingHost (param.convertTo0to1 (v));
        rot += v * 0.12f; repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { startTimerHz (60); }   // spring back
    void spin (float v) { rot += v * 0.08f; repaint(); }
private:
    void timerCallback() override
    {
        float v = param.convertFrom0to1 (param.getValue());
        v *= 0.6f;
        if (std::abs (v) < 0.01f) { v = 0.f; stopTimer(); param.setValueNotifyingHost (param.convertTo0to1 (0.f)); param.endChangeGesture(); }
        else param.setValueNotifyingHost (param.convertTo0to1 (v));
        repaint();
    }
    juce::RangedAudioParameter& param;
    float startX = 0.f, rot = 0.f;
};

// ---------------------------------------------------------------------
//  SMOG METER — analog needle on a dark face that bangs into the red
// ---------------------------------------------------------------------
class SmogMeter : public juce::Component
{
public:
    void setValue (float v)
    {
        // ballistics: fast up, slow fall, like a real VU
        needle += (v - needle) * (v > needle ? 0.6f : 0.08f);
        peak = std::max (peak * 0.985f, v);
        repaint();
    }
    juce::String caption = "KOLLAPS";
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (3.f);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (b.translated (1.5f, 2.f), 5.f);
        g.setColour (C::night); g.fillRoundedRectangle (b, 5.f);
        g.setColour (C::steel); g.drawRoundedRectangle (b, 5.f, 1.2f);
        const float cx = b.getCentreX(), cy = b.getBottom() - 14.f, R = std::min (b.getWidth() * 0.46f, b.getHeight() - 26.f);
        const float a0 = -1.05f, a1 = 1.05f;
        // scale
        for (int i = 0; i <= 20; ++i)
        {
            const float t = (float) i / 20.f, a = a0 + t * (a1 - a0);
            const bool major = i % 5 == 0;
            g.setColour (t > 0.7f ? C::alarm : C::tube.withAlpha (0.8f));
            g.drawLine (cx + std::sin (a) * R * (major ? 0.80f : 0.86f), cy - std::cos (a) * R * (major ? 0.80f : 0.86f),
                        cx + std::sin (a) * R * 0.95f, cy - std::cos (a) * R * 0.95f, major ? 1.6f : 0.9f);
        }
        juce::Path red; red.addCentredArc (cx, cy, R * 0.98f, R * 0.98f, 0.f, a0 + 0.7f * (a1 - a0), a1, true);
        g.setColour (C::alarm.withAlpha (0.5f)); g.strokePath (red, juce::PathStrokeType (3.f));
        if (peak > 0.7f) glowDot (g, cx, cy - R * 0.5f, R * 0.35f, C::alarm, (peak - 0.7f) * 2.5f);
        // needle
        const float a = a0 + juce::jlimit (0.f, 1.f, needle) * (a1 - a0);
        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.drawLine (cx + 1.5f, cy + 1.5f, cx + std::sin (a) * R * 0.92f + 1.5f, cy - std::cos (a) * R * 0.92f + 1.5f, 2.f);
        g.setColour (C::paper);
        g.drawLine (cx, cy, cx + std::sin (a) * R * 0.92f, cy - std::cos (a) * R * 0.92f, 1.6f);
        rivet (g, cx, cy, 5.f);
        g.setFont (stencil (11.f)); g.setColour (C::paperDim);
        g.drawText (caption, b.withTrimmedTop (b.getHeight() * 0.42f).withHeight (14.f), juce::Justification::centred, false);
    }
private:
    float needle = 0.f, peak = 0.f;
};

// ---------------------------------------------------------------------
//  GAIN REDUCTION bar for PRESS (vertical, green -> red as it squeezes)
// ---------------------------------------------------------------------
class GrMeter : public juce::Component
{
public:
    void setValue (float db) { v += (db - v) * (db > v ? 0.6f : 0.15f); repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat().reduced (1.f);
        auto bar = b.withTrimmedBottom (14.f);
        g.setColour (C::night); g.fillRoundedRectangle (bar, 3.f);
        g.setColour (C::steel); g.drawRoundedRectangle (bar, 3.f, 1.f);
        const float frac = juce::jlimit (0.f, 1.f, v / 18.f);
        auto fill = bar.reduced (3.f); fill = fill.withHeight (fill.getHeight() * frac);
        g.setColour ((v > 10.f ? C::alarm : C::tube).withAlpha (0.9f)); g.fillRoundedRectangle (fill, 2.f);
        g.setFont (stencil (9.f)); g.setColour (C::paperDim);
        g.drawText ("GR", b.removeFromBottom (14.f), juce::Justification::centred, false);
    }
private:
    float v = 0.f;
};

} // namespace dock
