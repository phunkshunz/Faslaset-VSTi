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

    // 8. MIDI out: every step goes out as a GM 909 note, every note-on gets its note-off
    {
        MotstandProcessor p; p.prepareToPlay (sr, 512);
        juce::AudioBuffer<float> buf (p.getTotalNumOutputChannels(), 512);
        int ons = 0, offs = 0, kicks = 0; bool badNote = false;
        std::array<int, 128> open {};
        for (int b = 0; b < (int) (8.0 * sr / 512); ++b)
        {
            juce::MidiBuffer m; p.processBlock (buf, m);
            for (const auto meta : m)
            {
                const auto msg = meta.getMessage();
                if (msg.isNoteOn()) { ++ons; ++open[(size_t) msg.getNoteNumber()]; if (msg.getNoteNumber() == 36) ++kicks; if (motstand::noteToInstrument (msg.getNoteNumber()) < 0) badNote = true; }
                else if (msg.isNoteOff()) { ++offs; if (open[(size_t) msg.getNoteNumber()] > 0) --open[(size_t) msg.getNoteNumber()]; }
            }
        }
        int hanging = 0; for (int v : open) hanging += v;
        std::printf ("MIDI UT  %d note-on (%d kick), %d note-off, %d hanging at the end\n", ons, kicks, offs, hanging);
        CHECK (ons > 100, "MIDI out sent too few notes");
        CHECK (kicks >= 16, "kick notes missing");
        CHECK (! badNote, "unknown note number sent");
        CHECK (hanging <= motstand::kNumInstruments, "notes left hanging");
        // switch it off: silence on the MIDI port
        p.apvts.getParameter (motstand::pid::midiOut)->setValueNotifyingHost (0.f);
        int after = 0;
        for (int b = 0; b < 200; ++b) { juce::MidiBuffer m; p.processBlock (buf, m); for (const auto meta : m) if (meta.getMessage().isNoteOn()) ++after; }
        CHECK (after == 0, "MIDI UT off still sends notes");
    }
    // 9. VINKELSLIP on every channel at full drive: still sane
    {
        for (int mode = 0; mode < motstand::kNumSlipModes; ++mode)
        {
            MotstandProcessor p;
            for (int i = 0; i < motstand::kNumInstruments; ++i)
            {
                p.apvts.getParameter (motstand::pid::inst (i, "slipDrive"))->setValueNotifyingHost (1.f);
                auto* mp = p.apvts.getParameter (motstand::pid::inst (i, "slipMode"));
                mp->setValueNotifyingHost (mp->convertTo0to1 ((float) mode));
            }
            p.prepareToPlay (sr, 512);
            auto r = run (p, 4.0);
            std::printf ("SLIP     %-10s full drive on all: rms %.3f peak %.2f\n", motstand::kSlipModeName[mode], r.mainRms, r.peak);
            CHECK (r.finite, "VINKELSLIP %s produced NaN", motstand::kSlipModeName[mode]);
            CHECK (r.peak < 1.3, "VINKELSLIP %s too hot (%.2f)", motstand::kSlipModeName[mode], r.peak);
            CHECK (r.mainRms > 0.02, "VINKELSLIP %s went silent", motstand::kSlipModeName[mode]);
        }
        MotstandProcessor p; p.prepareToPlay (sr, 512);
        std::printf ("LATENS   %d samples (%.2f ms)\n", p.getLatencySamples(), p.getLatencySamples() / sr * 1000.0);
    }

    // 10. Studio effects pushed to the limit: still sane. All three kills: silence.
    {
        MotstandProcessor p;
        auto set = [&] (const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
        for (int i = 0; i < motstand::kNumInstruments; ++i) { set (motstand::pid::inst (i, "sendHall"), 1.f); set (motstand::pid::inst (i, "sendEko"), 1.f); }
        set (motstand::pid::ekoFeedback, 1.05f); set (motstand::pid::ekoWear, 1.f); set (motstand::pid::hallDecay, 12.f);
        set (motstand::pid::pressAmt, 1.f); set (motstand::pid::pump, 1.f);
        p.prepareToPlay (sr, 512);
        auto r = run (p, 10.0);
        std::printf ("FX MAX   all sends, max feedback, 12 s hall, full press+pump: rms %.3f peak %.2f, GR %.1f dB\n", r.mainRms, r.peak, p.getPressGr());
        CHECK (r.finite && r.peak < 2.0, "effects at max misbehave (peak %.2f)", r.peak);
        set (motstand::pid::killLow, 1.f); set (motstand::pid::killMid, 1.f); set (motstand::pid::killHigh, 1.f);
        set (motstand::pid::pressAmt, 0.f);
        run (p, 0.5);
        auto k = run (p, 2.0);
        std::printf ("KILL     all three bands killed: rms %.5f\n", k.mainRms);
        CHECK (k.mainRms < 0.003, "kills leak (%.4f)", k.mainRms);
    }

    std::printf (failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
