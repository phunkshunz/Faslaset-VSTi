// MASKINHALLEN : MOTSTÅND — the DOCKLANDS panel (step 4b)
#include "PluginEditor.h"
#include "Presets.h"
#include <numeric>

using namespace motstand;

namespace dock {

static const char* const kSlipModeEN[kNumSlipModes] = { "DIODE", "GERMANIUM", "TAPE", "FOLD", "RECTIFIER", "ROM" };

// =====================================================================
//  PolyRings
// =====================================================================
void PolyRings::paint (juce::Graphics& g)
{
    auto b = getLocalBounds().toFloat();
    const float cx = b.getCentreX(), cy = b.getCentreY(), R = std::min (b.getWidth(), b.getHeight()) * 0.5f - 6.f;
    const bool running = proc.isRunning();
    const int64_t step = proc.getStepIndex();

    // which lanes are alive
    std::vector<int> active;
    for (int i = 0; i < kNumInstruments; ++i)
    {
        if (proc.apvts.getRawParameterValue (pid::inst (i, "mute"))->load() > 0.5f) continue;
        const int len = (int) proc.apvts.getRawParameterValue (pid::inst (i, "len"))->load();
        for (int s = 0; s < len; ++s) if (proc.getStep (i, s)) { active.push_back (i); break; }
    }

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (cx - R - 4, cy - R - 2, (R + 4) * 2, (R + 4) * 2);
    const float inner = 44.f;
    const float gap = active.empty() ? 0.f : (R - inner) / (float) active.size();

    int64_t cycle = 1;
    for (size_t k = 0; k < active.size(); ++k)
    {
        const int i = active[k];
        const int len = juce::jmax (1, (int) proc.apvts.getRawParameterValue (pid::inst (i, "len"))->load());
        cycle = std::lcm (cycle, (int64_t) len);
        const float rr = R - gap * (float) k - gap * 0.5f;
        const auto ringCol = i == KICK ? C::paper : C::tube;
        g.setColour (C::steel.withAlpha (0.5f));
        g.drawEllipse (cx - rr, cy - rr, rr * 2, rr * 2, 0.8f);
        const int pos = running ? (int) (step % len) : -1;
        const float dotR = juce::jlimit (1.4f, 3.4f, gap * 0.28f);
        for (int s = 0; s < len; ++s)
        {
            const float a = -juce::MathConstants<float>::halfPi + (float) s / len * juce::MathConstants<float>::twoPi;
            const float x = cx + std::cos (a) * rr, y = cy + std::sin (a) * rr;
            const int v = proc.getStep (i, s);
            if (s == pos)
            {
                glowDot (g, x, y, dotR * 1.4f, juce::Colours::white, 1.f);
                g.setColour (juce::Colours::white);
                g.fillEllipse (x - dotR * 1.3f, y - dotR * 1.3f, dotR * 2.6f, dotR * 2.6f);
            }
            else if (v)
            {
                g.setColour (ringCol.withAlpha (0.35f + 0.65f * v / 127.f));
                g.fillEllipse (x - dotR, y - dotR, dotR * 2, dotR * 2);
            }
            else
            {
                g.setColour (C::steel.withAlpha (0.6f));
                g.fillEllipse (x - 0.9f, y - 0.9f, 1.8f, 1.8f);
            }
        }
    }

    // the hunt for the one
    g.setColour (C::night); g.fillEllipse (cx - inner + 4, cy - inner + 4, (inner - 4) * 2, (inner - 4) * 2);
    g.setColour (C::steel); g.drawEllipse (cx - inner + 4, cy - inner + 4, (inner - 4) * 2, (inner - 4) * 2, 1.f);
    const int64_t inCycle = running ? step % cycle : 0;
    const int64_t barsLeft = (cycle - inCycle + 15) / 16;
    if (running && inCycle < 16)
    {
        glowDot (g, cx, cy, 16.f, C::tube, 1.f);
        g.setFont (stencil (17.f)); g.setColour (C::tube);
        g.drawText (together, juce::Rectangle<float> (cx - 40, cy - 11, 80, 22), juce::Justification::centred, false);
    }
    else
    {
        g.setFont (stencil (8.5f)); g.setColour (C::paperDim);
        g.drawText (oneIn, juce::Rectangle<float> (cx - 40, cy - 25, 80, 12), juce::Justification::centred, false);
        g.setFont (stencil (barsLeft > 999 ? 14.f : 19.f)); g.setColour (C::tube);
        g.drawText (juce::String (barsLeft), juce::Rectangle<float> (cx - 40, cy - 12, 80, 24), juce::Justification::centred, false);
        g.setFont (stencil (8.5f)); g.setColour (C::paperDim);
        g.drawText (bars, juce::Rectangle<float> (cx - 40, cy + 13, 80, 12), juce::Justification::centred, false);
    }
}

// =====================================================================
//  StepGrid
// =====================================================================
void StepGrid::paint (juce::Graphics& g)
{
    const int len = (int) proc.apvts.getRawParameterValue (pid::inst (lane, "len"))->load();
    const int pat = proc.viewPattern();
    const bool kick = lane == KICK;
    const float h = (float) getHeight();
    for (int s = 0; s < kMaxSteps; ++s)
    {
        auto r = juce::Rectangle<float> ((float) (s * kCell), 1.f, (float) kCell - 2.f, h - 2.f);
        if (s >= len)
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r, 2.f);
            continue;
        }
        const bool beat = (s / 4) % 2 == 0;
        g.setColour (juce::Colours::black.withAlpha (0.6f)); g.fillRoundedRectangle (r.translated (0.f, 1.f), 2.f);
        g.setColour (beat ? C::iron2 : C::night); g.fillRoundedRectangle (r, 2.f);
        const auto st = proc.getCell (pat, lane, s);
        const int v = st.vel;
        const auto col = kick ? C::paper : C::tube;
        if (v && mode == kSlag)
        {
            const float a = 0.3f + 0.7f * v / 127.f;
            g.setColour (col.withAlpha (0.18f * a)); g.fillRoundedRectangle (r.expanded (1.5f), 3.f);
            g.setColour (col.withAlpha (a)); g.fillRoundedRectangle (r.reduced (1.5f), 1.5f);
            const float vh = (r.getHeight() - 6.f) * v / 127.f;               // velocity bar
            g.setColour (C::night.withAlpha (0.55f));
            g.fillRect (r.getX() + 3.f, r.getBottom() - 3.f - vh, 2.f, vh);
            if (v > 110) { g.setColour (juce::Colours::white.withAlpha (0.5f)); g.fillRect (r.getX() + 3.f, r.getY() + 3.f, r.getWidth() - 6.f, 1.5f); }
            // the extras, as small marks so the grid still reads at a glance
            for (int k = 1; k < st.ratchet; ++k)                               // RULLE: dots on the right
            { g.setColour (C::night); g.fillEllipse (r.getRight() - 5.f, r.getY() + 3.f + (float) (k - 1) * 5.f, 3.f, 3.f); }
            if (st.chance < 100 || st.chance >= kChanceFill)                   // CHANS: hazard notch
            {
                juce::Path tri; tri.addTriangle (r.getX() + 1.5f, r.getBottom() - 1.5f, r.getX() + 7.f, r.getBottom() - 1.5f, r.getX() + 1.5f, r.getBottom() - 7.f);
                g.setColour (st.chance >= kChanceFill ? C::alarm : C::hazard); g.fillPath (tri);
            }
            if (st.hasLocks()) { g.setColour (C::night); g.fillRect (r.getX() + 2.f, r.getY() + 2.f, 4.f, 4.f); }   // LÅS: dark square
        }
        else if (v)
        {
            // the step exists: dim body, the edited property drawn on top
            g.setColour (col.withAlpha (0.16f)); g.fillRoundedRectangle (r.reduced (1.5f), 1.5f);
            const auto inner = r.reduced (2.5f);
            if (mode == kChans)
            {
                if (st.chance >= kChanceFill)
                {
                    g.setColour (st.chance == kChanceFill ? C::hazard : C::alarm); g.fillRoundedRectangle (inner, 1.5f);
                    g.setFont (stencil (8.f)); g.setColour (C::night);
                    g.drawText (st.chance == kChanceFill ? "F" : "!F", inner, juce::Justification::centred, false);
                }
                else
                {
                    const float fh = inner.getHeight() * st.chance / 100.f;
                    g.setColour (C::hazard.withAlpha (0.9f)); g.fillRect (inner.withTrimmedTop (inner.getHeight() - fh));
                }
            }
            else if (mode == kRulle)
            {
                const float w = inner.getWidth() / (float) st.ratchet;
                for (int k = 0; k < st.ratchet; ++k)
                {
                    g.setColour (col.withAlpha (k == 0 ? 1.f : 0.75f));
                    g.fillRect (inner.getX() + k * w + 0.5f, inner.getY(), std::max (1.f, w - 1.f), inner.getHeight());
                }
            }
            else   // TUNE / DECAY locks: bipolar bar from the middle
            {
                const int lock = mode == kTune ? st.tune : st.decay;
                const float mid = inner.getCentreY(), hh = inner.getHeight() * 0.5f * std::abs (lock) / (float) kLockRange;
                g.setColour (C::steel); g.fillRect (inner.getX(), mid - 0.5f, inner.getWidth(), 1.f);
                g.setColour (lock > 0 ? C::tube : C::alarm);
                if (lock > 0) g.fillRect (inner.getX(), mid - hh, inner.getWidth(), hh);
                else if (lock < 0) g.fillRect (inner.getX(), mid, inner.getWidth(), hh);
            }
        }
        if (s == playhead)
        {
            g.setColour (juce::Colours::white.withAlpha (0.85f)); g.drawRoundedRectangle (r, 2.f, 1.4f);
            g.setColour (juce::Colours::white.withAlpha (0.12f)); g.fillRoundedRectangle (r, 2.f);
        }
    }
}

void StepGrid::mouseDown (const juce::MouseEvent& e)
{
    const int c = cellAt (e.position.x);
    const int len = (int) proc.apvts.getRawParameterValue (pid::inst (lane, "len"))->load();
    dragCell = -1;
    if (c >= len) return;
    const int pat = proc.viewPattern();
    auto st = proc.getCell (pat, lane, c);
    dragStartY = e.position.y;

    if (mode == kSlag)
    {
        const int v = st.vel;
        dragCell = c; dragStartVel = v;
        if (e.mods.isPopupMenu())                // right click: accent / soft / off
        {
            proc.setStep (lane, c, (uint8_t) (v == 0 ? 127 : v > 110 ? 60 : 0));
            dragCell = -1;
        }
        else if (v == 0) { proc.setStep (lane, c, 100); paintOn = true; dragStartVel = 100; }
        else paintOn = false;
    }
    else if (st.vel > 0)                         // the other modes edit existing hits only
    {
        dragCell = c;
        if (mode == kChans)
        {
            if (e.mods.isPopupMenu())            // 100 % -> only with FILL -> only without FILL -> 100 %
            {
                st.chance = st.chance == kChanceFill ? kChanceNoFill : st.chance == kChanceNoFill ? 100 : kChanceFill;
                proc.setCell (pat, lane, c, st); dragCell = -1;
            }
            dragStartVel = st.chance >= kChanceFill ? 100 : st.chance;
        }
        else if (mode == kRulle)
        {
            st.ratchet = e.mods.isPopupMenu() ? 1 : st.ratchet % 4 + 1;       // 1 -> 2 -> 3 -> 4 -> 1
            proc.setCell (pat, lane, c, st); dragCell = -1;
        }
        else
        {
            int& lock = mode == kTune ? st.tune : st.decay;
            if (e.mods.isPopupMenu() || e.getNumberOfClicks() > 1) { lock = 0; proc.setCell (pat, lane, c, st); dragCell = -1; }
            dragStartVel = lock;
        }
    }
    repaint();
}

void StepGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (dragCell < 0) return;
    const int pat = proc.viewPattern();
    const float dy = dragStartY - e.position.y;
    if (mode == kSlag)
    {
        if (paintOn)
        {   // drag across to draw more hits
            const int c = cellAt (e.position.x);
            const int len = (int) proc.apvts.getRawParameterValue (pid::inst (lane, "len"))->load();
            if (c < len && proc.getStep (lane, c) == 0) proc.setStep (lane, c, 100);
            // ... and up/down to set the velocity of the first one
            if (std::abs (dy) > 3.f)
                proc.setStep (lane, dragCell, (uint8_t) juce::jlimit (1, 127, dragStartVel + (int) (dy * 2.f)));
        }
        else if (std::abs (e.getDistanceFromDragStart()) > 3)
            proc.setStep (lane, dragCell, (uint8_t) juce::jlimit (1, 127, dragStartVel + (int) (dy * 2.f)));
    }
    else if (mode == kChans)
    {
        auto st = proc.getCell (pat, lane, dragCell);
        st.chance = juce::jlimit (0, 100, dragStartVel + (int) (dy * 1.5f));
        proc.setCell (pat, lane, dragCell, st);
    }
    else if (mode == kTune || mode == kDecay)
    {
        auto st = proc.getCell (pat, lane, dragCell);
        (mode == kTune ? st.tune : st.decay) = juce::jlimit (-kLockRange, kLockRange, dragStartVel + (int) (dy / 2.5f));
        proc.setCell (pat, lane, dragCell, st);
    }
    repaint();
}

void StepGrid::mouseUp (const juce::MouseEvent& e)
{
    // in SLAG mode a plain click on a lit step turns it off
    if (mode == kSlag && dragCell >= 0 && ! paintOn && e.getDistanceFromDragStart() <= 3)
        proc.setStep (lane, dragCell, 0);
    dragCell = -1;
    repaint();
}

// =====================================================================
//  LaneRow
// =====================================================================
LaneRow::LaneRow (MotstandProcessor& p, int i, const int& editMode) : grid (p, i, editMode), proc (p), inst (i)
{
    addAndMakeVisible (grid);
    addAndMakeVisible (dice);
    dice.onClick = [this] { randomise(); };

    len.setSliderStyle (juce::Slider::LinearBarVertical);
    len.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 40, 20);
    len.onValueChange = [this] { grid.repaint(); };
    addAndMakeVisible (len);
    sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::inst (i, "len"), len));

    const char* vIds[4] = { "tune", "decay", "tone", "level" };
    for (int k = 0; k < 4; ++k)
    {
        voice[(size_t) k].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        voice[(size_t) k].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        voice[(size_t) k].setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (voice[(size_t) k]);
        sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::inst (i, vIds[k]), voice[(size_t) k]));
    }
    for (int m = 0; m < kNumSlipModes; ++m) mode.addItem (kSlipModeName[m], m + 1);
    addAndMakeVisible (mode);
    mAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, pid::inst (i, "slipMode"), mode);

    const char* sIds[3] = { "slipDrive", "slipFeed", "slipMix" };
    for (int k = 0; k < 3; ++k)
    {
        slipK[(size_t) k].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slipK[(size_t) k].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slipK[(size_t) k].setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (slipK[(size_t) k]);
        sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::inst (i, sIds[k]), slipK[(size_t) k]));
    }
    const char* fxIds[2] = { "sendHall", "sendEko" };
    for (int k = 0; k < 2; ++k)
    {
        sends[(size_t) k].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        sends[(size_t) k].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        sends[(size_t) k].setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (sends[(size_t) k]);
        sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::inst (i, fxIds[k]), sends[(size_t) k]));
    }
    mute.setColour (juce::ToggleButton::tickColourId, C::alarm);
    addAndMakeVisible (mute);
    muteAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, pid::inst (i, "mute"), mute);
    mute.onClick = [this] { repaint(); };
}

void LaneRow::poll()
{
    const int m = (int) proc.apvts.getRawParameterValue (pid::inst (inst, "model"))->load();
    if (m != lastModel) { lastModel = m; repaint(); }
}

void LaneRow::mouseDown (const juce::MouseEvent& e)
{
    if (e.x < xName || e.x > xName + 92) return;
    if (MotstandProcessor::isDemo) return;   // DEMO: the sound models are in the full version
    // click the Dymo name: pick one of the four sound models
    const int cur = (int) proc.apvts.getRawParameterValue (pid::inst (inst, "model"))->load();
    juce::PopupMenu m;
    m.addSectionHeader (lang.t ("LJUD FÖR ", "SOUND FOR ") + name);
    for (int k = 0; k < kNumModels; ++k)
        m.addItem (k + 1, u (lang.en ? kModelEN[inst][k] : kModelSV[inst][k]), true, k == cur);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (140),
        [this] (int r)
        {
            if (r <= 0) return;
            if (auto* p = proc.apvts.getParameter (pid::inst (inst, "model")))
            { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 ((float) (r - 1))); p->endChangeGesture(); }
            poll();
        });
}

void LaneRow::lockForDemo (std::vector<juce::Component*>& locked)
{
    locked.push_back (&dice); locked.push_back (&len); locked.push_back (&mode);
    for (int k = 0; k < 3; ++k) locked.push_back (&voice[(size_t) k]);     // NIVÅ stays open
    for (auto& k : slipK) locked.push_back (&k);
    for (auto& k : sends) locked.push_back (&k);
}

void LaneRow::setLang (const Lang& l)
{
    lang = l;
    name = u (l.en ? kInstNameEN[inst] : kInstNameSV[inst]);
    mute.setButtonText (l.t ("TYST", "MUTE"));
    // JUCE forgets the selection when the shown text no longer matches, so restore it by id
    const int sel = juce::jmax (1, 1 + (int) proc.apvts.getRawParameterValue (pid::inst (inst, "slipMode"))->load());
    for (int m = 0; m < kNumSlipModes; ++m) mode.changeItemText (m + 1, l.en ? kSlipModeEN[m] : kSlipModeName[m]);
    mode.setSelectedId (sel, juce::dontSendNotification);
    dice.setTooltip (l.t ("Slumpa mönstret för ", "Randomise the pattern for ") + name);
    setTooltip (l.t ("Klicka på namnet för att byta ljud", "Click the name to change the sound"));
    grid.setTooltip (l.t ("Klicka = slag. Dra upp/ner = styrka. Högerklick = accent/mjuk/av.",
                          "Click = hit. Drag up/down = velocity. Right click = accent/soft/off."));
    len.setTooltip (l.t ("Loopens längd i steg (1-32)", "Loop length in steps (1-32)"));
    mode.setTooltip (l.t ("VINKELSLIP-läge", "VINKELSLIP mode"));
    sends[0].setTooltip (l.t ("Skicka till VARVSHALL", "Send to VARVSHALL (reverb)"));
    sends[1].setTooltip (l.t ("Skicka till EKO", "Send to EKO (tape echo)"));
    repaint();
}

void LaneRow::resized()
{
    const int h = getHeight(), k = knobW;
    dice.setBounds (xDice, (h - 22) / 2, 22, 22);
    grid.setBounds (xGrid, 6, StepGrid::kCell * kMaxSteps, h - 12);
    len.setBounds (xLen, 9, 40, h - 18);
    for (int i = 0; i < 4; ++i) voice[(size_t) i].setBounds (xKnobs + i * knobStep, 3, k, h - 6);
    mode.setBounds (xMode, (h - 22) / 2, 86, 22);
    for (int i = 0; i < 2; ++i) sends[(size_t) i].setBounds (xSend + i * knobStep, 3, k, h - 6);
    for (int i = 0; i < 3; ++i) slipK[(size_t) i].setBounds (xSlip + i * slipStep, 3, k, h - 6);
    mute.setBounds (xMute, (h - 22) / 2, 44, 22);
}

void LaneRow::paint (juce::Graphics& g)
{
    const bool muted = mute.getToggleState();
    if (inst % 2 == 0) { g.setColour (juce::Colours::white.withAlpha (0.025f)); g.fillRect (getLocalBounds()); }
    const int m = juce::jlimit (0, kNumModels - 1, (int) proc.apvts.getRawParameterValue (pid::inst (inst, "model"))->load());
    auto tag = juce::Rectangle<float> ((float) xName, 7.f, 92.f, (float) getHeight() - 14.f);
    dymo (g, tag.withTrimmedBottom (m > 0 ? 9.f : 2.f), name, muted ? juce::Colour (0xff3a1610) : juce::Colour (0xff141414));
    if (m > 0)
    {   // the chosen sound model, stencilled under the tape
        g.setFont (stencil (8.5f)); g.setColour (C::tube.withAlpha (0.9f));
        g.drawText (u (lang.en ? kModelEN[inst][m] : kModelSV[inst][m]), tag.removeFromBottom (10.f), juce::Justification::centred, false);
    }
    // VINKELSLIP well
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillRoundedRectangle ((float) xMode - 6.f, 3.f, (float) (xSend - xMode) - 4.f, (float) getHeight() - 6.f, 4.f);
    // send well
    g.setColour (C::tubeDeep.withAlpha (0.35f));
    g.fillRoundedRectangle ((float) xSend - 5.f, 3.f, (float) (2 * knobStep) + 4.f, (float) getHeight() - 6.f, 4.f);
}

void LaneRow::randomise()
{
    juce::Random r;
    std::array<uint8_t, kMaxSteps> s {};
    int len = 16;
    auto vel = [&] (int lo, int hi) { return (uint8_t) (lo + r.nextInt (hi - lo + 1)); };
    switch (inst)
    {
        case KICK:
            for (int i = 0; i < 16; i += 4) s[(size_t) i] = vel (112, 124);
            if (r.nextFloat() < 0.35f) s[(size_t) (r.nextBool() ? 14 : 10)] = vel (70, 95);
            if (r.nextFloat() < 0.2f) s[3] = vel (60, 80);
            break;
        case SNARE:
            for (int n = r.nextInt (3); n-- > 0;) s[(size_t) r.nextInt (16)] = vel (60, 110);
            break;
        case CLAP:
            s[4] = vel (95, 115); s[12] = vel (95, 115);
            if (r.nextFloat() < 0.3f) s[(size_t) (r.nextBool() ? 7 : 15)] = vel (60, 85);
            break;
        case CH:
            for (int i = 0; i < 16; ++i)
                s[(size_t) i] = (i % 4 == 2) ? vel (95, 115) : (r.nextFloat() < 0.85f ? vel (38, 70) : 0);
            break;
        case OH:
            for (int i = 2; i < 16; i += 4) s[(size_t) i] = vel (90, 112);
            if (r.nextFloat() < 0.25f) s[(size_t) (r.nextInt (8) * 2 + 1)] = vel (60, 80);
            break;
        case RIDE:
            len = r.nextFloat() < 0.25f ? 12 : 16;
            for (int i = 0; i < len; i += 2) s[(size_t) i] = vel (60, 100);
            break;
        default:   // toms and rim: polymeter
        {
            const int lens[] = { 5, 7, 9, 11, 13 };
            len = lens[r.nextInt (5)];
            const int hits = 1 + r.nextInt (3);
            for (int n = 0; n < hits; ++n) s[(size_t) r.nextInt (len)] = vel (65, 115);
            break;
        }
    }
    for (int i = 0; i < kMaxSteps; ++i) proc.setStep (inst, i, s[(size_t) i]);
    if (auto* p = proc.apvts.getParameter (pid::inst (inst, "len")))
    { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 ((float) len)); p->endChangeGesture(); }
    grid.repaint();
}

// =====================================================================
//  Root
// =====================================================================
Root::Root (MotstandProcessor& p)
    : proc (p), jog (*p.apvts.getParameter (pid::nudge)), rings (p)
{
    wall = makeRustWall (W, H);

    // ---- top bar ----
    presetBox.addItem ("FABRIK", 1);
    {
        juce::String lastCat;
        for (int i = 0; i < (int) styles().size(); ++i)
        {
            if (! MotstandProcessor::presetAllowed (i + 1)) continue;   // DEMO: only the demo presets
            const auto cat = u (styles()[(size_t) i].cat);
            if (cat != lastCat) { presetBox.addSectionHeading (cat); lastCat = cat; }
            presetBox.addItem (u (styles()[(size_t) i].name), i + 2);
        }
    }
    presetBox.onChange = [this] { pickPreset (presetBox.getSelectedId() - 1); };
    prevPreset.setButtonText ("<"); nextPreset.setButtonText (">");
    prevPreset.onClick = [this] { stepPreset (-1); };
    nextPreset.onClick = [this] { stepPreset (1); };
    for (auto* c : std::initializer_list<juce::Component*> { &presetBox, &prevPreset, &nextPreset }) addAndMakeVisible (*c);
    addAndMakeVisible (langBtn);
    // SV | EN is shared by every MASKINHALLEN machine on the computer
    langBtn.onClick = [this] { sprak::sattEngelska (! lang.en); setLang (sprak::engelska()); };
    romBox.addItem ("REN", 1); romBox.addItem ("909 6-BIT", 2);
    addAndMakeVisible (romBox);
    romAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, pid::romMode, romBox);
    midiBtn.setColour (juce::ToggleButton::tickColourId, C::tube);
    addAndMakeVisible (midiBtn);
    midiAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, pid::midiOut, midiBtn);
    seed.setSliderStyle (juce::Slider::LinearBarVertical);
    seed.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 20);
    addAndMakeVisible (seed);
    sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::seed, seed));
    addAndMakeVisible (seedDice);
    seedDice.onClick = [this] { setParam (pid::seed, (float) (1 + juce::Random().nextInt (9999))); };

    // ---- DIE ZEIT ----
    addAndMakeVisible (strobe);
    addAndMakeVisible (jog);
    for (auto* k : { &jitter, &drift, &fylla, &bpm }) addAndMakeVisible (*k);
    sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::jitter, jitter.slider));
    sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::drift, drift.slider));
    sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::ride, fylla.slider));
    sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::bpm, bpm.slider));
    ownTempo.setColour (juce::ToggleButton::tickColourId, C::hazard);
    addAndMakeVisible (ownTempo);
    tempoAtt = std::make_unique<APVTS::ButtonAttachment> (p.apvts, pid::tempoSrc, ownTempo);
    addAndMakeVisible (zeitDice);
    zeitDice.onClick = [this] { randomiseZeit(); };

    // ---- rings ----
    addAndMakeVisible (rings);

    // ---- DETROIT SMOG ----
    addAndMakeVisible (meter);
    for (auto* k : { &smogAmt, &smogThr, &elnat, &master }) addAndMakeVisible (*k);
    sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::smog, smogAmt.slider));
    sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::smogThr, smogThr.slider));
    sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::elnat, elnat.slider));
    sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, pid::master, master.slider));
    addAndMakeVisible (smogDice);
    smogDice.onClick = [this] { randomiseSmog(); };

    // ---- studio effects ----
    auto knob = [&] (LabeledKnob& k, const juce::String& id) { addAndMakeVisible (k); sAtt.push_back (std::make_unique<APVTS::SliderAttachment> (p.apvts, id, k.slider)); };
    auto lamp = [&] (juce::ToggleButton& b, const juce::String& id, juce::Colour c) {
        b.setColour (juce::ToggleButton::tickColourId, c); addAndMakeVisible (b);
        bAtt.push_back (std::make_unique<APVTS::ButtonAttachment> (p.apvts, id, b)); };
    knob (hSize, pid::hallSize); knob (hDecay, pid::hallDecay); knob (hDamp, pid::hallDamp);
    knob (hPre, pid::hallPre); knob (hLevel, pid::hallLevel);
    lamp (hFreeze, pid::hallFreeze, C::hazard);
    for (auto* t : { "1/16", "1/8", "3/16", "1/4", "3/8", "1/2" }) eTime.addItem (t, eTime.getNumItems() + 1);
    addAndMakeVisible (eTime);
    eTimeAtt = std::make_unique<APVTS::ComboBoxAttachment> (p.apvts, pid::ekoTime, eTime);
    knob (eFb, pid::ekoFeedback); knob (eTone, pid::ekoTone); knob (eWear, pid::ekoWear); knob (eLevel, pid::ekoLevel);
    lamp (ePing, pid::ekoPing, C::tube);
    knob (iLow, pid::isoLow); knob (iMid, pid::isoMid); knob (iHigh, pid::isoHigh);
    lamp (kLow, pid::killLow, C::alarm); lamp (kMid, pid::killMid, C::alarm); lamp (kHigh, pid::killHigh, C::alarm);
    kLow.setButtonText ("KILL"); kMid.setButtonText ("KILL"); kHigh.setButtonText ("KILL");
    knob (pAmt, pid::pressAmt); knob (pPump, pid::pump);
    addAndMakeVisible (gr);
    addAndMakeVisible (fxDice);
    fxDice.onClick = [this] { randomiseFx(); };

    // ---- UNDERGÅNG page ----
    knob (mAmt, pid::mullerAmt); knob (mDepth, pid::mullerDepth); knob (mFreq, pid::mullerFreq);
    knob (mDirt, pid::mullerDirt); knob (mPump, pid::mullerPump);
    knob (oGas, pid::motorGas); knob (oRpm, pid::motorRpm); knob (oTone, pid::motorTone); knob (oRattle, pid::motorRattle);
    knob (sAmt, pid::stormAmt); knob (sSweep, pid::stormSweep); knob (sColor, pid::stormColor); knob (sAlarm, pid::alarm);
    knob (doomKnob, pid::doom);
    doomKnob.slider.getProperties().set ("doom", true);
    addAndMakeVisible (doomDice);
    doomDice.onClick = [this] { randomiseDoom(); };
    for (auto* t : { &tabStudio, &tabDoom })
    {
        t->setClickingTogglesState (true); t->setRadioGroupId (8008); addAndMakeVisible (*t);
    }
    tabStudio.onClick = [this] { setFxPage (0); };
    tabDoom.onClick   = [this] { setFxPage (1); };
    setFxPage (0);

    // ---- patterns A..H, copy/paste/clear, FILL, edit modes ----
    for (int i = 0; i < kNumPatterns; ++i)
    {
        patBtns.push_back (std::make_unique<PatternButton> (i));
        addAndMakeVisible (*patBtns.back());
        patBtns.back()->onClick = [this, i] { setParam (pid::pattern, (float) i); };
    }
    for (auto* b : { &copyBtn, &pasteBtn, &clearBtn, &fillBtn }) addAndMakeVisible (*b);
    copyBtn.onClick = [this] { clipboard = std::make_unique<PatternData> (proc.copyPattern (proc.viewPattern())); };
    pasteBtn.onClick = [this] { if (clipboard) { proc.pastePattern (proc.viewPattern(), *clipboard); for (auto& l : lanes) l->grid.repaint(); } };
    clearBtn.onClick = [this]
    {
        clipboard = std::make_unique<PatternData> (proc.copyPattern (proc.viewPattern()));   // KLISTRA undoes it
        PatternData empty {}; for (auto& l : empty) l.fill (Step::rest());
        proc.pastePattern (proc.viewPattern(), empty);
        for (auto& l : lanes) l->grid.repaint();
    };
    fillBtn.onStateChange = [this]
    {   // momentary: FILL plays only while the button is held
        const bool d = fillBtn.isDown();
        if (d != fillDown) { fillDown = d; setParam (pid::fill, d ? 1.f : 0.f); }
    };
    for (int k = 0; k < kNumEditModes; ++k)
    {
        modeBtns[(size_t) k].setClickingTogglesState (true);
        modeBtns[(size_t) k].setRadioGroupId (4711);
        modeBtns[(size_t) k].onClick = [this, k] { setEditMode (k); };
        addAndMakeVisible (modeBtns[(size_t) k]);
    }
    modeBtns[0].setToggleState (true, juce::dontSendNotification);

    // ---- lanes ----
    for (int i = 0; i < kNumInstruments; ++i)
    {
        lanes.push_back (std::make_unique<LaneRow> (p, i, editMode));
        addAndMakeVisible (*lanes.back());
    }
    addAndMakeVisible (slipDice);
    slipDice.onClick = [this] { randomiseSlip(); };

    if (MotstandProcessor::isDemo) setupDemo();
    sprak::laddaOm();   // the choice may have been made in another machine
    setLang (sprak::engelska());
}

void Root::stepPreset (int dir)
{
    if (MotstandProcessor::isDemo)
    {
        const auto& v = MotstandProcessor::demoPresets();
        const int n = (int) v.size();
        const auto it = std::find (v.begin(), v.end(), proc.currentPreset);
        const int pos = it == v.end() ? 0 : (int) (it - v.begin());
        pickPreset (v[(size_t) ((pos + dir + n) % n)]);
        return;
    }
    pickPreset ((proc.currentPreset + dir + numPresets()) % numPresets());
}

// ---------------------------------------------------------------- demo
void Root::setupDemo()
{
    std::vector<juce::Component*> l { &jitter, &drift, &fylla, &bpm, &ownTempo, &zeitDice,
                                      &smogAmt, &smogThr, &elnat, &smogDice,
                                      &hSize, &hDecay, &hDamp, &hPre, &hLevel, &hFreeze, &eTime, &ePing, &eFb, &eTone, &eWear, &eLevel,
                                      &iLow, &iMid, &iHigh, &kLow, &kMid, &kHigh, &pAmt, &pPump, &fxDice,
                                      &mAmt, &mDepth, &mFreq, &mDirt, &mPump, &oGas, &oRpm, &oTone, &oRattle,
                                      &sAmt, &sSweep, &sColor, &sAlarm, &doomDice,
                                      &seed, &seedDice, &slipDice };
    for (int k = 1; k < kNumEditModes; ++k) l.push_back (&modeBtns[(size_t) k]);   // SLAG stays open
    for (auto& lane : lanes) lane->lockForDemo (l);
    demoLocked = l;
    for (auto* c : demoLocked) { c->setEnabled (false); c->setAlpha (0.35f); }
    romBox.setVisible (false);
    midiBtn.setVisible (false);
    buyBtn.setColour (juce::TextButton::buttonColourId, C::hazard);
    buyBtn.onClick = [] { juce::URL (licens::kKopUrl).launchInDefaultBrowser(); };
    addAndMakeVisible (buyBtn);
}

void Root::paintOverChildren (juce::Graphics& g)
{
    if (! MotstandProcessor::isDemo) return;
    // a small padlock on everything that is closed in the demo
    auto visibleHere = [this] (juce::Component* c)
    {
        for (auto* p = c; p != nullptr && p != this; p = p->getParentComponent())
            if (! p->isVisible()) return false;
        return true;
    };
    for (auto* c : demoLocked)
    {
        if (c->getWidth() < 12 || ! visibleHere (c)) continue;
        const auto b = getLocalArea (c->getParentComponent(), c->getBounds()).toFloat();
        const float w = 9.f, x = b.getRight() - w - 1.f, y = b.getY() + 1.f;
        g.setColour (C::night.withAlpha (0.9f));
        g.fillRoundedRectangle (x - 1.5f, y + 3.f, w + 3.f, 9.5f, 2.f);
        g.setColour (C::hazard);
        g.fillRoundedRectangle (x, y + 5.f, w, 6.f, 1.2f);
        juce::Path shackle;
        shackle.addCentredArc (x + w * 0.5f, y + 5.f, 2.8f, 3.2f, 0.f, -juce::MathConstants<float>::halfPi, juce::MathConstants<float>::halfPi, true);
        g.strokePath (shackle, juce::PathStrokeType (1.4f));
    }
}

void Root::setParam (const juce::String& id, float v)
{
    if (auto* p = proc.apvts.getParameter (id))
    { p->beginChangeGesture(); p->setValueNotifyingHost (p->convertTo0to1 (v)); p->endChangeGesture(); }
}

void Root::randomiseZeit()
{
    juce::Random r;
    setParam (pid::jitter, 0.3f + r.nextFloat() * 1.2f);
    setParam (pid::drift, 0.1f + r.nextFloat() * 0.7f);
    setParam (pid::ride, 0.2f + r.nextFloat() * 0.8f);
}

void Root::randomiseSmog()
{
    juce::Random r;
    setParam (pid::smog, 0.1f + r.nextFloat() * 0.6f);
    setParam (pid::smogThr, 2.5f + r.nextFloat() * 2.f);
}

void Root::pickPreset (int index)
{
    if (index < 0) return;
    proc.loadPreset (index);
    shownPreset = index;
    presetBox.setSelectedId (index + 1, juce::dontSendNotification);
    for (auto& l : lanes) { l->poll(); l->grid.repaint(); }
    setLang (lang.en);   // refresh combo texts (slip modes etc.) after the jump
}

void Root::setEditMode (int m)
{
    editMode = m;
    for (int k = 0; k < kNumEditModes; ++k) modeBtns[(size_t) k].setToggleState (k == m, juce::dontSendNotification);
    const juce::String tip[kNumEditModes] = {
        lang.t ("Klicka = slag. Dra upp/ner = styrka. Högerklick = accent/mjuk/av.", "Click = hit. Drag up/down = velocity. Right click = accent/soft/off."),
        lang.t ("Dra upp/ner = chans i %. Högerklick = bara med FILL / bara utan FILL.", "Drag up/down = chance in %. Right click = only with FILL / only without FILL."),
        lang.t ("Klicka = 1, 2, 3 eller 4 slag i steget. Högerklick = av.", "Click = 1, 2, 3 or 4 hits in the step. Right click = off."),
        lang.t ("Dra upp/ner = egen tune för steget. Dubbelklick = nollställ.", "Drag up/down = this step's own tune. Double click = reset."),
        lang.t ("Dra upp/ner = egen decay för steget. Dubbelklick = nollställ.", "Drag up/down = this step's own decay. Double click = reset.") };
    for (auto& l : lanes) { l->grid.setTooltip (tip[m]); l->grid.repaint(); }
}

void Root::setFxPage (int p)
{
    fxPage = p;
    tabStudio.setToggleState (p == 0, juce::dontSendNotification);
    tabDoom.setToggleState (p == 1, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*> { &hSize, &hDecay, &hDamp, &hPre, &hLevel, &hFreeze, &eTime, &ePing,
                                                             &eFb, &eTone, &eWear, &eLevel, &iLow, &iMid, &iHigh, &kLow, &kMid, &kHigh,
                                                             &pAmt, &pPump, &gr, &fxDice })
        c->setVisible (p == 0);
    for (auto* c : std::initializer_list<juce::Component*> { &mAmt, &mDepth, &mFreq, &mDirt, &mPump, &oGas, &oRpm, &oTone, &oRattle,
                                                             &sAmt, &sSweep, &sColor, &sAlarm, &doomKnob, &doomDice })
        c->setVisible (p == 1);
    repaint();
}

void Root::randomiseDoom()
{
    juce::Random r;
    setParam (pid::mullerAmt, 0.3f + r.nextFloat() * 0.6f);
    setParam (pid::mullerDepth, 3.f + r.nextFloat() * 8.f);
    setParam (pid::mullerFreq, 70.f + r.nextFloat() * 150.f);
    setParam (pid::mullerDirt, 0.2f + r.nextFloat() * 0.7f);
    setParam (pid::motorGas, r.nextFloat() < 0.5f ? r.nextFloat() * 0.6f : 0.f);
    setParam (pid::motorRpm, 700.f + r.nextFloat() * 3500.f);
    setParam (pid::motorTone, 80.f + r.nextFloat() * 200.f);
    setParam (pid::motorRattle, r.nextFloat());
    setParam (pid::stormAmt, r.nextFloat() * 0.6f);
    setParam (pid::stormSweep, r.nextFloat());
    setParam (pid::stormColor, 500.f + r.nextFloat() * 3500.f);
    setParam (pid::alarm, r.nextFloat() < 0.3f ? r.nextFloat() * 0.6f : 0.f);
}

void Root::randomiseFx()
{
    juce::Random r;
    setParam (pid::hallSize, 0.3f + r.nextFloat() * 0.7f);
    setParam (pid::hallDecay, 0.8f + r.nextFloat() * 5.f);
    setParam (pid::hallDamp, 0.2f + r.nextFloat() * 0.6f);
    setParam (pid::hallPre, r.nextFloat() * 60.f);
    setParam (pid::ekoTime, (float) r.nextInt (6));
    setParam (pid::ekoFeedback, 0.2f + r.nextFloat() * 0.5f);
    setParam (pid::ekoTone, 0.2f + r.nextFloat() * 0.6f);
    setParam (pid::ekoWear, r.nextFloat() * 0.7f);
    // tasteful sends: hats, ride, clap and rim get some space, kick stays dry
    for (int i = 0; i < kNumInstruments; ++i)
    {
        const bool wet = i == CLAP || i == RIM || i == OH || i == RIDE || (i >= TOM_LO && i <= TOM_HI);
        setParam (pid::inst (i, "sendHall"), wet ? r.nextFloat() * 0.45f : 0.f);
        setParam (pid::inst (i, "sendEko"), wet && r.nextFloat() < 0.4f ? r.nextFloat() * 0.4f : 0.f);
    }
}

void Root::randomiseSlip()
{
    // keeps it tasteful: about half the lanes get some grind, never full blast
    juce::Random r;
    for (int i = 0; i < kNumInstruments; ++i)
    {
        const bool on = r.nextFloat() < 0.5f;
        setParam (pid::inst (i, "slipMode"), (float) r.nextInt (kNumSlipModes));
        setParam (pid::inst (i, "slipDrive"), on ? 0.1f + r.nextFloat() * 0.45f : 0.f);
        setParam (pid::inst (i, "slipFeed"), r.nextFloat());
    }
}

void Root::setLang (bool english)
{
    lang.en = english;
    langBtn.setButtonText (english ? "EN" : "SV");
    if (MotstandProcessor::isDemo)
    {
        const auto tip = lang.t ("Finns i fullversionen", "In the full version");
        for (auto* c : demoLocked)
        {
            if (auto* k = dynamic_cast<LabeledKnob*> (c)) k->slider.setTooltip (tip);
            else if (auto* t = dynamic_cast<juce::SettableTooltipClient*> (c)) t->setTooltip (tip);
        }
        buyBtn.setButtonText (lang.t ("KÖP FULLVERSION", "BUY FULL VERSION"));
        buyBtn.setTooltip (lang.t ("Lås upp allt i MOTSTÅND: €20, eller alla maskiner i bundlen för €89",
                                   "Unlock everything in MOTSTÅND: €20, or every machine in the bundle for €89"));
        prevPreset.setTooltip (lang.t ("Förra demoläget", "Previous demo preset"));
        nextPreset.setTooltip (lang.t ("Nästa demoläge", "Next demo preset"));
    }
    if (onLang) onLang();
    presetBox.setTooltip (lang.t ("Presets: kit, mönster A-H, VINKELSLIP och effekter", "Presets: kit, patterns A-H, VINKELSLIP and effects"));
    prevPreset.setTooltip (lang.t ("Förra preset", "Previous preset")); nextPreset.setTooltip (lang.t ("Nästa preset", "Next preset"));
    langBtn.setTooltip (u ("Byt språk / Switch language"));
    const int romSel = 1 + (int) proc.apvts.getRawParameterValue (pid::romMode)->load();
    romBox.changeItemText (1, lang.t ("REN", "CLEAN"));
    romBox.setSelectedId (romSel, juce::dontSendNotification);
    romBox.setTooltip (lang.t ("Cymbalernas ROM: ren eller 909:ans 6-bitars korn", "Cymbal ROM: clean or the 909's 6-bit grit"));
    midiBtn.setButtonText (lang.t ("MIDI UT", "MIDI OUT"));
    midiBtn.setTooltip (lang.t ("Skicka varje steg som MIDI, med drift och jitter", "Send every step as MIDI, drift and jitter included"));
    seed.setTooltip (lang.t ("Seed: samma nummer = samma tagning", "Seed: same number = same take"));
    seedDice.setTooltip (lang.t ("Ny seed", "New seed"));

    jitter.text = "JITTER"; drift.text = "DRIFT";
    fylla.text = lang.t ("FYLLA", "DRUNK"); bpm.text = "BPM";
    ownTempo.setButtonText (lang.t ("EGET TEMPO", "OWN TEMPO"));
    ownTempo.setTooltip (lang.t ("Av = DAW:ens tempo. På = eget BPM.", "Off = DAW tempo. On = own BPM."));
    jog.label = "NUDGE";
    jog.setTooltip (lang.t ("Dra höger/vänster för att knuffa maskinen i fas", "Drag right/left to push the machine back in phase"));
    strobe.inPhase = lang.t ("I FAS", "IN PHASE"); strobe.ahead = lang.t ("FÖRE", "AHEAD"); strobe.behind = lang.t ("EFTER", "BEHIND");
    zeitDice.setTooltip (lang.t ("Slumpa klockans instabilitet", "Randomise the clock's instability"));

    rings.oneIn = lang.t ("ETTAN OM", "THE ONE IN"); rings.bars = lang.t ("TAKTER", "BARS"); rings.together = lang.t ("ETTAN!", "THE ONE!");

    smogAmt.text = lang.t ("MÄNGD", "AMOUNT"); smogThr.text = lang.t ("TRÖSKEL", "THRESHOLD");
    elnat.text = lang.t ("ELNÄT", "MAINS HUM"); master.text = "MASTER";
    meter.caption = lang.t ("KOLLAPS", "COLLAPSE");
    smogDice.setTooltip (lang.t ("Slumpa Detroit Smog", "Randomise Detroit Smog"));
    slipDice.setTooltip (lang.t ("Slumpa VINKELSLIP på alla kanaler", "Randomise VINKELSLIP on all channels"));

    hSize.text = lang.t ("STORLEK", "SIZE"); hDecay.text = lang.t ("SVANS", "TAIL");
    hDamp.text = lang.t ("MÖRKER", "DARK"); hPre.text = lang.t ("AVSTÅND", "PREDELAY"); hLevel.text = lang.t ("NIVÅ", "LEVEL");
    hDecay.slider.setTooltip (lang.t ("Efterklangstid (svansen)", "Reverb decay time"));
    hDamp.slider.setTooltip (lang.t ("Dämpning: hur mörk hallen låter", "Damping: how dark the hall sounds"));
    hPre.slider.setTooltip (lang.t ("Fördröjning innan hallen svarar", "Pre-delay before the hall answers"));
    hFreeze.setButtonText (lang.t ("FRYS", "FREEZE"));
    hFreeze.setTooltip (lang.t ("Frys hallen: den ringer för evigt", "Freeze the hall: it rings forever"));
    eFb.text = lang.t ("ÅTERKOPPL.", "FEEDBACK"); eTone.text = lang.t ("TON", "TONE");
    eWear.text = lang.t ("SLITAGE", "WEAR"); eLevel.text = lang.t ("NIVÅ", "LEVEL");
    ePing.setButtonText ("PINGPONG");
    eTime.setTooltip (lang.t ("Ekots tid, följer maskinens drivande tempo", "Echo time, follows the machine's drifting tempo"));
    iLow.text = lang.t ("LÅG", "LOW"); iMid.text = lang.t ("MITT", "MID"); iHigh.text = lang.t ("TOPP", "HIGH");
    iLow.slider.setTooltip (lang.t ("Isolator: startläget (80 %) är rakt. Ner = mindre, upp = lite mer, KILL = bandet helt bort", "Isolator: the start position (80 %) is flat. Down = less, up = a little more, KILL = the band is gone"));
    pAmt.text = "PRESS"; pPump.text = "PUMP";
    pAmt.slider.setTooltip (lang.t ("Bus-kompressor med automatisk nivå", "Bus compressor with automatic make-up"));
    pPump.slider.setTooltip (lang.t ("Kicken duckar allt annat", "The kick ducks everything else"));
    fxDice.setTooltip (lang.t ("Slumpa effekterna och sends", "Randomise the effects and sends"));
    for (auto* k : { &hSize, &hDecay, &hDamp, &hPre, &hLevel, &eFb, &eTone, &eWear, &eLevel, &iLow, &iMid, &iHigh, &pAmt, &pPump }) { k->resized(); k->repaint(); }

    tabStudio.setButtonText ("STUDIO"); tabDoom.setButtonText (lang.t ("UNDERGÅNG", "DOOM"));
    tabDoom.setTooltip (lang.t ("Mad Max och jordens undergång", "Mad Max and the end of the world"));
    mAmt.text = lang.t ("MÄNGD", "AMOUNT"); mDepth.text = lang.t ("DJUP", "DEPTH"); mFreq.text = lang.t ("FREKVENS", "FREQ");
    mDirt.text = lang.t ("SMUTS", "DIRT"); mPump.text = "PUMP";
    mAmt.slider.setTooltip (lang.t ("Kicken matas in i en mörk jättehall: hardtechno-muller", "The kick feeds a huge dark hall: hard techno rumble"));
    oGas.text = "GAS"; oRpm.text = lang.t ("VARV", "RPM"); oTone.text = lang.t ("KLANG", "TONE"); oRattle.text = lang.t ("RASSEL", "RATTLE");
    oGas.slider.setTooltip (lang.t ("En V8 med ojämn tändföljd. Vänster bank vänster, höger bank höger.", "A cross-plane V8. Left bank left, right bank right."));
    sAmt.text = lang.t ("SAND", "SAND"); sSweep.text = lang.t ("SVEP", "SWEEP"); sColor.text = lang.t ("FÄRG", "COLOUR"); sAlarm.text = lang.t ("LARM", "SIREN");
    sAlarm.slider.setTooltip (lang.t ("Flyglarm långt bort, i hallen", "An air-raid siren far away, in the hall"));
    doomKnob.text = lang.t ("UNDERGÅNG", "DOOM");
    doomKnob.slider.setTooltip (lang.t ("En ratt: Smog, Press, VINKELSLIP, muller, storm och pump på en gång", "One knob: Smog, Press, VINKELSLIP, rumble, storm and pump at once"));
    doomDice.setTooltip (lang.t ("Slumpa undergången", "Randomise the doom"));
    for (auto* k : { &mAmt, &mDepth, &mFreq, &mDirt, &mPump, &oGas, &oRpm, &oTone, &oRattle, &sAmt, &sSweep, &sColor, &sAlarm, &doomKnob }) { k->resized(); k->repaint(); }

    for (auto& l : lanes) l->setLang (lang);
    copyBtn.setButtonText (lang.t ("KOPIERA", "COPY")); pasteBtn.setButtonText (lang.t ("KLISTRA", "PASTE"));
    clearBtn.setButtonText (lang.t ("RENSA", "CLEAR")); fillBtn.setButtonText ("FILL");
    copyBtn.setTooltip (lang.t ("Kopiera mönstret på skärmen", "Copy the pattern on screen"));
    pasteBtn.setTooltip (lang.t ("Klistra in i mönstret på skärmen", "Paste into the pattern on screen"));
    clearBtn.setTooltip (lang.t ("Töm mönstret (KLISTRA tar tillbaka det)", "Empty the pattern (PASTE brings it back)"));
    fillBtn.setTooltip (lang.t ("Håll inne: steg märkta FILL spelar", "Hold: steps marked FILL play"));
    const char* mSV[kNumEditModes] = { "SLAG", "CHANS", "RULLE", "TUNE", "DECAY" };
    const char* mEN[kNumEditModes] = { "HITS", "CHANCE", "ROLL", "TUNE", "DECAY" };
    for (int k = 0; k < kNumEditModes; ++k) modeBtns[(size_t) k].setButtonText (lang.t (mSV[k], mEN[k]));
    for (int i = 0; i < kNumPatterns; ++i)
        patBtns[(size_t) i]->setTooltip (lang.t ("Mönster ", "Pattern ") + juce::String::charToString ((juce::juce_wchar) ('A' + i))
                                         + lang.t (" (byter på nästa takt)", " (switches on the next bar)"));
    setEditMode (editMode);
    for (auto* k : { &jitter, &drift, &fylla, &bpm, &smogAmt, &smogThr, &elnat, &master }) { k->resized(); k->repaint(); }
    repaint();
}

void Root::resized()
{
    // top bar
    langBtn.setBounds (1140, 12, 44, 24);
    buyBtn.setBounds (930, 12, 202, 24);
    prevPreset.setBounds (488, 13, 24, 22);
    presetBox.setBounds (516, 13, 250, 22);
    nextPreset.setBounds (770, 13, 24, 22);
    romBox.setBounds (1020, 13, 112, 22);
    midiBtn.setBounds (930, 13, 82, 22);
    seedDice.setBounds (898, 13, 24, 22);
    seed.setBounds (836, 13, 58, 22);

    // DIE ZEIT  (plate 8,52 396x250)
    strobe.setBounds (16, 74, 196, 196);
    jog.setBounds (232, 70, 120, 128);
    ownTempo.setBounds (290, 270, 104, 20);
    zeitDice.setBounds (368, 58, 24, 24);
    const int kw = 44, ky = 206;
    jitter.setBounds (214, ky, kw, 56); drift.setBounds (258, ky, kw, 56);
    fylla.setBounds (302, ky, kw, 56);  bpm.setBounds (346, ky, kw + 6, 56);

    // rings (plate 412,52 376x250)
    rings.setBounds (474, 66, 252, 232);

    // DETROIT SMOG (plate 796,52 396x250)
    meter.setBounds (812, 76, 232, 158);
    smogAmt.setBounds (1056, 78, 60, 72); smogThr.setBounds (1120, 78, 60, 72);
    elnat.setBounds (1056, 160, 60, 72);  master.setBounds (1120, 160, 60, 72);
    smogDice.setBounds (1152, 58, 24, 24);

    // page tabs
    tabStudio.setBounds (12, 307, 104, 24);
    tabDoom.setBounds (120, 307, 124, 24);

    constexpr int fy = 336;   // FX row top
    // VARVSHALL (plate 8,fy 300x152)
    { int x = 18; for (auto* k : { &hSize, &hDecay, &hDamp, &hPre, &hLevel }) { k->setBounds (x, fy + 30, 56, 70); x += 56; } }
    hFreeze.setBounds (18, fy + 110, 72, 22);
    // EKO (plate 316,fy 300x152)
    eTime.setBounds (328, fy + 34, 70, 22);
    ePing.setBounds (328, fy + 68, 70, 22);
    { int x = 404; for (auto* k : { &eFb, &eTone, &eWear, &eLevel }) { k->setBounds (x, fy + 30, 52, 70); x += 52; } }
    // ISOLATOR (plate 624,fy 276x152)
    { int x = 640; for (auto* k : { &iLow, &iMid, &iHigh }) { k->setBounds (x, fy + 24, 76, 82); x += 84; } }
    kLow.setBounds (652, fy + 112, 52, 20); kMid.setBounds (736, fy + 112, 52, 20); kHigh.setBounds (820, fy + 112, 52, 20);
    // PRESS (plate 908,fy 284x152)
    pAmt.setBounds (926, fy + 24, 76, 82); pPump.setBounds (1010, fy + 24, 76, 82);
    gr.setBounds (1110, fy + 24, 30, 104);
    fxDice.setBounds (1158, fy + 6, 24, 24);

    // UNDERGÅNG page, same plates
    { int x = 18; for (auto* k : { &mAmt, &mDepth, &mFreq, &mDirt, &mPump }) { k->setBounds (x, fy + 34, 56, 74); x += 56; } }
    { int x = 330; for (auto* k : { &oGas, &oRpm, &oTone, &oRattle }) { k->setBounds (x, fy + 30, 68, 82); x += 68; } }
    { int x = 636; for (auto* k : { &sAmt, &sSweep, &sColor, &sAlarm }) { k->setBounds (x, fy + 30, 64, 82); x += 64; } }
    doomKnob.setBounds (990, fy + 20, 120, 128);
    doomDice.setBounds (1158, fy + 6, 24, 24);

    // pattern strip (top of the lanes plate)
    constexpr int ly = 496;   // lanes plate top
    for (int i = 0; i < kNumPatterns; ++i) patBtns[(size_t) i]->setBounds (92 + i * 30, ly + 8, 28, 24);
    copyBtn.setBounds (344, ly + 9, 72, 22); pasteBtn.setBounds (420, ly + 9, 72, 22);
    clearBtn.setBounds (496, ly + 9, 62, 22); fillBtn.setBounds (566, ly + 9, 62, 22);
    for (int k = 0; k < kNumEditModes; ++k) modeBtns[(size_t) k].setBounds (728 + k * 76, ly + 9, 72, 22);

    // lanes
    for (int i = 0; i < kNumInstruments; ++i)
        lanes[(size_t) i]->setBounds (8, ly + 60 + i * 42, 1184, 42);
    slipDice.setBounds (8 + LaneRow::xMute + 11, ly + 36, 22, 22);
}

void Root::paint (juce::Graphics& g)
{
    g.drawImageAt (wall, 0, 0);

    // ---- title + fluorescent tube ----
    g.setFont (stencil (30.f)); g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawText (u ("MOTSTÅND"), 19, 7, 300, 36, juce::Justification::centredLeft, false);
    g.setColour (C::paper);
    g.drawText (u ("MOTSTÅND"), 18, 5, 300, 36, juce::Justification::centredLeft, false);
    g.setFont (stencil (10.5f)); g.setColour (C::paperDim);
    if (MotstandProcessor::isDemo) g.setColour (C::hazard);
    g.drawText (MotstandProcessor::isDemo ? u ("DEMO  ·  MASKINHALLEN  ·  PHUNKSHUNZ AUDIO") : u ("MASKINHALLEN  ·  PHUNKSHUNZ AUDIO"),
                214, 16, 272, 16, juce::Justification::centredLeft, false);
    {
        const float y = 46.f;
        juce::ColourGradient tube (C::tube.withAlpha (0.0f), 0, y - 6, C::tube.withAlpha (0.22f), 0, y, false);
        g.setGradientFill (tube); g.fillRect (12.f, y - 6.f, (float) W - 24.f, 6.f);
        g.setColour (C::tube.withAlpha (0.9f)); g.fillRect (60.f, y - 1.f, (float) W - 120.f, 1.6f);
        g.setColour (C::steel); g.fillRect (48.f, y - 3.f, 12.f, 5.f); g.fillRect ((float) W - 60.f, y - 3.f, 12.f, 5.f);
    }
    g.setFont (stencil (9.f)); g.setColour (C::paperDim);
    g.drawText ("SEED", 796, 13, 38, 22, juce::Justification::centredRight, false);

    // ---- plates ----
    plate (g, { 8.f, 52.f, 396.f, 250.f }, "DIE ZEIT");
    plate (g, { 412.f, 52.f, 376.f, 250.f }, lang.t ("RINGARNA", "THE RINGS"));
    plate (g, { 796.f, 52.f, 396.f, 250.f }, "DETROIT SMOG", true);
    if (fxPage == 0)
    {
        plate (g, { 8.f, 336.f, 300.f, 152.f }, "VARVSHALL");
        plate (g, { 316.f, 336.f, 300.f, 152.f }, "EKO");
        plate (g, { 624.f, 336.f, 276.f, 152.f }, "ISOLATOR");
        plate (g, { 908.f, 336.f, 284.f, 152.f }, "PRESS", true);
    }
    else
    {
        plate (g, { 8.f, 336.f, 300.f, 152.f }, "MULLER", true);
        plate (g, { 316.f, 336.f, 300.f, 152.f }, "MOTOR V8", true);
        plate (g, { 624.f, 336.f, 276.f, 152.f }, lang.t ("STORM + LARM", "STORM + SIREN"), true);
        plate (g, { 908.f, 336.f, 284.f, 152.f }, lang.t ("UNDERGÅNG", "DOOM"), true);
        // a warning glow behind the doom knob, growing with the knob
        const float d = proc.apvts.getRawParameterValue (pid::doom)->load();
        if (d > 0.01f) glowDot (g, 1050.f, 420.f, 34.f, C::alarm, d);
    }
    plate (g, { 8.f, 496.f, 1184.f, 488.f }, "");
    g.setFont (stencil (10.f)); g.setColour (C::paper.withAlpha (0.85f));
    g.drawText (lang.t ("MÖNSTER", "PATTERN"), 22, 505, 66, 22, juce::Justification::centredLeft, false);
    g.drawText (lang.t ("REDIGERA", "EDIT"), 652, 505, 72, 22, juce::Justification::centredRight, false);
    g.setColour (C::steel.withAlpha (0.5f)); g.fillRect (20.f, 531.f, 1160.f, 1.f);

    // ---- lane header ----
    g.setFont (stencil (9.5f)); g.setColour (C::paperDim);
    auto hdr = [&] (int x, int w, const juce::String& s) { g.drawText (s, 8 + x, 536, w, 14, juce::Justification::centred, false); };
    hdr (LaneRow::xName, 92, lang.t ("KANAL", "CHANNEL"));
    for (int i = 0; i < 32; i += 4)
        hdr (LaneRow::xGrid + i * StepGrid::kCell, StepGrid::kCell, juce::String (i + 1));
    hdr (LaneRow::xLen, 40, lang.t ("LÄNGD", "LENGTH"));
    const char* vSV[4] = { "TUNE", "DECAY", "TON", u8"NIVÅ" };
    const char* vEN[4] = { "TUNE", "DECAY", "TONE", "LEVEL" };
    for (int i = 0; i < 4; ++i) hdr (LaneRow::xKnobs + i * LaneRow::knobStep - 4, LaneRow::knobW + 8, lang.t (vSV[i], vEN[i]));
    g.setColour (C::tube.withAlpha (0.85f));
    hdr (LaneRow::xMode - 4, 96, "VINKELSLIP");
    g.setColour (C::paperDim);
    const char* sSV[3] = { "DRIVE", "MATNING", "MIX" };
    const char* sEN[3] = { "DRIVE", "FEED", "MIX" };
    for (int i = 0; i < 3; ++i) hdr (LaneRow::xSlip + i * LaneRow::slipStep - 5, LaneRow::knobW + 10, lang.t (sSV[i], sEN[i]));
    g.setColour (C::tube.withAlpha (0.7f));
    hdr (LaneRow::xSend - 4, LaneRow::knobW + 8, "HALL");
    hdr (LaneRow::xSend + LaneRow::knobStep - 4, LaneRow::knobW + 8, "EKO");
}

void Root::tick()
{
    strobe.update (proc.getSlipMs(), proc.getCurrentBpm(), proc.isRunning());
    meter.setValue (proc.getCollapse());
    gr.setValue (proc.getPressGr());
    {
        const int view = proc.viewPattern(), playingPat = proc.playingPattern();
        const bool blink = (juce::Time::getMillisecondCounter() / 250) % 2 == 0;
        for (int i = 0; i < kNumPatterns; ++i)
        {
            auto& b = *patBtns[(size_t) i];
            b.setToggleState (i == view, juce::dontSendNotification);
            b.playing = proc.isRunning() && i == playingPat;
            b.queued = proc.isRunning() && i == view && view != playingPat;
            b.blink = blink;
            b.repaint();
        }
        if (view != lastView) { lastView = view; for (auto& l : lanes) l->grid.repaint(); }
        if (proc.currentPreset != shownPreset)          // the DAW changed program
        {
            shownPreset = proc.currentPreset;
            presetBox.setSelectedId (shownPreset + 1, juce::dontSendNotification);
            for (auto& l : lanes) l->grid.repaint();
        }
        for (auto& l : lanes) l->poll();
        if (fxPage == 1)
        {   // keep the doom glow in step with the knob (and with automation)
            const float d = proc.apvts.getRawParameterValue (pid::doom)->load();
            if (std::abs (d - lastDoom) > 0.005f) { lastDoom = d; repaint (900, 330, 300, 170); }
        }
    }
    rings.repaint();
    const float n = proc.apvts.getRawParameterValue (pid::nudge)->load();
    if (std::abs (n) > 0.01f) jog.spin (n);
    const bool run = proc.isRunning();
    const int64_t step = proc.getStepIndex();
    for (int i = 0; i < kNumInstruments; ++i)
    {
        const int len = juce::jmax (1, (int) proc.apvts.getRawParameterValue (pid::inst (i, "len"))->load());
        lanes[(size_t) i]->grid.setPlayhead (run ? (int) (step % len) : -1);
    }
}

} // namespace dock

// =====================================================================
//  Editor
// =====================================================================
MotstandEditor::MotstandEditor (MotstandProcessor& p)
    : AudioProcessorEditor (p), root (p)
{
    setLookAndFeel (&lnf);
    root.setLookAndFeel (&lnf);
    tips.setLookAndFeel (&lnf);
    addAndMakeVisible (root);
    if (! MotstandProcessor::isDemo)
    {
        addChildComponent (licensRuta);    // visible by itself while the machine is locked
        licensRuta.onUpplast = [this] { repaint(); };
    }
    root.onLang = [this] { licensRuta.texter(); };
    licensRuta.texter();
    setResizable (true, true);
    setResizeLimits (660, 543, 1800, 1482);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) dock::Root::W / dock::Root::H);
    setSize (900, 702);   // fits a 13" laptop next to the DAW; drag the corner for bigger
    startTimerHz (30);
}

MotstandEditor::~MotstandEditor()
{
    stopTimer();
    tips.setLookAndFeel (nullptr);
    root.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void MotstandEditor::resized()
{
    const float s = (float) getWidth() / (float) dock::Root::W;
    root.setBounds (0, 0, dock::Root::W, dock::Root::H);
    root.setTransform (juce::AffineTransform::scale (s));
    licensRuta.setBounds (getLocalBounds());
    licensRuta.toFront (false);
}
