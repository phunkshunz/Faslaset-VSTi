#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Engine.h"

class FaslasetProcessor : public juce::AudioProcessor
{
public:
    FaslasetProcessor();
    ~FaslasetProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return juce::String::fromUTF8 ("FASLÅSET"); }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return std::numeric_limits<double>::infinity(); } // sövs aldrig av DAW:en

    int getNumPrograms() override { return fl::numPresets(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---------------------------------------------------------------- för GUI:t (meddelandetråden)
    juce::AudioProcessorValueTreeState apvts;

    void loadPreset (int index);
    void diceKick();
    void diceBass();
    void diceAll();
    juce::String presetLabel() const;

    fl::ScopeFifo& scope() noexcept { return engine.scope(); }
    uint32_t overlaps() const noexcept { return engine.overlapCount.load(); }
    uint32_t faults() const noexcept { return engine.faultCount.load(); }

    std::atomic<double> uiBpm { 145.0 }, uiSr { 48000.0 };
    std::atomic<bool> uiPlaying { false };
    std::atomic<int> currentPreset { -1 };

private:
    void setParam (int id, float value);
    float get (int id) const { return raw[(size_t) id]->load(); }
    void readCurrent (float* v) const;

    std::array<std::atomic<float>*, fl::NUM_P> raw {};
    std::atomic<float>* pSpela = nullptr;

    fl::Engine engine;
    std::vector<float> mono;
    double sr = 48000.0;
    double internalPpq = 0.0;
    bool internalWasOn = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaslasetProcessor)
};
