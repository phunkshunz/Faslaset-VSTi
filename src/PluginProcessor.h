#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Engine.h"
#include "Presets.h"

class VevhusProcessor : public juce::AudioProcessor
{
public:
    VevhusProcessor();
    ~VevhusProcessor() override = default;

    void prepareToPlay (double sampleRate, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& l) const override
    { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "VEVHUS"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 2.0; }
    int getNumPrograms() override { return kNumPresets; }
    int getCurrentProgram() override { return aktuell; }
    void setCurrentProgram (int i) override;
    const juce::String getProgramName (int i) override
    { return juce::isPositiveAndBelow (i, kNumPresets) ? juce::String::fromUTF8 (kPresets[i].namn) : juce::String(); }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    void slumpa();

    juce::AudioProcessorValueTreeState apvts;
    static const juce::StringArray& typNamn (int kalla);   // 0 ånga, 1 pys, 2 knack, 3 klick, 4 skrammel
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();

private:
    pr::Params las();
    pr::Engine engine;
    double egenPos = 0;   // fritt löpande position när transporten står still
    juce::Random slump;
    int aktuell = 0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VevhusProcessor)
};
