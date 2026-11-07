// MASKINHALLEN : MOTSTÅND — the DOCKLANDS panel (step 4b)
#pragma once
#include "PluginProcessor.h"
#include "Gui.h"
#include "Licens.h"

namespace dock {

using APVTS = juce::AudioProcessorValueTreeState;

// Text in both languages. Swedish labels stay the default.
struct Lang
{
    bool en = false;
    juce::String t (const char* sv, const char* enText) const { return u (en ? enText : sv); }
};

// ---------------------------------------------------------------------
//  Small pieces
// ---------------------------------------------------------------------
class DiceButton : public juce::Button
{
public:
    DiceButton() : juce::Button ("dice") {}
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.f).translated (0.f, down ? 1.f : 0.f);
        const float s = std::min (r.getWidth(), r.getHeight());
        r = r.withSizeKeepingCentre (s, s);
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (1.f, 1.5f), s * 0.2f);
        g.setColour (over ? C::paper : C::paper.withAlpha (0.85f)); g.fillRoundedRectangle (r, s * 0.2f);
        g.setColour (C::rustDk);
        const float d = s * 0.16f, c = s * 0.5f, o = s * 0.26f;
        for (auto p : { juce::Point<float> (c - o, c - o), { c + o, c - o }, { c, c }, { c - o, c + o }, { c + o, c + o } })
            g.fillEllipse (r.getX() + p.x - d * 0.5f, r.getY() + p.y - d * 0.5f, d, d);
    }
};

class LabeledKnob : public juce::Component
{
public:
    LabeledKnob()
    {
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        slider.setPopupDisplayEnabled (true, true, nullptr);
        addAndMakeVisible (slider);
    }
    juce::Slider slider;
    juce::String text;
    void resized() override { slider.setBounds (getLocalBounds().withTrimmedBottom (text.isEmpty() ? 0 : 14)); }
    void paint (juce::Graphics& g) override
    {
        if (text.isEmpty()) return;
        g.setFont (stencil (10.f)); g.setColour (C::paper.withAlpha (0.85f));
        g.drawFittedText (text, getLocalBounds().removeFromBottom (14), juce::Justification::centred, 1, 0.7f);
    }
};

// ---------------------------------------------------------------------
//  POLYMETER RINGS — every active lane is a ring; watch 7, 11 and 13
//  slide against the kick's 16 and hunt for the one.
// ---------------------------------------------------------------------
class PolyRings : public juce::Component
{
public:
    PolyRings (MotstandProcessor& p) : proc (p) {}
    juce::String oneIn = "ETTAN OM", bars = "TAKTER", together = "ETTAN!";
    void paint (juce::Graphics& g) override;
private:
    MotstandProcessor& proc;
};

// ---------------------------------------------------------------------
//  One lane: dice, Dymo name, 32-step grid, length, voice knobs,
//  VINKELSLIP and mute.
// ---------------------------------------------------------------------
// what the step grid edits right now
enum EditMode { kSlag = 0, kChans, kRulle, kTune, kDecay, kNumEditModes };

class StepGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    StepGrid (MotstandProcessor& p, int inst, const int& editMode) : proc (p), lane (inst), mode (editMode) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void setPlayhead (int pos) { if (pos != playhead) { playhead = pos; repaint(); } }
    static constexpr int kCell = 15;
private:
    int cellAt (float x) const { return juce::jlimit (0, motstand::kMaxSteps - 1, (int) (x / kCell)); }
    MotstandProcessor& proc;
    int lane;
    const int& mode;
    int playhead = -1, dragCell = -1, dragStartVel = 0;
    float dragStartY = 0.f;
    bool paintOn = true;
};

// A..H: lit = on screen, white ring = playing, blinking = waiting for the bar
class PatternButton : public juce::Button
{
public:
    PatternButton (int i) : juce::Button (juce::String::charToString ((juce::juce_wchar) ('A' + i))) {}
    bool playing = false, queued = false, blink = false;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f).translated (0.f, down ? 1.f : 0.f);
        const bool sel = getToggleState();
        g.setColour (juce::Colours::black.withAlpha (0.55f)); g.fillRoundedRectangle (r.translated (0.f, 1.f), 3.f);
        g.setColour (sel ? C::tube.withAlpha (0.28f) : (over ? C::iron2.brighter (0.2f) : C::night)); g.fillRoundedRectangle (r, 3.f);
        g.setColour (queued && blink ? C::hazard : sel ? C::tube : C::steel); g.drawRoundedRectangle (r, 3.f, sel ? 1.4f : 1.f);
        if (playing) { g.setColour (juce::Colours::white); g.fillEllipse (r.getRight() - 6.f, r.getY() + 3.f, 3.5f, 3.5f); }
        g.setFont (stencil (13.f)); g.setColour (sel ? C::tube : C::paperDim);
        g.drawText (getName(), r, juce::Justification::centred, false);
    }
};

class LaneRow : public juce::Component, public juce::SettableTooltipClient
{
public:
    LaneRow (MotstandProcessor& p, int inst, const int& editMode);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void setLang (const Lang& l);
    void randomise();
    void poll();                                   // repaint if the sound model changed
    void mouseDown (const juce::MouseEvent& e) override;
    void lockForDemo (std::vector<juce::Component*>& locked);   // DEMO: what this lane keeps closed
    StepGrid grid;
    // layout x positions (design units), shared with the header
    static constexpr int xDice = 6, xName = 32, xGrid = 130, xLen = 618, xKnobs = 666, knobStep = 38, xMode = 826, xSlip = 920, slipStep = 38, xSend = 1042, xMute = 1124, knobW = 36;
private:
    MotstandProcessor& proc;
    int inst;
    juce::String name;
    Lang lang;
    int lastModel = -1;
    DiceButton dice;
    juce::Slider len;
    std::array<juce::Slider, 4> voice;      // TUNE DECAY TON NIVÅ
    juce::ComboBox mode;
    std::array<juce::Slider, 3> slipK;      // DRIVE MATNING MIX
    std::array<juce::Slider, 2> sends;      // HALL EKO
    juce::ToggleButton mute;
    std::vector<std::unique_ptr<APVTS::SliderAttachment>> sAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> mAtt;
    std::unique_ptr<APVTS::ButtonAttachment> muteAtt;
};

// ---------------------------------------------------------------------
//  The whole panel, drawn at 1200 x 780 design units and scaled.
// ---------------------------------------------------------------------
class Root : public juce::Component
{
public:
    explicit Root (MotstandProcessor& p);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void tick();                       // 30 Hz from the editor timer
    static constexpr int W = 1200, H = 988;
private:
    void setLang (bool english);
    void randomiseZeit();
    void randomiseSmog();
    void randomiseSlip();
    void setParam (const juce::String& id, float plainValue);

    MotstandProcessor& proc;
    Lang lang;
    juce::Image wall;

    // top bar
    juce::TextButton langBtn, prevPreset, nextPreset, buyBtn;
    std::vector<juce::Component*> demoLocked;
    void setupDemo();
    void stepPreset (int dir);
public:
    std::function<void()> onLang;          // the editor refreshes the licence box text
    void paintOverChildren (juce::Graphics& g) override;
private:
    juce::ComboBox presetBox;
    int shownPreset = -1;
    void pickPreset (int index);
    juce::ComboBox romBox;
    juce::ToggleButton midiBtn, multiBtn;
    juce::Slider seed;
    DiceButton seedDice;
    // DIE ZEIT
    StrobeRing strobe;
    JogWheel jog;
    LabeledKnob jitter, drift, fylla, bpm;
    juce::ToggleButton ownTempo;
    DiceButton zeitDice;
    // rings
    PolyRings rings;
    // DETROIT SMOG
    SmogMeter meter;
    LabeledKnob smogAmt, smogThr, elnat, master;
    DiceButton smogDice;
    // VARVSHALL
    LabeledKnob hSize, hDecay, hDamp, hPre, hLevel;
    juce::ToggleButton hFreeze;
    // EKO
    juce::ComboBox eTime;
    LabeledKnob eFb, eTone, eWear, eLevel;
    juce::ToggleButton ePing;
    // ISOLATOR
    LabeledKnob iLow, iMid, iHigh;
    juce::ToggleButton kLow, kMid, kHigh;
    // PRESS
    LabeledKnob pAmt, pPump;
    GrMeter gr;
    DiceButton fxDice;
    // UNDERGÅNG page
    juce::TextButton tabStudio, tabDoom;
    int fxPage = 0;
    void setFxPage (int p);
    LabeledKnob mAmt, mDepth, mFreq, mDirt, mPump;        // MULLER
    LabeledKnob oGas, oRpm, oTone, oRattle;               // MOTOR
    LabeledKnob sAmt, sSweep, sColor, sAlarm;             // STORM
    LabeledKnob doomKnob;                                 // UNDERGÅNG
    DiceButton doomDice;
    void randomiseDoom();
    float lastDoom = -1.f;
    // patterns + edit modes
    int editMode = kSlag;
    std::vector<std::unique_ptr<PatternButton>> patBtns;
    juce::TextButton copyBtn, pasteBtn, clearBtn, fillBtn;
    std::array<juce::TextButton, kNumEditModes> modeBtns;
    std::unique_ptr<motstand::PatternData> clipboard;
    void setEditMode (int m);
    bool fillDown = false;
    int lastView = -1;
    // lanes
    std::vector<std::unique_ptr<LaneRow>> lanes;
    DiceButton slipDice;

    std::vector<std::unique_ptr<APVTS::SliderAttachment>> sAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> romAtt;
    std::unique_ptr<APVTS::ButtonAttachment> midiAtt, multiAtt, tempoAtt;
    std::vector<std::unique_ptr<APVTS::ButtonAttachment>> bAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> eTimeAtt;
    void randomiseFx();
};

} // namespace dock

class MotstandEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MotstandEditor (MotstandProcessor&);
    ~MotstandEditor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;
private:
    void timerCallback() override { root.tick(); }
    dock::DockLnF lnf;
    dock::Root root;
    licens::Ruta licensRuta;               // fullversionen: ber om nyckeln tills maskinen är upplåst
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotstandEditor)
};
