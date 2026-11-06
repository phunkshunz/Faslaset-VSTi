// MASKINHALLEN : MOTSTÅND — audio processor (step 3)
#include "PluginProcessor.h"

using namespace motstand;

juce::AudioProcessor::BusesProperties MotstandProcessor::makeBuses()
{
    auto b = BusesProperties()
                 .withOutput ("MIX", juce::AudioChannelSet::stereo(), true);
    // One extra (disabled by default) stereo output per instrument.
    // Enabled in the DAW, that voice leaves the MIX and Detroit Smog.
    for (int i = 0; i < kNumInstruments; ++i)
        b = b.withOutput (juce::String (kInstNameEN[i]), juce::AudioChannelSet::stereo(), false);
    return b;
}

MotstandProcessor::MotstandProcessor()
    : AudioProcessor (makeBuses()),
      apvts (*this, nullptr, "MOTSTAND", createLayout())
{
    for (int i = 0; i < kNumInstruments; ++i)
    {
        ip[(size_t) i] = { apvts.getRawParameterValue (pid::inst (i, "tune")),
                           apvts.getRawParameterValue (pid::inst (i, "decay")),
                           apvts.getRawParameterValue (pid::inst (i, "tone")),
                           apvts.getRawParameterValue (pid::inst (i, "level")),
                           apvts.getRawParameterValue (pid::inst (i, "len")),
                           apvts.getRawParameterValue (pid::inst (i, "mute")),
                           apvts.getRawParameterValue (pid::inst (i, "slipMode")),
                           apvts.getRawParameterValue (pid::inst (i, "slipDrive")),
                           apvts.getRawParameterValue (pid::inst (i, "slipFeed")),
                           apvts.getRawParameterValue (pid::inst (i, "slipMix")) };
    }
    pTempoSrc = apvts.getRawParameterValue (pid::tempoSrc);
    pBpm      = apvts.getRawParameterValue (pid::bpm);
    pJitter   = apvts.getRawParameterValue (pid::jitter);
    pDrift    = apvts.getRawParameterValue (pid::drift);
    pNudge    = apvts.getRawParameterValue (pid::nudge);
    pRide     = apvts.getRawParameterValue (pid::ride);
    pSmog     = apvts.getRawParameterValue (pid::smog);
    pSmogThr  = apvts.getRawParameterValue (pid::smogThr);
    pSeed     = apvts.getRawParameterValue (pid::seed);
    pMaster   = apvts.getRawParameterValue (pid::master);
    pElnat    = apvts.getRawParameterValue (pid::elnat);
    pMidiOut  = apvts.getRawParameterValue (pid::midiOut);
    pRomMode  = apvts.getRawParameterValue (pid::romMode);

    std::array<std::array<uint8_t, kMaxSteps>, kNumInstruments> def {};
    defaultPattern (def);
    for (int i = 0; i < kNumInstruments; ++i)
        for (int s = 0; s < kMaxSteps; ++s)
            setStep (i, s, def[(size_t) i][(size_t) s]);
}

bool MotstandProcessor::isBusesLayoutSupported (const BusesLayout& layout) const
{
    if (layout.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    for (int b = 1; b < layout.outputBuses.size(); ++b)
    {
        const auto& set = layout.outputBuses.getReference (b);
        if (! set.isDisabled() && set != juce::AudioChannelSet::stereo() && set != juce::AudioChannelSet::mono())
            return false;
    }
    return layout.inputBuses.isEmpty();
}

void MotstandProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    kit.prepare (sr);
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampler->initProcessing ((size_t) samplesPerBlock);
    smog.prepare (sr * 2.0);
    for (auto& v : slip) v.prepare (sr);
    elnat.prepare (sr);
    // every channel goes through VINKELSLIP's fixed delay, the MIX also through Smog's oversampling
    setLatencySamples (Vinkelslip::kLatency + juce::roundToInt (oversampler->getLatencyInSamples()));
    mixBuffer.setSize (2, samplesPerBlock);
    midiOut.ensureSize (4096);
    pendingOff.fill (0);
    lastSeed = -1;
    reseed();
    wasPlaying = false;
}

void MotstandProcessor::reseed()
{
    const int seed = (int) pSeed->load();
    if (seed == lastSeed) return;
    lastSeed = seed;
    clock.prepare (sr, (uint64_t) seed * 0x9E3779B1ull);
    for (int i = 0; i < kNumInstruments; ++i)
        rider[(size_t) i].prepare (sr, (uint64_t) seed * 131u + (uint64_t) i);
    kit.setTolerance ((uint64_t) seed * 0x2545F491ull);   // this machine's own "factory errors"
}

void MotstandProcessor::pullParameters()
{
    for (int i = 0; i < kNumInstruments; ++i)
    {
        auto& p = kit.params (i);
        const auto& q = ip[(size_t) i];
        p.tune  = q.tune->load();
        p.decay = q.decay->load();
        p.tone  = q.tone->load();
        p.level = q.level->load();
        lanes[(size_t) i].length = juce::jlimit (1, kMaxSteps, (int) q.len->load());
        auto& v = slip[(size_t) i];
        v.mode    = juce::jlimit (0, kNumSlipModes - 1, (int) q.slipMode->load());
        v.drive   = q.slipDrive->load();
        v.matning = q.slipFeed->load();
        v.mix     = q.slipMix->load();
    }
    elnat.amount = pElnat->load();
    kit.setRomBits (pRomMode->load() > 0.5f ? 6 : 0);
    sendMidi = pMidiOut->load() > 0.5f;
    const float jitterMax = pJitter->load();
    clock.jitterMaxMs   = jitterMax;
    clock.jitterSigmaMs = jitterMax * 0.4;   // 1.5 ms max -> 0.6 ms sigma
    clock.driftAmount   = pDrift->load();
    clock.nudge         = pNudge->load();
    smog.amount         = pSmog->load();
    smog.threshold      = pSmogThr->load();
    masterGain          = juce::Decibels::decibelsToGain (pMaster->load());
}

bool MotstandProcessor::triggerVoice (int inst, float vel01, int sampleOffset)
{
    if (ip[(size_t) inst].mute->load() > 0.5f) return false;
    const float amt = kRideBase[inst] * pRide->load();
    auto& r = rider[(size_t) inst];
    const float decayMod = r.onTrigger() * amt;
    const float tuneMod  = r.onTrigger() * amt;
    kit.trigger (inst, vel01, tuneMod, decayMod);
    slip[(size_t) inst].onTrigger (vel01);
    if (! toAux[(size_t) inst])
        smog.addTransient (sampleOffset * 2, vel01);   // smog runs at 2x
    return true;
}

void MotstandProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0) return;
    if (mixBuffer.getNumSamples() < n)          // host broke its promise; stay alive
        mixBuffer.setSize (2, n, false, false, true);
    mixBuffer.clear();

    pullParameters();
    reseed();

    // ---- where does each voice go? ----
    std::array<float*, kNumInstruments> auxL {}, auxR {};
    for (int i = 0; i < kNumInstruments; ++i)
    {
        auto* bus = getBus (false, 1 + i);
        const bool on = bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() > 0;
        toAux[(size_t) i] = on;
        if (on)
        {
            auto bb = getBusBuffer (buffer, false, 1 + i);
            auxL[(size_t) i] = bb.getWritePointer (0);
            auxR[(size_t) i] = bb.getWritePointer (bb.getNumChannels() > 1 ? 1 : 0);
        }
    }

    // ---- transport and tempo ----
    bool playing = true;   // no host transport (standalone): just run
    double hostBpm = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) hostBpm = *b;
        }
    const bool useDaw = pTempoSrc->load() < 0.5f && hostBpm > 1.0;
    clock.baseBpm = useDaw ? hostBpm : (double) pBpm->load();

    if (playing && ! wasPlaying)
        clock.reset();                 // first hit lands on the play press
    wasPlaying = playing;
    running.store (playing);

    StepEvent ev[64];
    const int numEv = playing ? clock.process (n, ev, 64) : 0;
    for (auto& r : rider) r.advance (n);

    // ---- MIDI in (909 GM map) ----
    struct Hit { int off, inst; float vel; };
    std::array<Hit, 128> midiHits; int numMidi = 0;
    for (const auto meta : midi)
    {
        const auto m = meta.getMessage();
        if (m.isNoteOn() && numMidi < (int) midiHits.size())
        {
            const int inst = noteToInstrument (m.getNoteNumber());
            if (inst >= 0)
                midiHits[(size_t) numMidi++] = { juce::jlimit (0, n - 1, meta.samplePosition), inst, m.getFloatVelocity() };
        }
    }

    midiOut.clear();
    const int offLen = juce::jmax (1, (int) (0.03 * sr));     // 30 ms notes
    auto sendOn = [&] (int inst, uint8_t vel, int s)
    {
        if (! sendMidi) return;
        const int note = kInstNote[inst];
        if (pendingOff[(size_t) inst] > 0)                     // retrigger: close the old one first
            midiOut.addEvent (juce::MidiMessage::noteOff (10, note), s);
        midiOut.addEvent (juce::MidiMessage::noteOn (10, note, (juce::uint8) juce::jlimit (1, 127, (int) vel)), s);
        pendingOff[(size_t) inst] = offLen;
    };

    // ---- render ----
    float* mixL = mixBuffer.getWritePointer (0);
    float* mixR = mixBuffer.getWritePointer (1);
    std::array<float, kNumInstruments> gL {}, gR {};
    for (int i = 0; i < kNumInstruments; ++i)
    {   // equal-power pan
        const float a = (pan[(size_t) i] + 1.f) * 0.25f * kPi;
        gL[(size_t) i] = std::cos (a) * 0.7071f;   // centre = 0.5 per side
        gR[(size_t) i] = std::sin (a) * 0.7071f;
    }

    int e = 0, mi = 0;
    for (int s = 0; s < n; ++s)
    {
        while (e < numEv && ev[e].sampleOffset <= s)
        {
            const int64_t step = ev[e].stepIndex;
            for (int i = 0; i < kNumInstruments; ++i)
            {
                const uint8_t v = getStep (i, lanes[(size_t) i].positionAt (step));
                if (v && triggerVoice (i, v / 127.f, s))
                    sendOn (i, v, s);                          // MIDI out carries the Mills timing
            }
            stepNow.store (step);
            ++e;
        }
        while (mi < numMidi && midiHits[(size_t) mi].off <= s)
        {
            triggerVoice (midiHits[(size_t) mi].inst, midiHits[(size_t) mi].vel, s);
            ++mi;
        }

        // note-offs
        for (int i = 0; i < kNumInstruments; ++i)
            if (pendingOff[(size_t) i] > 0 && --pendingOff[(size_t) i] == 0)
                midiOut.addEvent (juce::MidiMessage::noteOff (10, kInstNote[i]), s);

        float o[kNumInstruments];
        kit.process (o);
        for (int i = 0; i < kNumInstruments; ++i)
            o[i] = slip[(size_t) i].process (o[i]);           // VINKELSLIP (fixed latency on every channel)
        const float hum = elnat.process();
        mixL[s] += hum; mixR[s] += hum;
        for (int i = 0; i < kNumInstruments; ++i)
        {
            const float l = o[i] * gL[(size_t) i], r = o[i] * gR[(size_t) i];
            if (toAux[(size_t) i]) { auxL[(size_t) i][s] += l; if (auxR[(size_t) i] != auxL[(size_t) i]) auxR[(size_t) i][s] += r; }
            else                   { mixL[s] += l; mixR[s] += r; }
        }
    }

    // ---- Detroit Smog on the MIX, at 2x ----
    juce::dsp::AudioBlock<float> block (mixBuffer.getArrayOfWritePointers(), 2, (size_t) n);
    auto up = oversampler->processSamplesUp (block);
    smog.process (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples());
    oversampler->processSamplesDown (block);

    auto main = getBusBuffer (buffer, false, 0);
    for (int c = 0; c < main.getNumChannels(); ++c)
        main.addFrom (c, 0, mixBuffer, juce::jmin (c, 1), 0, n, masterGain);   // main was cleared above
    for (int i = 0; i < kNumInstruments; ++i)
        if (toAux[(size_t) i])
            getBusBuffer (buffer, false, 1 + i).applyGain (masterGain);

    // transport stopped or MIDI out switched off: close any hanging notes
    if (! playing || ! sendMidi)
        for (int i = 0; i < kNumInstruments; ++i)
            if (pendingOff[(size_t) i] > 0) { pendingOff[(size_t) i] = 0; midiOut.addEvent (juce::MidiMessage::noteOff (10, kInstNote[i]), n - 1); }
    midi.swapWith (midiOut);

    slipMs.store ((float) clock.slipMs());
    collapse.store (smog.currentCollapse());
    bpmNow.store ((float) clock.currentBpm());
}

// ---- state: parameters + pattern ----
void MotstandProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    juce::ValueTree pat ("PATTERN");
    for (int i = 0; i < kNumInstruments; ++i)
    {
        juce::String hex;
        for (int s = 0; s < kMaxSteps; ++s)
            hex << juce::String::toHexString ((int) getStep (i, s)).paddedLeft ('0', 2);
        pat.setProperty (kInstId[i], hex, nullptr);
    }
    state.removeChild (state.getChildWithName ("PATTERN"), nullptr);
    state.appendChild (pat, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void MotstandProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return;

    auto pat = state.getChildWithName ("PATTERN");
    if (pat.isValid())
    {
        for (int i = 0; i < kNumInstruments; ++i)
        {
            const auto hex = pat.getProperty (kInstId[i]).toString();
            for (int s = 0; s < kMaxSteps && s * 2 + 1 < hex.length(); ++s)
                setStep (i, s, (uint8_t) hex.substring (s * 2, s * 2 + 2).getHexValue32());
        }
        state.removeChild (pat, nullptr);
    }
    apvts.replaceState (state);
}

juce::AudioProcessorEditor* MotstandProcessor::createEditor()
{
    // Temporary until step 4 (the real MOTSTÅND panel).
    return new juce::GenericAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MotstandProcessor();
}
