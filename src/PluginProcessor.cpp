#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace fl;

FaslasetProcessor::FaslasetProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "FASLASET", createLayout())
{
    for (int i = 0; i < NUM_P; ++i)
        raw[(size_t) i] = apvts.getRawParameterValue (paramId (i));
    pSpela = apvts.getRawParameterValue ("spela");

    engine.setPattern (&pattern);
    loadPreset (0);
    undoStack.clear();

    // Som fristående app finns ingen DAW: då startar FASLÅSET med egen klocka.
    if (wrapperType == wrapperType_Standalone)
        if (auto* p = apvts.getParameter ("spela"))
            p->setValueNotifyingHost (1.0f);
}

bool FaslasetProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void FaslasetProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = (std::isfinite (sampleRate) && sampleRate > 0.0) ? sampleRate : 48000.0;
    uiSr = sr;
    mono.assign ((size_t) juce::jmax (512, samplesPerBlock), 0.0f);   // enda allokeringen, här och inte i ljudet
    engine.prepare (sr);
    internalPpq = 0.0;
    internalWasOn = false;
}

void FaslasetProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numSamples <= 0 || numCh <= 0) return;
    if (mono.empty()) { buffer.clear(); return; }

    // ---- parametrar -> motorn (motorn sanerar allt själv)
    Params p;
    p.mode = get (PLage) > 0.5f ? Mode::Slap : Mode::Spikrakt;
    p.kickTune = get (PKStam);
    p.kickLengthMs = get (PKLangd);
    p.kickDrive = get (PKDriv);
    p.kickLevel = get (PKNiva);
    p.rootNote = juce::roundToInt (get (PBTon));
    p.scale = juce::roundToInt (get (PBSkala));
    p.beats = juce::roundToInt (get (PBSlag));
    p.bassForm = get (PBForm);
    p.bassCutoff = get (PBFilter);
    p.bassReso = get (PBReso);
    p.bassEnv = get (PBEnv);
    p.bassEnvMs = get (PBEnvTid);
    p.bassDecayMs = get (PBDecay);
    p.bassGate = get (PBLangd);
    p.bassDrive = get (PBBett);
    p.bassLevel = get (PBNiva);
    p.releaseMs = get (PUtkling);
    p.outGain = get (PVolym);
    engine.setParams (p);
    engine.setKick (kicks.get (juce::roundToInt (get (PKick))));

    // ---- klockan: värden om den spelar, annars egen klocka om SPELA är på
    Transport t;
    bool hostPlaying = false;
    double hostBpm = 0.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) hostBpm = *b;
            if (pos->getIsPlaying())
                if (auto q = pos->getPpqPosition())
                {
                    hostPlaying = true;
                    t.ppqAtStart = *q;
                }
        }

    const double tempoParam = get (PTempo);
    if (hostPlaying)
    {
        t.isPlaying = true;
        t.bpm = (std::isfinite (hostBpm) && hostBpm > 0.0) ? hostBpm : tempoParam;
        internalWasOn = false;
    }
    else if (pSpela->load() > 0.5f)
    {
        if (! internalWasOn) internalPpq = 0.0;
        internalWasOn = true;
        t.isPlaying = true;
        t.bpm = tempoParam;
        t.ppqAtStart = internalPpq;
    }
    else
    {
        internalWasOn = false;
        t.bpm = (std::isfinite (hostBpm) && hostBpm > 0.0) ? hostBpm : tempoParam;
    }
    uiBpm = t.bpm;
    uiPlaying = t.isPlaying;

    // ---- ljud, i bitar om värden skickar större block än utlovat
    const int chunkMax = (int) mono.size();
    int done = 0;
    while (done < numSamples)
    {
        const int n = juce::jmin (chunkMax, numSamples - done);
        Transport tc = t;
        tc.ppqAtStart = t.ppqAtStart + (double) done * t.bpm / 60.0 / sr;
        engine.process (mono.data(), n, tc);
        for (int ch = 0; ch < numCh; ++ch)
            buffer.copyFrom (ch, done, mono.data(), n);
        done += n;
    }

    if (t.isPlaying && ! hostPlaying)
        internalPpq += (double) numSamples * t.bpm / 60.0 / sr;
}

// ---------------------------------------------------------------- presets, tärningar, ångra
void FaslasetProcessor::setParam (int id, float value)
{
    if (auto* prm = apvts.getParameter (paramId (id)))
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

uint32_t FaslasetProcessor::newSeed()
{
    return (uint32_t) random.nextInt() ^ (uint32_t) juce::Time::getHighResolutionTicks();
}

void FaslasetProcessor::regenerate (uint32_t seed, bool keepRhythm, bool keepNotes)
{
    BasslineGen::generate (pattern, seed, juce::roundToInt (get (PBSlag)), juce::roundToInt (get (PBSkala)),
                           (BassStyle) juce::roundToInt (get (PBStil)), get (PLage) > 0.5f, keepRhythm, keepNotes);
}

void FaslasetProcessor::loadPreset (int index)
{
    int n = 0;
    const auto* ps = presets (n);
    if (index < 0 || index >= n) return;
    pushUndo();
    for (int i = 0; i < NUM_P; ++i)
        setParam (i, ps[index].v[i]);
    // parametrarna ovan är satta synkront; generera basgången från presetets frö
    BasslineGen::generate (pattern, ps[index].seed, (int) ps[index].v[PBSlag], (int) ps[index].v[PBSkala],
                           (BassStyle) (int) ps[index].v[PBStil], ps[index].v[PLage] > 0.5f);
    currentPreset = index;
}

const juce::String FaslasetProcessor::getProgramName (int index)
{
    int n = 0;
    const auto* ps = presets (n);
    return (index >= 0 && index < n) ? juce::String::fromUTF8 (ps[index].name) : juce::String();
}

juce::String FaslasetProcessor::presetLabel() const
{
    const int i = currentPreset.load();
    if (i < 0) return juce::String::fromUTF8 ("EGEN · TÄRNAD");
    int n = 0;
    const auto* ps = presets (n);
    return i < n ? juce::String::fromUTF8 (ps[i].name) : juce::String();
}

void FaslasetProcessor::diceKick()
{
    pushUndo();
    const bool spik = get (PLage) < 0.5f;
    static const int psyKicks[] = { 0, 2, 3, 6, 9, 12, 14, 18, 22, 24, 26 };
    static const int technoKicks[] = { 1, 4, 5, 7, 8, 10, 11, 13, 15, 16, 17, 19, 21, 23, 27, 28 };
    const int k = spik ? psyKicks[random.nextInt ((int) std::size (psyKicks))]
                       : technoKicks[random.nextInt ((int) std::size (technoKicks))];
    setParam (PKick, (float) k);
    setParam (PKStam, (float) (random.nextInt (7) - 3));
    const float sixteenthMs = 60000.0f / (float) juce::jmax (90.0, uiBpm.load()) / 4.0f;
    setParam (PKLangd, spik ? sixteenthMs * (0.6f + random.nextFloat() * 0.25f) : 110.0f + random.nextFloat() * 90.0f);
    setParam (PKDriv, 1.0f + random.nextFloat() * random.nextFloat() * 3.5f);
    edited();
}

void FaslasetProcessor::diceBass()
{
    pushUndo();
    regenerate (newSeed(), false, false);
    edited();
}

void FaslasetProcessor::diceNotes()
{
    pushUndo();
    regenerate (newSeed(), true, false);
    edited();
}

void FaslasetProcessor::diceRhythm()
{
    pushUndo();
    regenerate (newSeed(), false, true);
    edited();
}

void FaslasetProcessor::diceSound()
{
    pushUndo();
    const bool spik = get (PLage) < 0.5f;
    auto& r = random;
    setParam (PBForm, spik ? 0.5f + r.nextFloat() * 0.5f : r.nextFloat() * 0.4f);
    setParam (PBFilter, spik ? 110.0f + r.nextFloat() * 240.0f : 160.0f + r.nextFloat() * 300.0f);
    setParam (PBReso, r.nextFloat() * (spik ? 0.6f : 0.35f));
    setParam (PBEnv, spik ? 0.4f + r.nextFloat() * 0.6f : r.nextFloat() * 0.5f);
    setParam (PBEnvTid, spik ? 25.0f + r.nextFloat() * 70.0f : 60.0f + r.nextFloat() * 150.0f);
    setParam (PBDecay, spik ? 70.0f + r.nextFloat() * 120.0f : 200.0f + r.nextFloat() * 500.0f);
    setParam (PBLangd, 0.6f + r.nextFloat() * 0.4f);
    setParam (PBBett, 1.0f + r.nextFloat() * r.nextFloat() * 3.5f);
    edited();
}

void FaslasetProcessor::diceAll()
{
    pushUndo();
    auto& r = random;
    setParam (PBTon, (float) (26 + r.nextInt (8)));            // D1 .. A1
    setParam (PBSkala, (float) r.nextInt (kNumScales));
    setParam (PBStil, (float) r.nextInt (kNumStyles));
    static const int lengths[] = { 1, 2, 2, 4, 4, 4, 8, 8 };
    setParam (PBSlag, (float) lengths[r.nextInt (8)]);
    suppressUndo = true;    // ett enda ångra-steg för hela tärningen
    diceKick();
    diceSound();
    suppressUndo = false;
    regenerate (newSeed(), false, false);
    edited();
}

juce::String FaslasetProcessor::patternToString() const
{
    juce::String s;
    for (int b = 0; b < kMaxBeats; ++b)
        for (int k = 0; k < kSlots; ++k)
            s << juce::String::toHexString ((juce::int64) pattern.get (b, k).pack()) << " ";
    return s.trim();
}

void FaslasetProcessor::patternFromString (const juce::String& s)
{
    juce::StringArray words;
    words.addTokens (s, " ", "");
    if (words.size() != kMaxBeats * kSlots) return;
    for (int i = 0; i < words.size(); ++i)
        pattern.set (i / kSlots, i % kSlots, Cell::unpack ((uint32_t) words[i].getHexValue64()));
}

juce::ValueTree FaslasetProcessor::snapshot()
{
    auto state = apvts.copyState();
    state.setProperty ("monster", patternToString(), nullptr);
    state.setProperty ("preset", currentPreset.load(), nullptr);
    return state;
}

void FaslasetProcessor::restore (const juce::ValueTree& tree)
{
    currentPreset = (int) tree.getProperty ("preset", -1);
    if (tree.hasProperty ("monster")) patternFromString (tree.getProperty ("monster").toString());
    apvts.replaceState (tree.createCopy());
}

void FaslasetProcessor::pushUndo()
{
    if (suppressUndo) return;
    undoStack.push_back (snapshot());
    if (undoStack.size() > 40) undoStack.erase (undoStack.begin());
}

bool FaslasetProcessor::undo()
{
    if (undoStack.empty()) return false;
    auto t = undoStack.back();
    undoStack.pop_back();
    restore (t);
    return true;
}

void FaslasetProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = snapshot().createXml()) copyXmlToBinary (*xml, destData);
}

void FaslasetProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            restore (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* FaslasetProcessor::createEditor() { return new FaslasetEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FaslasetProcessor(); }
