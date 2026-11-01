#pragma once
#include "PluginProcessor.h"

class VevhusEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit VevhusEditor (VevhusProcessor&);
    ~VevhusEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Ratt
    {
        juce::Slider s;
        juce::Label namn;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Typ
    {
        juce::ComboBox box;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> att;
    };
    struct Look : juce::LookAndFeel_V4
    {
        Look();
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float a0, float a1, juce::Slider&) override;
    };

    VevhusProcessor& proc;
    Look look;
    std::vector<std::unique_ptr<Ratt>> ratter;
    std::array<Typ, 5> typer;
    juce::ToggleButton avgang;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> avgangAtt;
    juce::TextButton slumpa { "SLUMPA" }, forra { "<" }, nasta { ">" };
    juce::ComboBox presetBox;
    juce::TooltipWindow tips { this, 600 };
    int visadPreset = -1;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VevhusEditor)
};
