#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "Params.h"
#include "Engine.h"
#include "Bassline.h"
#include "Kicks.h"

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
    double getTailLengthSeconds() const override { return std::numeric_limits<double>::infinity(); }

    int getNumPrograms() override { return fl::numPresets(); }
    int getCurrentProgram() override { return juce::jmax (0, currentPreset.load()); }
    void setCurrentProgram (int index) override { loadPreset (index); }
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---------------------------------------------------------------- för GUI:t (meddelandetråden)
    juce::AudioProcessorValueTreeState apvts;
    fl::Pattern pattern;

    void loadPreset (int index);
    void diceKick();
    void diceBass();          // ny basgång: rytm + toner
    void diceNotes();         // nya toner, samma rytm
    void diceRhythm();        // ny rytm, samma toner
    void diceSound();         // basens klang (filter, form, bett ...)
    void diceAll();           // allt: kick, tonart, skala, stil, längd, basgång
    void regenerate (uint32_t seed, bool keepRhythm, bool keepNotes);

    void pushUndo();
    bool undo();
    bool canUndo() const { return ! undoStack.empty(); }
    void edited() { currentPreset = -1; }

    juce::String presetLabel() const;
    float get (int id) const { return raw[(size_t) id]->load(); }

    fl::ScopeFifo& scope() noexcept { return engine.scope(); }
    uint32_t overlaps() const noexcept { return engine.overlapCount.load(); }
    uint32_t faults() const noexcept { return engine.faultCount.load(); }
    int playingBeat() const noexcept { return engine.currentBeat.load(); }
    int numKicks() const noexcept { return kicks.size(); }

    std::atomic<double> uiBpm { 145.0 }, uiSr { 48000.0 };
    std::atomic<bool> uiPlaying { false };
    std::atomic<int> currentPreset { -1 };

private:
    void setParam (int id, float value);
    juce::ValueTree snapshot();
    void restore (const juce::ValueTree& tree);
    juce::String patternToString() const;
    void patternFromString (const juce::String& s);
    uint32_t newSeed();

    std::array<std::atomic<float>*, fl::NUM_P> raw {};
    std::atomic<float>* pSpela = nullptr;

    fl::KickBank kicks;
    fl::Engine engine;
    std::vector<float> mono;
    double sr = 48000.0;
    double internalPpq = 0.0;
    bool internalWasOn = false;
    juce::Random random;

    std::vector<juce::ValueTree> undoStack;
    bool suppressUndo = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FaslasetProcessor)
};
