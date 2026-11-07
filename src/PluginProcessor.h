// MASKINHALLEN : MOTSTÅND — audio processor
#pragma once

// DEMO: byggs med -DMOTSTAND_DEMO=ON. Samma kod, men bara några rattar öppna.
#ifndef MOTSTAND_DEMO
 #define MOTSTAND_DEMO 0
#endif
// Testbyggen (plugintest, rundtur, skärmbild) körs upplåsta. Aldrig i det som säljs.
#ifndef MOTSTAND_TEST_UNLOCKED
 #define MOTSTAND_TEST_UNLOCKED 0
#endif

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "MotstandCore.h"
#include "Voices.h"
#include "Params.h"
#include "Vinkelslip.h"

class MotstandProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    MotstandProcessor();
    ~MotstandProcessor() override { stopTimer(); }

    static constexpr bool isDemo = MOTSTAND_DEMO != 0;
    // DEMO: the presets you can try (index into the full list)
    static const std::vector<int>& demoPresets();
    static bool presetAllowed (int index);
    // DEMO: parameters you may turn (everything else is held at the preset's value)
    static bool demoOpen (const juce::String& paramId);

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return juce::String::fromUTF8 (isDemo ? "MOTSTÅND DEMO" : "MOTSTÅND"); }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }

    // presets show up as programs in the DAW, too
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void loadPreset (int index);
    int currentPreset = 0;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // ---- pattern bank (GUI writes, audio reads; one 32-bit step is atomic) ----
    motstand::Step getCell (int pat, int inst, int step) const
    { return motstand::Step::unpack (bank[(size_t) pat][(size_t) inst][(size_t) step].load (std::memory_order_relaxed)); }
    void setCell (int pat, int inst, int step, const motstand::Step& st)
    { bank[(size_t) pat][(size_t) inst][(size_t) step].store (st.pack(), std::memory_order_relaxed); }
    int viewPattern() const     { return juce::jlimit (0, motstand::kNumPatterns - 1, (int) pPattern->load()); }
    int playingPattern() const  { return activePat.load(); }
    // velocity-only access to the pattern on screen (keeps chance, rolls and locks)
    uint8_t getStep (int inst, int step) const { return (uint8_t) getCell (viewPattern(), inst, step).vel; }
    void setStep (int inst, int step, uint8_t vel)
    {
        auto st = getCell (viewPattern(), inst, step);
        if (vel == 0) st = motstand::Step::unpack (motstand::Step::rest());   // a rest forgets its extras
        st.vel = vel; setCell (viewPattern(), inst, step, st);
    }
    motstand::PatternData copyPattern (int pat) const;
    void pastePattern (int pat, const motstand::PatternData& d);

    // ---- meters for the GUI ----
    float getSlipMs() const      { return slipMs.load(); }
    float getCollapse() const    { return collapse.load(); }
    float getPressGr() const     { return pressGr.load(); }
    float getCurrentBpm() const  { return bpmNow.load(); }
    int64_t getStepIndex() const { return stepNow.load(); }
    bool isRunning() const       { return running.load(); }

    juce::AudioProcessorValueTreeState apvts;

private:
    void timerCallback() override;
    std::vector<std::pair<juce::String, float>> demoLocked;   // DEMO: locked parameters and their preset values
    void captureDemoLocks();
    static BusesProperties makeBuses();
    void pullParameters();
    void reseed();
    bool triggerVoice (int inst, float vel01, int sampleOffset, int tuneLock = 0, int decayLock = 0);

    std::array<std::array<std::array<std::atomic<uint32_t>, motstand::kMaxSteps>, motstand::kNumInstruments>, motstand::kNumPatterns> bank;
    std::atomic<int> activePat { 0 };
    motstand::Rng chanceRng;
    struct Roll { int wait, inst, vel, tune, decay; };
    std::array<Roll, 128> rolls {};
    int numRolls = 0;

    motstand::DieZeit clock;
    motstand::Kit kit;
    motstand::DetroitSmog smog;
    std::array<motstand::DrunkRider, motstand::kNumInstruments> rider;
    std::array<motstand::PolyTrack, motstand::kNumInstruments> lanes;
    std::array<motstand::Vinkelslip, motstand::kNumInstruments> slip;
    motstand::Elnat elnat;
    motstand::Varvshall hall;
    motstand::Eko eko;
    motstand::Isolator iso;
    motstand::Press press;
    motstand::Pump pump;
    motstand::Muller muller;
    motstand::Motor motor;
    motstand::Storm storm;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    // MIDI out: note-offs waiting to be sent (one per instrument is enough)
    std::array<int, motstand::kNumInstruments> pendingOff {};   // samples left, 0 = none
    juce::MidiBuffer midiOut;
    bool sendMidi = true;

    juce::AudioBuffer<float> mixBuffer;
    std::array<bool, motstand::kNumInstruments> toAux {};   // routed to its own output this block?
    std::array<float, motstand::kNumInstruments> pan { 0.f, 0.f, 0.1f, -0.5f, -0.4f, 0.1f, 0.5f, 0.45f, -0.3f, 0.35f };

    // cached parameter pointers
    struct InstParams { std::atomic<float>* tune; std::atomic<float>* decay; std::atomic<float>* tone;
                        std::atomic<float>* level; std::atomic<float>* len; std::atomic<float>* mute;
                        std::atomic<float>* slipMode; std::atomic<float>* slipDrive; std::atomic<float>* slipFeed; std::atomic<float>* slipMix;
                        std::atomic<float>* sendHall; std::atomic<float>* sendEko; std::atomic<float>* model; };
    std::array<InstParams, motstand::kNumInstruments> ip {};
    std::atomic<float> *pTempoSrc, *pBpm, *pJitter, *pDrift, *pNudge, *pRide, *pSmog, *pSmogThr, *pSeed, *pMaster, *pElnat, *pMidiOut, *pRomMode,
        *pHallSize, *pHallDecay, *pHallDamp, *pHallPre, *pHallLevel, *pHallFreeze,
        *pEkoTime, *pEkoFb, *pEkoTone, *pEkoWear, *pEkoLevel, *pEkoPing,
        *pIsoLow, *pIsoMid, *pIsoHigh, *pKillLow, *pKillMid, *pKillHigh, *pPress, *pPump, *pPattern, *pFill,
        *pMuller, *pMullerDepth, *pMullerFreq, *pMullerDirt, *pMullerPump,
        *pMotorGas, *pMotorRpm, *pMotorTone, *pMotorRattle, *pStorm, *pStormSweep, *pStormColor, *pAlarm, *pDoom;
    float hallLevel = 0.8f, ekoLevel = 0.8f;
    std::array<float, motstand::kNumInstruments> sendH {}, sendE {};

    double sr = 48000.0;
    bool wasPlaying = false;
    int lastSeed = -1;
    float masterGain = 1.f;

    std::atomic<float> slipMs { 0.f }, collapse { 0.f }, bpmNow { 133.f }, pressGr { 0.f };
    std::atomic<int64_t> stepNow { 0 };
    std::atomic<bool> running { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MotstandProcessor)
};
