// MASKINHALLEN : MOTSTÅND — audio processor
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"
#include "Licens.h"

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
                           apvts.getRawParameterValue (pid::inst (i, "slipMix")),
                           apvts.getRawParameterValue (pid::inst (i, "sendHall")),
                           apvts.getRawParameterValue (pid::inst (i, "sendEko")),
                           apvts.getRawParameterValue (pid::inst (i, "model")) };
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
    auto P = [this] (const juce::String& id) { return apvts.getRawParameterValue (id); };
    pHallSize = P (pid::hallSize); pHallDecay = P (pid::hallDecay); pHallDamp = P (pid::hallDamp);
    pHallPre = P (pid::hallPre); pHallLevel = P (pid::hallLevel); pHallFreeze = P (pid::hallFreeze);
    pEkoTime = P (pid::ekoTime); pEkoFb = P (pid::ekoFeedback); pEkoTone = P (pid::ekoTone);
    pEkoWear = P (pid::ekoWear); pEkoLevel = P (pid::ekoLevel); pEkoPing = P (pid::ekoPing);
    pIsoLow = P (pid::isoLow); pIsoMid = P (pid::isoMid); pIsoHigh = P (pid::isoHigh);
    pKillLow = P (pid::killLow); pKillMid = P (pid::killMid); pKillHigh = P (pid::killHigh);
    pPress = P (pid::pressAmt); pPump = P (pid::pump);
    pPattern = P (pid::pattern); pFill = P (pid::fill);
    pMuller = P (pid::mullerAmt); pMullerDepth = P (pid::mullerDepth); pMullerFreq = P (pid::mullerFreq);
    pMullerDirt = P (pid::mullerDirt); pMullerPump = P (pid::mullerPump);
    pMotorGas = P (pid::motorGas); pMotorRpm = P (pid::motorRpm); pMotorTone = P (pid::motorTone); pMotorRattle = P (pid::motorRattle);
    pStorm = P (pid::stormAmt); pStormSweep = P (pid::stormSweep); pStormColor = P (pid::stormColor);
    pAlarm = P (pid::alarm); pDoom = P (pid::doom);

    const auto fb = factoryBank();
    for (int pt = 0; pt < kNumPatterns; ++pt) pastePattern (pt, fb[(size_t) pt]);

   #if MOTSTAND_TEST_UNLOCKED
    licens::upplast().store (true);
   #else
    if (! isDemo) licens::kontrollera();   // fullversionen: finns en giltig nyckel?
   #endif
    if (isDemo)
    {
        loadPreset (0);
        startTimerHz (10);                 // holds the locked parameters, also against DAW automation
    }
}

// ---------------------------------------------------------------- demo
const std::vector<int>& MotstandProcessor::demoPresets()
{
    static const std::vector<int> v = []
    {
        std::vector<int> out { 0 };   // FABRIK
        const char* names[] = { "MOTORSTAD", "BETONGVALV", "TOMRUMMET", "RULLGARDIN", "DIMMA", u8"ÖKENVÄGEN", "PSYKOS" };
        for (auto* nm : names)
            for (int i = 0; i < (int) styles().size(); ++i)
                if (juce::String::fromUTF8 (styles()[(size_t) i].name) == juce::String::fromUTF8 (nm)) { out.push_back (i + 1); break; }
        return out;
    }();
    return v;
}

bool MotstandProcessor::presetAllowed (int index)
{
    if (! isDemo) return true;
    const auto& d = demoPresets();
    return std::find (d.begin(), d.end(), index) != d.end();
}

bool MotstandProcessor::demoOpen (const juce::String& id)
{
    return id == pid::pattern || id == pid::fill || id == pid::nudge || id == pid::master || id == pid::doom
        || id.endsWith ("_level") || id.endsWith ("_mute");
}

void MotstandProcessor::captureDemoLocks()
{
    demoLocked.clear();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! demoOpen (rp->getParameterID()))
                demoLocked.push_back ({ rp->getParameterID(), apvts.getRawParameterValue (rp->getParameterID())->load() });
}

void MotstandProcessor::timerCallback()
{
    // DEMO: locked parameters go back to the preset, even when the DAW automates them
    for (const auto& [id, v] : demoLocked)
        if (auto* raw = apvts.getRawParameterValue (id); raw != nullptr && std::abs (raw->load() - v) > 1.0e-4f)
            if (auto* rp = apvts.getParameter (id))
                rp->setValueNotifyingHost (rp->convertTo0to1 (v));
}

PatternData MotstandProcessor::copyPattern (int pat) const
{
    PatternData d {};
    for (int i = 0; i < kNumInstruments; ++i)
        for (int s = 0; s < kMaxSteps; ++s)
            d[(size_t) i][(size_t) s] = bank[(size_t) pat][(size_t) i][(size_t) s].load (std::memory_order_relaxed);
    return d;
}

void MotstandProcessor::pastePattern (int pat, const PatternData& d)
{
    for (int i = 0; i < kNumInstruments; ++i)
        for (int s = 0; s < kMaxSteps; ++s)
            bank[(size_t) pat][(size_t) i][(size_t) s].store (d[(size_t) i][(size_t) s], std::memory_order_relaxed);
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
    hall.prepare (sr); eko.prepare (sr); iso.prepare (sr); press.prepare (sr); pump.prepare (sr);
    muller.prepare (sr); motor.prepare (sr); storm.prepare (sr);
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
    chanceRng.setSeed ((uint64_t) seed * 7919u + 17u);   // same seed = same dice for CHANS
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
        p.model = juce::jlimit (0, kNumModels - 1, (int) q.model->load());
        lanes[(size_t) i].length = juce::jlimit (1, kMaxSteps, (int) q.len->load());
        auto& v = slip[(size_t) i];
        v.mode    = juce::jlimit (0, kNumSlipModes - 1, (int) q.slipMode->load());
        v.drive   = q.slipDrive->load();
        v.matning = q.slipFeed->load();
        v.mix     = q.slipMix->load();
        sendH[(size_t) i] = q.sendHall->load();
        sendE[(size_t) i] = q.sendEko->load();
    }
    hall.size = pHallSize->load(); hall.decay = pHallDecay->load(); hall.damping = pHallDamp->load();
    hall.predelayMs = pHallPre->load(); hall.freeze = pHallFreeze->load() > 0.5f; hallLevel = pHallLevel->load();
    eko.division = (int) pEkoTime->load(); eko.feedback = pEkoFb->load(); eko.tone = pEkoTone->load();
    eko.wear = pEkoWear->load(); eko.pingPong = pEkoPing->load() > 0.5f; ekoLevel = pEkoLevel->load();
    iso.low  = isoGain (pIsoLow->load(),  pKillLow->load()  > 0.5f);
    iso.mid  = isoGain (pIsoMid->load(),  pKillMid->load()  > 0.5f);
    iso.high = isoGain (pIsoHigh->load(), pKillHigh->load() > 0.5f);
    press.amount = pPress->load(); pump.amount = pPump->load();

    muller.amount = pMuller->load(); muller.depth = pMullerDepth->load(); muller.freq = pMullerFreq->load();
    muller.dirt = pMullerDirt->load(); muller.pump = pMullerPump->load();
    motor.gas = pMotorGas->load(); motor.rpm = pMotorRpm->load(); motor.tone = pMotorTone->load(); motor.rattle = pMotorRattle->load();
    storm.amount = pStorm->load(); storm.sweep = pStormSweep->load(); storm.color = pStormColor->load(); storm.alarm = pAlarm->load();

    elnat.amount = pElnat->load();
    kit.setRomBits (pRomMode->load() > 0.5f ? 6 : 0);
    sendMidi = ! isDemo && pMidiOut->load() > 0.5f;   // MIDI out is a full-version feature
    const float jitterMax = pJitter->load();
    clock.jitterMaxMs   = jitterMax;
    clock.jitterSigmaMs = jitterMax * 0.4;   // 1.5 ms max -> 0.6 ms sigma
    clock.driftAmount   = pDrift->load();
    clock.nudge         = pNudge->load();
    smog.amount         = pSmog->load();
    smog.threshold      = pSmogThr->load();
    masterGain          = juce::Decibels::decibelsToGain (pMaster->load());

    // UNDERGÅNG: one knob that drags the whole machine towards the end of the world
    if (const float m = pDoom->load(); m > 0.f)
    {
        // heavy, not noisy: weight from the rumble and the bus, only a little grind and sand
        smog.amount  = std::max (smog.amount, m * 0.6f);
        smog.threshold = std::min (smog.threshold, 3.5f - m * 0.8f);
        press.amount = juce::jlimit (0.f, 1.f, press.amount + m * 0.5f);
        pump.amount  = std::max (pump.amount, m * 0.45f);
        muller.amount = std::max (muller.amount, m * 0.7f);
        muller.dirt   = std::max (muller.dirt, m * 0.5f);
        storm.amount  = std::max (storm.amount, m * 0.12f);
        for (auto& v : slip) v.drive = juce::jlimit (0.f, 1.f, v.drive + m * 0.25f);
        masterGain *= 1.f + m * 0.7f;      // the squeeze costs level; give it back (about +4.6 dB at full)
    }
}

bool MotstandProcessor::triggerVoice (int inst, float vel01, int sampleOffset, int tuneLock, int decayLock)
{
    if (ip[(size_t) inst].mute->load() > 0.5f) return false;
    const float amt = kRideBase[inst] * pRide->load();
    auto& r = rider[(size_t) inst];
    const float decayMod = r.onTrigger() * amt;
    const float tuneMod  = r.onTrigger() * amt;
    // step locks: this hit only gets its own tune/decay (2 % of the knob per lock step)
    auto& vp = kit.params (inst);
    const float keepTune = vp.tune, keepDecay = vp.decay;
    vp.tune  = juce::jlimit (0.f, 1.f, keepTune  + 0.02f * (float) tuneLock);
    vp.decay = juce::jlimit (0.f, 1.f, keepDecay + 0.02f * (float) decayLock);
    kit.trigger (inst, vel01, tuneMod, decayMod);
    vp.tune = keepTune; vp.decay = keepDecay;
    slip[(size_t) inst].onTrigger (vel01);
    if (inst == KICK) { pump.trigger (vel01); muller.onKick (vel01); }
    if (! toAux[(size_t) inst])
        smog.addTransient (sampleOffset * 2, vel01);   // smog runs at 2x
    return true;
}

void MotstandProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    buffer.clear();
    // fullversionen utan giltig nyckel: tyst, och inget MIDI ut (nyckelrutan syns i panelen)
    if (! isDemo && ! licens::upplast().load (std::memory_order_relaxed)) { midi.clear(); return; }
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

    const int wantPat = viewPattern();
    const bool fill = pFill->load() > 0.5f;
    if (playing && ! wasPlaying)
    {
        clock.reset();                 // first hit lands on the play press
        activePat.store (wantPat);
    }
    if (! playing) { activePat.store (wantPat); numRolls = 0; }
    wasPlaying = playing;
    running.store (playing);

    eko.bpm = clock.currentBpm();          // the echo follows the machine's own drifting tempo
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
            if (step % 16 == 0 && wantPat != activePat.load())
                activePat.store (wantPat);                     // pattern changes land on the next bar
            const int pat = activePat.load();
            const float stepLen = (float) (60.0 / clock.currentBpm() / 4.0 * sr);
            for (int i = 0; i < kNumInstruments; ++i)
            {
                const auto st = getCell (pat, i, lanes[(size_t) i].positionAt (step));
                if (! st.plays ((float) chanceRng.uniform(), fill)) continue;
                if (triggerVoice (i, st.vel / 127.f, s, st.tune, st.decay))
                    sendOn (i, (uint8_t) st.vel, s);           // MIDI out carries the Mills timing
                // RULLE: the extra hits of a roll, spread evenly inside the step
                for (int k = 1; k < st.ratchet && numRolls < (int) rolls.size(); ++k)
                    rolls[(size_t) numRolls++] = { (int) std::lround (k * stepLen / st.ratchet), i,
                                                   juce::jmax (1, (int) std::lround (st.vel * std::pow (0.9f, (float) k))), st.tune, st.decay };
            }
            stepNow.store (step);
            ++e;
        }
        while (mi < numMidi && midiHits[(size_t) mi].off <= s)
        {
            triggerVoice (midiHits[(size_t) mi].inst, midiHits[(size_t) mi].vel, s);
            ++mi;
        }

        // rolls that are due now
        for (int r = 0; r < numRolls;)
        {
            auto& ro = rolls[(size_t) r];
            if (ro.wait <= 0)
            {
                if (triggerVoice (ro.inst, ro.vel / 127.f, s, ro.tune, ro.decay)) sendOn (ro.inst, (uint8_t) ro.vel, s);
                ro = rolls[(size_t) --numRolls];
            }
            else { --ro.wait; ++r; }
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
        const float pg = pump.next();           // ducks everything except the kick
        float hIL = 0.f, hIR = 0.f, eIL = 0.f, eIR = 0.f;
        for (int i = 0; i < kNumInstruments; ++i)
        {
            const float duck = i == KICK ? 1.f : pg;
            const float l = o[i] * gL[(size_t) i] * duck, r = o[i] * gR[(size_t) i] * duck;
            if (toAux[(size_t) i]) { auxL[(size_t) i][s] += l; if (auxR[(size_t) i] != auxL[(size_t) i]) auxR[(size_t) i][s] += r; }
            else                   { mixL[s] += l; mixR[s] += r; }
            hIL += l * sendH[(size_t) i]; hIR += r * sendH[(size_t) i];
            eIL += l * sendE[(size_t) i]; eIR += r * sendE[(size_t) i];
        }
        // UNDERGÅNG layers: rumble from the kick, the V8, the sandstorm, the siren in the distance
        const float rumble = muller.process (o[KICK]);
        float motL, motR, stL, stR, siren;
        motor.process (motL, motR);
        storm.process (stL, stR, siren);
        mixL[s] += rumble + (motL + stL + siren * 0.35f) * pg;
        mixR[s] += rumble + (motR + stR + siren * 0.35f) * pg;
        hIL += siren * 0.8f; hIR += siren * 0.8f;          // the siren is far away: mostly hall

        float hl, hr, el, er;
        hall.process (hIL, hIR, hl, hr);
        eko.process (eIL, eIR, el, er);
        mixL[s] += (hl * hallLevel + el * ekoLevel) * pg;
        mixR[s] += (hr * hallLevel + er * ekoLevel) * pg;
    }

    // ---- Detroit Smog on the MIX, at 2x ----
    juce::dsp::AudioBlock<float> block (mixBuffer.getArrayOfWritePointers(), 2, (size_t) n);
    auto up = oversampler->processSamplesUp (block);
    smog.process (up.getChannelPointer (0), up.getChannelPointer (1), (int) up.getNumSamples());
    oversampler->processSamplesDown (block);

    // ---- PRESS then ISOLATOR (the DJ mixer sits last, like Mills' setup) ----
    for (int s = 0; s < n; ++s)
    {
        float l = mixL[s], r = mixR[s];
        press.process (l, r);
        iso.process (l, r);
        mixL[s] = l; mixR[s] = r;
    }
    pressGr.store (press.gainReductionDb());

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
    juce::ValueTree bk ("BANK");
    for (int pt = 0; pt < kNumPatterns; ++pt)
        for (int i = 0; i < kNumInstruments; ++i)
        {
            juce::String hex;
            for (int s = 0; s < kMaxSteps; ++s)
                hex << juce::String::toHexString ((juce::int64) bank[(size_t) pt][(size_t) i][(size_t) s].load()).paddedLeft ('0', 8);
            bk.setProperty ("p" + juce::String (pt) + "_" + kInstId[i], hex, nullptr);
        }
    state.removeChild (state.getChildWithName ("BANK"), nullptr);
    state.removeChild (state.getChildWithName ("PATTERN"), nullptr);
    state.appendChild (bk, nullptr);
    state.setProperty ("preset", currentPreset, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void MotstandProcessor::setStateInformation (const void* data, int size)
{
    if (isDemo) return;   // demon sparar inget: den startar alltid på FABRIK
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.hasType (apvts.state.getType())) return;

    auto bk = state.getChildWithName ("BANK");
    if (bk.isValid())
    {
        for (int pt = 0; pt < kNumPatterns; ++pt)
            for (int i = 0; i < kNumInstruments; ++i)
            {
                const auto hex = bk.getProperty ("p" + juce::String (pt) + "_" + kInstId[i]).toString();
                for (int s = 0; s < kMaxSteps; ++s)
                    bank[(size_t) pt][(size_t) i][(size_t) s].store (s * 8 + 8 <= hex.length()
                        ? (uint32_t) hex.substring (s * 8, s * 8 + 8).getHexValue64() : Step::rest());
            }
        state.removeChild (bk, nullptr);
    }
    auto pat = state.getChildWithName ("PATTERN");   // older projects: velocities only, into A
    if (pat.isValid())
    {
        for (int i = 0; i < kNumInstruments; ++i)
        {
            const auto hex = pat.getProperty (kInstId[i]).toString();
            for (int s = 0; s < kMaxSteps && s * 2 + 1 < hex.length(); ++s)
            {
                Step st; st.vel = hex.substring (s * 2, s * 2 + 2).getHexValue32();
                setCell (0, i, s, st);
            }
        }
        state.removeChild (pat, nullptr);
    }
    currentPreset = juce::jlimit (0, numPresets() - 1, (int) state.getProperty ("preset", 0));
    apvts.replaceState (state);
}

// ---- presets ----
// in the demo, the DAW's program list only shows the demo presets
int MotstandProcessor::getNumPrograms() { return isDemo ? (int) demoPresets().size() : numPresets(); }

const juce::String MotstandProcessor::getProgramName (int index)
{
    if (isDemo) return presetName (demoPresets()[(size_t) juce::jlimit (0, (int) demoPresets().size() - 1, index)]);
    return presetName (index);
}

int MotstandProcessor::getCurrentProgram()
{
    if (! isDemo) return currentPreset;
    const auto& d = demoPresets();
    const auto it = std::find (d.begin(), d.end(), currentPreset);
    return it == d.end() ? 0 : (int) (it - d.begin());
}

void MotstandProcessor::setCurrentProgram (int index)
{
    if (isDemo) loadPreset (demoPresets()[(size_t) juce::jlimit (0, (int) demoPresets().size() - 1, index)]);
    else loadPreset (index);
}

void MotstandProcessor::loadPreset (int index)
{
    index = juce::jlimit (0, numPresets() - 1, index);
    if (! presetAllowed (index)) return;
    currentPreset = index;
    demoLocked.clear();   // let the preset set everything, then lock again
    // everything back to factory defaults first (the panel language is not a parameter, it stays)
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->getParameterID() != pid::fill)
            { rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->getDefaultValue()); rp->endChangeGesture(); }
    if (index == 0)
    {
        const auto fb = factoryBank();
        for (int pt = 0; pt < kNumPatterns; ++pt) pastePattern (pt, fb[(size_t) pt]);
        if (isDemo) captureDemoLocks();
        return;
    }
    auto set = [this] (const juce::String& id, float plain)
    {
        if (auto* rp = apvts.getParameter (id))
        { rp->beginChangeGesture(); rp->setValueNotifyingHost (rp->convertTo0to1 (plain)); rp->endChangeGesture(); }
    };
    const auto generated = applyStyle (styles()[(size_t) index - 1], set);
    for (int pt = 0; pt < kNumPatterns; ++pt) pastePattern (pt, generated[(size_t) pt]);
    if (isDemo) captureDemoLocks();
}

juce::AudioProcessorEditor* MotstandProcessor::createEditor()
{
    return new MotstandEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MotstandProcessor();
}
