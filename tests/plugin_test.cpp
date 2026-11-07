// MOTSTÅND — plugin test (runs the real processor without a DAW)
//   cmake -B build -DMOTSTAND_TESTS=ON && cmake --build build --target MotstandTest && ./build/.../MotstandTest
#include "../src/PluginProcessor.h"
#include "../src/Presets.h"
#include "../src/Licens.h"
#include <chrono>
#include <cstdio>
#include <functional>

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

// counts MIDI note-ons for one note (from MIDI out) over a number of seconds
static int countNotes (MotstandProcessor& p, double seconds, int note, std::function<void (int64_t)> onBlock = {})
{
    juce::AudioBuffer<float> buf (p.getTotalNumOutputChannels(), 512);
    int count = 0;
    for (int b = 0; b < (int) (seconds * 48000.0 / 512); ++b)
    {
        if (onBlock) onBlock (p.getStepIndex());
        juce::MidiBuffer m; p.processBlock (buf, m);
        for (const auto meta : m) if (meta.getMessage().isNoteOn() && meta.getMessage().getNoteNumber() == note) ++count;
    }
    return count;
}

static void setP (MotstandProcessor& p, const juce::String& id, float v)
{ auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); }

#if MOTSTAND_DEMO
// ---- the demo build: 8 presets, locked knobs stay put, no MIDI out, still sounds ----
static int demoTests()
{
    const double sr = 48000;
    MotstandProcessor p;
    std::printf ("DEMO     %d programs: ", p.getNumPrograms());
    for (int i = 0; i < p.getNumPrograms(); ++i) std::printf ("%s%s", i ? ", " : "", p.getProgramName (i).toRawUTF8());
    std::printf ("\n");
    CHECK (MotstandProcessor::isDemo, "not a demo build");
    CHECK (p.getNumPrograms() == 8, "demo should have 8 presets (%d)", p.getNumPrograms());
    p.loadPreset (30);
    CHECK (p.currentPreset == 0, "a full-version preset loaded in the demo");
    p.setCurrentProgram (3);
    CHECK (p.currentPreset == MotstandProcessor::demoPresets()[3], "program change in the demo went wrong");

    // a locked knob springs back (also against DAW automation), an open one stays
    setP (p, motstand::pid::jitter, 0.1f);
    setP (p, motstand::pid::master, -6.f);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (400);
    const float jit = p.apvts.getRawParameterValue (motstand::pid::jitter)->load();
    const float mas = p.apvts.getRawParameterValue (motstand::pid::master)->load();
    std::printf ("DEMOLÅS  JITTER set to 0.10 -> %.2f (locked), MASTER set to -6 -> %.1f (open)\n", jit, mas);
    CHECK (std::abs (jit - 0.1f) > 0.05f, "locked JITTER did not spring back");
    CHECK (std::abs (mas + 6.f) < 0.05f, "open MASTER was reset");

    p.prepareToPlay (sr, 512);
    juce::AudioBuffer<float> buf (p.getTotalNumOutputChannels(), 512);
    int notes = 0; double sum = 0;
    for (int b = 0; b < 300; ++b)
    {
        juce::MidiBuffer m; p.processBlock (buf, m);
        for (const auto meta : m) if (meta.getMessage().isNoteOn()) ++notes;
        sum += buf.getRMSLevel (0, 0, 512);
    }
    std::printf ("DEMOLJUD rms %.3f, MIDI notes out: %d\n", sum / 300, notes);
    CHECK (sum / 300 > 0.01, "the demo is silent");
    CHECK (notes == 0, "the demo sends MIDI out");

    juce::MemoryBlock mb; p.getStateInformation (mb);
    MotstandProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
    CHECK (q.currentPreset == 0, "the demo restored a saved state");

    std::printf (failures ? "\n%d FAILED\n" : "\nALL OK (demo)\n", failures);
    return failures ? 1 : 0;
}
#endif

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
   #if MOTSTAND_DEMO
    return demoTests();
   #endif
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
    // 4. Bitwig switches on ALL aux buses by itself: with MULTI UT off everything must stay in MIX
    {
        auto allOn = [] (MotstandProcessor& p)
        {
            auto layout = p.getBusesLayout();
            for (int k = 0; k < motstand::kNumInstruments; ++k)
                layout.outputBuses.getReference (1 + k) = juce::AudioChannelSet::stereo();
            return p.setBusesLayout (layout);
        };
        MotstandProcessor ref; ref.prepareToPlay (sr, 512);
        const auto r0 = run (ref, 6.0);
        MotstandProcessor p;
        CHECK (allOn (p), "could not enable all outputs");
        p.prepareToPlay (sr, 512);
        auto r = run (p, 6.0);
        std::printf ("ALLBUSES multi off: mix rms %.3f (alone %.3f), kick bus %.4f\n", r.mainRms, r0.mainRms, r.auxRms[motstand::KICK]);
        CHECK (r.mainRms > r0.mainRms * 0.9, "drums left the MIX when the DAW enabled the aux buses");
        CHECK (r.auxRms[motstand::KICK] < 1.0e-6, "aux bus got sound with MULTI UT off");

        MotstandProcessor q;
        CHECK (allOn (q), "could not enable all outputs");
        q.apvts.getParameter (motstand::pid::multiOut)->setValueNotifyingHost (1.f);
        q.prepareToPlay (sr, 512);
        auto m = run (q, 6.0);
        std::printf ("MULTIOUT on: kick bus rms %.3f, mix %.3f\n", m.auxRms[motstand::KICK], m.mainRms);
        CHECK (m.auxRms[motstand::KICK] > 0.02, "kick bus is silent with MULTI UT on");
        q.setCurrentProgram (3);
        CHECK (q.apvts.getRawParameterValue (motstand::pid::multiOut)->load() > 0.5f, "preset change switched MULTI UT off");
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

    // 11. CHANS: 0 % never plays, 50 % plays about half the time
    {
        MotstandProcessor p; p.prepareToPlay (sr, 512);
        for (int s : { 0, 4, 8, 12 }) { auto st = p.getCell (0, motstand::KICK, s); st.chance = 0; p.setCell (0, motstand::KICK, s, st); }
        const int never = countNotes (p, 8.0, 36);
        MotstandProcessor q; q.prepareToPlay (sr, 512);
        for (int s : { 0, 4, 8, 12 }) { auto st = q.getCell (0, motstand::KICK, s); st.chance = 50; q.setCell (0, motstand::KICK, s, st); }
        const int half = countNotes (q, 30.0, 36);
        const int full = (int) (30.0 * 133.0 / 60.0);   // kicks in 30 s at 4 per bar
        std::printf ("CHANS    0 %%: %d kicks; 50 %%: %d of ~%d\n", never, half, full);
        CHECK (never == 0, "chance 0 still plays");
        CHECK (half > full * 0.35 && half < full * 0.65, "chance 50 off (%d of %d)", half, full);
    }
    // 12. RULLE: a 4-roll on the first kick gives 7 kicks per bar instead of 4
    {
        MotstandProcessor p; p.prepareToPlay (sr, 512);
        auto st = p.getCell (0, motstand::KICK, 0); st.ratchet = 4; p.setCell (0, motstand::KICK, 0, st);
        const double bars = 6.0, secs = bars * 4.0 * 60.0 / 133.0 - 0.05;
        const int kicks = countNotes (p, secs, 36);
        std::printf ("RULLE    4-roll on step 1: %d kicks in %.0f bars (expect %d)\n", kicks, bars, (int) bars * 7);
        CHECK (std::abs (kicks - (int) bars * 7) <= 2, "ratchet count wrong (%d)", kicks);
    }
    // 13. Pattern change waits for the next bar (C has no kick)
    {
        MotstandProcessor p; p.prepareToPlay (sr, 512);
        int64_t switchedAt = -1;
        const int kicks = countNotes (p, 4.0, 36, [&] (int64_t step) {
            if (switchedAt < 0 && step == 6) { setP (p, motstand::pid::pattern, 2.f); switchedAt = step; } });
        std::printf ("MÖNSTER  switch to C at step 6: %d kicks played (steps 0,4,8,12 -> 4), playing pattern now %c\n", kicks, 'A' + p.playingPattern());
        CHECK (kicks == 4, "pattern switched before the bar ended (%d kicks)", kicks);
        CHECK (p.playingPattern() == 2, "pattern C never took over");
    }
    // 14. FILL: pattern F's snare only plays while FILL is held
    {
        MotstandProcessor p; setP (p, motstand::pid::pattern, 5.f); p.prepareToPlay (sr, 512);
        const int without = countNotes (p, 6.0, 38);
        setP (p, motstand::pid::fill, 1.f);
        const int with = countNotes (p, 6.0, 38);
        std::printf ("FILL     snare notes without FILL: %d, with FILL: %d\n", without, with);
        CHECK (without == 0 && with > 5, "FILL condition wrong");
    }
    // 15. Locks change the sound; the whole bank survives save/load
    {
        auto render = [&] (int lock) {
            MotstandProcessor p; setP (p, motstand::pid::pattern, 6.f);   // G: kick, rim, hats
            for (int s = 0; s < 16; ++s) { auto st = p.getCell (6, motstand::KICK, s); if (st.vel) { st.tune = lock; p.setCell (6, motstand::KICK, s, st); } }
            p.prepareToPlay (sr, 512);
            juce::AudioBuffer<float> buf (p.getTotalNumOutputChannels(), 512); double sum = 0;
            for (int b = 0; b < 100; ++b) { juce::MidiBuffer m; p.processBlock (buf, m); for (int i = 0; i < 512; i += 3) sum += buf.getSample (0, i) * (double) ((i % 7) - 3); }
            return sum;
        };
        const double a = render (0), b = render (24);
        std::printf ("LÅS      tune lock changes the kick: %s\n", a != b ? "yes" : "no");
        CHECK (a != b, "tune lock had no effect");

        MotstandProcessor x; x.prepareToPlay (sr, 512);
        motstand::Step st; st.vel = 99; st.chance = motstand::kChanceFill; st.ratchet = 3; st.tune = -7; st.decay = 11;
        x.setCell (4, motstand::TOM_MID, 9, st);
        juce::MemoryBlock mb; x.getStateInformation (mb);
        MotstandProcessor y; y.setStateInformation (mb.getData(), (int) mb.getSize());
        const auto r = y.getCell (4, motstand::TOM_MID, 9);
        const bool same = r.vel == 99 && r.chance == motstand::kChanceFill && r.ratchet == 3 && r.tune == -7 && r.decay == 11;
        std::printf ("BANK     step with FILL+3-roll+locks in pattern E restored: %s\n", same ? "yes" : "no");
        CHECK (same, "bank state not restored");
        CHECK (y.getCell (3, motstand::TOM_LO, 0).ratchet == 2, "factory pattern D lost");
    }

    // 18. The licence: without a valid key the full version is silent and sends no MIDI
    {
        MotstandProcessor p; p.prepareToPlay (sr, 512);
        licens::upplast().store (false);
        juce::AudioBuffer<float> buf (p.getTotalNumOutputChannels(), 512);
        double sum = 0; int notes = 0;
        for (int b = 0; b < 200; ++b)
        {
            juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 120), 0);
            p.processBlock (buf, m);
            sum += buf.getMagnitude (0, 512);
            for (const auto meta : m) if (meta.getMessage().isNoteOn()) ++notes;
        }
        licens::upplast().store (true);
        auto r = run (p, 2.0);
        std::printf ("LICENS   locked: peak sum %.6f, MIDI out %d; unlocked again: rms %.3f\n", sum, notes, r.mainRms);
        CHECK (sum == 0.0 && notes == 0, "locked full version still makes sound or MIDI");
        CHECK (r.mainRms > 0.01, "unlocking did not bring the sound back");
        CHECK (! licens::giltig ("AAAA-BBBB-CCCC-DDDD") && ! licens::giltig ("") && ! licens::giltig ("hello"), "a made-up key was accepted");
        CHECK (licens::normalisera ("abcd-efgh ijkl-mnop") == "ABCDEFGH1JK1MN0P", "key normalising changed");
    }

    // 17. UNDERGÅNG really drives the machine (regression: the macro used to be overwritten)
    {
        auto collapseWith = [&] (float doom) {
            MotstandProcessor p; setP (p, motstand::pid::doom, doom); p.prepareToPlay (sr, 512);
            juce::AudioBuffer<float> buf (p.getTotalNumOutputChannels(), 512); float mx = 0;
            for (int b = 0; b < 400; ++b) { juce::MidiBuffer m; p.processBlock (buf, m); mx = std::max (mx, p.getCollapse()); }
            return mx;
        };
        const float off = collapseWith (0.f), full = collapseWith (1.f);
        std::printf ("UNDERGÅNG Smog collapse peak: off %.2f, full %.2f\n", off, full);
        CHECK (full > off + 0.2f, "UNDERGÅNG does not reach Detroit Smog");
    }

    // 16. Every preset loads, plays sane, and is the same every time
    {
        double worstLoad = 0; juce::String worstName;
        for (int i = 0; i < motstand::numPresets(); ++i)
        {
            MotstandProcessor p; p.loadPreset (i); p.prepareToPlay (sr, 512);
            const auto t0 = std::chrono::steady_clock::now();
            auto r = run (p, 4.0);
            const double cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / 4.0;
            if (cpu > worstLoad) { worstLoad = cpu; worstName = motstand::presetName (i); }
            CHECK (r.finite && r.peak < 1.6 && r.mainRms > 0.01, "preset %d %s: rms %.3f peak %.2f", i, motstand::presetName (i).toRawUTF8(), r.mainRms, r.peak);
            if (i == 0 || i % 12 == 5)
                std::printf ("PRESET   %-30s rms %.3f peak %.2f\n", motstand::presetName (i).toRawUTF8(), r.mainRms, r.peak);
        }
        std::printf ("PRESETS  %d checked. Heaviest: %s uses %.1f %% of one core in real time\n",
                     motstand::numPresets(), worstName.toRawUTF8(), worstLoad * 100.0);
        MotstandProcessor a, b; a.loadPreset (27); b.loadPreset (27);
        juce::MemoryBlock ma, mb; a.getStateInformation (ma); b.getStateInformation (mb);
        CHECK (ma == mb, "same preset gave a different machine");
        a.loadPreset (0);
        CHECK (a.getCell (3, motstand::TOM_LO, 0).ratchet == 2, "FABRIK did not restore the factory bank");
    }

    std::printf (failures ? "\n%d FAILED\n" : "\nALL OK\n", failures);
    return failures ? 1 : 0;
}
