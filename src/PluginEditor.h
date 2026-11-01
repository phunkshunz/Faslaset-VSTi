#pragma once
#include "PluginProcessor.h"

class VevhusEditor : public juce::AudioProcessorEditor
{
public:
    explicit VevhusEditor (VevhusProcessor&);
    ~VevhusEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Ratt
    {
        juce::Slider s;
        juce::Label namn;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };
    struct Look : juce::LookAndFeel_V4
    {
        void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                               float a0, float a1, juce::Slider&) override;
    };

    VevhusProcessor& proc;
    Look look;
    std::vector<std::unique_ptr<Ratt>> ratter;
    juce::ToggleButton avgang;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> avgangAtt;
    juce::TextButton slumpa { "SLUMPA" };
    juce::TooltipWindow tips { this, 600 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VevhusEditor)
};
