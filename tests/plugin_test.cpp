// MOTSTÅND — plugin test (runs the real processor without a DAW)
//   cmake -B build -DMOTSTAND_TESTS=ON && cmake --build build --target MotstandTest && ./build/.../MotstandTest
#include "../src/PluginProcessor.h"
#include <cstdio>

static int failures = 0;
#define CHECK(cond, ...) do { if (!(cond)) { ++failures; std::printf("FAIL: "); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct FakeHost : juce::AudioPlayHead
{
    bool playing = true; double bpm = 128.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setIsPlaying (playing); p.setBpm (bpm); return p;
    }
};

struct Result { double mainRms = 0, peak = 0; std::array<double, motstand::kNumInstruments> auxRms {}; bool finite = true; };

static Result run (MotstandProcessor& p, double seconds, juce::MidiBuffer* midiFirst = nullptr)
{
    const int bs = 512; const double sr = 48000;
    juce::AudioBuffer<float> buf (p.getTotalNumOutputChannels(), bs);
    juce::MidiBuffer midi;
    Result r; double sumMain = 0; long nMain = 0;
    std::array<double, motstand::kNumInstruments> sumAux {};
    const int blocks = (int) (seconds * sr / bs);
    for (int b = 0; b < blocks; ++b)
    {
        midi.clear();
        if (b == 0 && midiFirst) midi = *midiFirst;
        p.processBlock (buf, midi);
        auto main = p.getBusBuffer (buf, false, 0);
        for (int c = 0; c < main.getNumChannels(); ++c)
            for (int i = 0; i < bs; ++i)
            {
                const float x = main.getSample (c, i);
                if (! std::isfinite (x)) r.finite = false;
                sumMain += x * x; ++nMain; r.peak = std::max (r.peak, (double) std::fabs (x));
            }
        for (int k = 0; k < motstand::kNumInstruments; ++k)
            if (auto* bus = p.getBus (false, 1 + k); bus && bus->isEnabled())
            {
                auto ab = p.getBusBuffer (buf, false, 1 + k);
                sumAux[(size_t) k] += ab.getRMSLevel (0, 0, bs) * ab.getRMSLevel (0, 0, bs);
            }
    }
    r.mainRms = std::sqrt (sumMain / std::max (1L, nMain));
    for (int k = 0; k < motstand::kNumInstruments; ++k) r.auxRms[(size_t) k] = std::sqrt (sumAux[(size_t) k] / blocks);
    return r;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const double sr = 48000;

    // 1. Standalone (no host): runs by itself and makes sound
    {
        MotstandProcessor p; p.prepareToPlay (sr, 512);
        auto r = run (p, 8.0);
        std::printf ("RUN      main rms %.3f, peak %.2f, latency %d samples, slip %.2f ms\n",
                     r.mainRms, r.peak, p.getLatencySamples(), p.getSlipMs());
        CHECK (r.finite, "NaN/inf in output");
        CHECK (r.mainRms > 0.02, "too quiet (%.3f)", r.mainRms);
        CHECK (r.peak < 1.2, "too hot (%.2f)", r.peak);
        CHECK (p.getStepIndex() > 60, "sequencer did not advance");   // 8 s at 133 BPM = ~71 steps
    }
    // 2. Host stopped: silent
    {
        MotstandProcessor p; FakeHost h; h.playing = false; p.setPlayHead (&h); p.prepareToPlay (sr, 512);
        auto r = run (p, 2.0);
        std::printf ("STOPPED  main rms %.5f\n", r.mainRms);
        CHECK (r.mainRms < 1e-4, "plays while transport is stopped");
    }
    // 3. DAW tempo as base, drift on top
    {
        MotstandProcessor p; FakeHost h; h.bpm = 128.0; p.setPlayHead (&h); p.prepareToPlay (sr, 512);
        run (p, 20.0);
        std::printf ("TEMPO    host 128, machine now %.2f BPM\n", p.getCurrentBpm());
        CHECK (std::fabs (p.getCurrentBpm() - 128.0) < 128.0 * 0.016, "tempo not following host base");
    }
    // 4. Own output for the kick
    {
        MotstandProcessor p;
        auto layout = p.getBusesLayout();
        layout.outputBuses.getReference (1 + motstand::KICK) = juce::AudioChannelSet::stereo();
        CHECK (p.setBusesLayout (layout), "could not enable kick output");
        p.prepareToPlay (sr, 512);
        auto r = run (p, 6.0);
        std::printf ("MULTIOUT kick bus rms %.3f, snare bus enabled: %s\n", r.auxRms[motstand::KICK],
                     p.getBus (false, 1 + motstand::SNARE)->isEnabled() ? "yes" : "no");
        CHECK (r.auxRms[motstand::KICK] > 0.02, "kick bus is silent");
        CHECK (r.mainRms > 0.01, "mix went silent");
    }
    // 5. MIDI note 36 triggers the kick with the transport stopped
    {
        MotstandProcessor p; FakeHost h; h.playing = false; p.setPlayHead (&h); p.prepareToPlay (sr, 512);
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 120), 100);
        auto r = run (p, 1.0, &m);
        std::printf ("MIDI     note 36 -> rms %.3f\n", r.mainRms);
        CHECK (r.mainRms > 0.01, "MIDI note did not sound");
    }
    // 6. State round trip (parameters + pattern)
    {
        MotstandProcessor a; a.prepareToPlay (sr, 512);
        a.apvts.getParameter (motstand::pid::seed)->setValueNotifyingHost (0.5f);
        a.setStep (motstand::TOM_HI, 5, 77);
        juce::MemoryBlock mb; a.getStateInformation (mb);
        MotstandProcessor b; b.setStateInformation (mb.getData(), (int) mb.getSize());
        const float sa = a.apvts.getRawParameterValue (motstand::pid::seed)->load();
        const float sb = b.apvts.getRawParameterValue (motstand::pid::seed)->load();
        std::printf ("STATE    seed %.0f -> %.0f, tom step 5: %d, %d bytes\n", sa, sb, b.getStep (motstand::TOM_HI, 5), (int) mb.getSize());
        CHECK (sa == sb, "seed not restored");
        CHECK (b.getStep (motstand::TOM_HI, 5) == 77, "pattern not restored");
        CHECK (b.getStep (motstand::KICK, 0) == 120, "default pattern lost");
    }
    // 7. Same seed = same take
    {
        auto render = [&] {
            MotstandProcessor p; p.prepareToPlay (sr, 512);
            juce::AudioBuffer<float> buf (p.getTotalNumOutputChannels(), 512); juce::MidiBuffer m; double sum = 0;
            for (int b = 0; b < 200; ++b) { p.processBlock (buf, m); sum += buf.getRMSLevel (0, 0, 512) * (b + 1); }
            return sum;
        };
        const double x = render(), y = render();
        std::printf ("SEED     two renders %s\n", x == y ? "identical" : "differ");
        CHECK (x == y, "same seed gave a different take");
    }

    std::printf (failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
