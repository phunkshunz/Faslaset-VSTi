// MASKINHALLEN : MOTSTÅND — audio processor (step 3)
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "MotstandCore.h"
#include "Voices.h"
#include "Params.h"

class MotstandProcessor : public juce::AudioProcessor
{
public:
    MotstandProcessor();
    ~MotstandProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "MOTSTAND"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- pattern (GUI writes, audio reads; one byte per cell is atomic) ----
    uint8_t getStep (int inst, int step) const { return cells[(size_t) inst][(size_t) step].load (std::memory_order_relaxed); }
    void setStep (int inst, int step, uint8_t vel) { cells[(size_t) inst][(size_t) step].store (vel, std::memory_order_relaxed); }

    // ---- meters for the GUI ----
    float getSlipMs() const      { return slipMs.load(); }
    float getCollapse() const    { return collapse.load(); }
    float getCurrentBpm() const  { return bpmNow.load(); }
    int64_t getStepIndex() const { return stepNow.load(); }
    bool isRunning() const       { return running.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    static BusesProperties makeBuses();
    void pullParameters();
    void reseed();
    void triggerVoice (int inst, float vel01, int sampleOffset);

    std::array<std::array<std::atomic<uint8_t>, motstand::kMaxSteps>, motstand::kNumInstruments> cells;

    motstand::DieZeit clock;
    motstand::Kit kit;
    motstand::DetroitSmog smog;
    std::array<motstand::DrunkRider, motstand::kNumInstruments> rider;
    std::array<motstand::PolyTrack, motstand::kNumInstruments> lanes;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::AudioBuffer<float> mixBuffer;
    std::array<bool, motstand::kNumInstruments> toAux {};   // routed to its own output this block?
    std::array<float, motstand::kNumInstruments> pan { 0.f, 0.f, 0.1f, -0.5f, -0.4f, 0.1f, 0.5f, 0.45f, -0.3f, 0.35f };

    // cached parameter pointers
    struct InstParams { std::atomic<float>* tune; std::atomic<float>* decay; std::atomic<float>* tone;
                        std::atomic<float>* level; std::atomic<float>* len; std::atomic<float>* mute; };
    std::array<InstParams, motstand::kNumInstruments> ip {};
    std::atomic<float> *pTempoSrc, *pBpm, *pJitter, *pDrift, *pNudge, *pRide, *pSmog, *pSmogThr, *pSeed, *pMaster;

    double sr = 48000.0;
    bool wasPlaying = false;
    int lastSeed = -1;
    float masterGain = 1.f;

    std::atomic<float> slipMs { 0.f }, collapse { 0.f }, bpmNow { 133.f };
    std::atomic<int64_t> stepNow { 0 };
    std::atomic<bool> running { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotstandProcessor)
};
