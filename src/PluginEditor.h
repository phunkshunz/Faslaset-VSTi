// MASKINHALLEN : MOTSTÅND — the DOCKLANDS panel (step 4b)
#pragma once
#include "PluginProcessor.h"
#include "Gui.h"

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
class StepGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    StepGrid (MotstandProcessor& p, int inst) : proc (p), lane (inst) {}
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void setPlayhead (int pos) { if (pos != playhead) { playhead = pos; repaint(); } }
    static constexpr int kCell = 15;
private:
    int cellAt (float x) const { return juce::jlimit (0, motstand::kMaxSteps - 1, (int) (x / kCell)); }
    MotstandProcessor& proc;
    int lane, playhead = -1, dragCell = -1, dragStartVel = 0;
    float dragStartY = 0.f;
    bool paintOn = true;
};

class LaneRow : public juce::Component
{
public:
    LaneRow (MotstandProcessor& p, int inst);
    void paint (juce::Graphics& g) override;
    void resized() override;
    void setLang (const Lang& l);
    void randomise();
    StepGrid grid;
    // layout x positions (design units), shared with the header
    static constexpr int xDice = 6, xName = 32, xGrid = 130, xLen = 618, xKnobs = 666, knobStep = 38, xMode = 826, xSlip = 920, slipStep = 38, xSend = 1042, xMute = 1124, knobW = 36;
private:
    MotstandProcessor& proc;
    int inst;
    juce::String name;
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
    static constexpr int W = 1200, H = 936;
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
    juce::TextButton langBtn;
    juce::ComboBox romBox;
    juce::ToggleButton midiBtn;
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
    // lanes
    std::vector<std::unique_ptr<LaneRow>> lanes;
    DiceButton slipDice;

    std::vector<std::unique_ptr<APVTS::SliderAttachment>> sAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> romAtt;
    std::unique_ptr<APVTS::ButtonAttachment> midiAtt, tempoAtt;
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
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotstandEditor)
};
