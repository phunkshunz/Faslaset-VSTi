#pragma once

#include "PluginProcessor.h"
#include "Gui.h"
#include "About.h"
#include "Scope.h"
#include "BassGrid.h"

class FaslasetEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FaslasetEditor (FaslasetProcessor&);
    ~FaslasetEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void showPresetMenu();
    void setMode (int m);

    FaslasetProcessor& proc;
    ArmLnF lnf;
    juce::TooltipWindow tips { this, 600 };

    // toppen
    std::unique_ptr<juce::Component> logo;
    juce::TextButton prevBtn { juce::String::fromUTF8 ("◀") }, nextBtn { juce::String::fromUTF8 ("▶") }, presetBtn;
    juce::TextButton spikBtn { "SPIKRAKT" }, slapBtn { juce::String::fromUTF8 ("SLÄP") };
    juce::TextButton undoBtn { juce::String::fromUTF8 ("ÅNGRA") }, diceAllBtn { "DICE" };
    ParamToggle spela { "SPELA" };
    Knob tempo;

    Scope scope;

    // kick
    ParamCombo kickBox;
    Knob kStam, kLangd, kDriv, kNiva;
    DiceButton kickDice { juce::String::fromUTF8 ("Slumpa kicken") };

    // bas
    BassGrid grid;
    ParamCombo skala, stil;
    Knob bTon, bSlag;
    Knob bForm, bFilter, bReso, bEnv, bEnvTid, bDecay, bLangd, bBett, bNiva;
    juce::TextButton tonerBtn { "TONER" }, rytmBtn { "RYTM" }, klangBtn { "KLANG" };
    DiceButton bassDice { juce::String::fromUTF8 ("Ny basgång: rytm och toner") };

    // master
    Knob utkling, volym;

    juce::Rectangle<int> topArea, scopeArea, kickArea, bassArea, masterArea, skalaLabel, stilLabel, kickLabel;
    AboutOverlay about;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaslasetEditor)
};
