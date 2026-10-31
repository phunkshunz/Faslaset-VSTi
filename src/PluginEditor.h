#pragma once

#include "PluginProcessor.h"
#include "Gui.h"
#include "About.h"
#include "Scope.h"

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
    juce::TextButton diceAllBtn { "DICE" };
    ParamToggle spela { "SPELA" };
    Knob tempo;

    Scope scope;

    // panelerna
    Knob kStart, kSlut, kSvep, kLangd, kDriv, kNiva;
    Knob bTon, bForm, bFilter, bReso, bEnv, bEnvTid, bDecay, bBett, bNiva;
    Knob giljotin, volym;
    DiceButton kickDice { juce::String::fromUTF8 ("Slumpa kicken") }, bassDice { juce::String::fromUTF8 ("Slumpa basen") };

    juce::Rectangle<int> topArea, scopeArea, kickArea, bassArea, masterArea;
    AboutOverlay about;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaslasetEditor)
};
