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

    loadPreset (0);

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

void FaslasetProcessor::readCurrent (float* v) const
{
    for (int i = 0; i < NUM_P; ++i) v[i] = get (i);
}

void FaslasetProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();
    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();
    if (numSamples <= 0 || numCh <= 0) return;

    // ---- parametrar -> motorn (motorn sanerar allt själv)
    Params p;
    p.mode = get (PLage) > 0.5f ? Mode::Slap : Mode::Spikrakt;
    p.kickStartHz = get (PKStart);
    p.kickEndHz = get (PKSlut);
    p.kickPitchMs = get (PKSvep);
    p.kickLengthMs = get (PKLangd);
    p.kickDrive = get (PKDriv);
    p.kickLevel = get (PKNiva);
    p.bassNote = get (PBTon);
    p.bassForm = get (PBForm);
    p.bassCutoff = get (PBFilter);
    p.bassReso = get (PBReso);
    p.bassEnv = get (PBEnv);
    p.bassEnvMs = get (PBEnvTid);
    p.bassDecayMs = get (PBDecay);
    p.bassDrive = get (PBBett);
    p.bassLevel = get (PBNiva);
    p.guillotineMs = get (PGiljotin);
    p.outGain = get (PVolym);
    engine.setParams (p);

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

    // ---- ljud, i bitar om värden skickar större block än utlovat (ingen allokering här)
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

// ---------------------------------------------------------------- presets och tärningar
void FaslasetProcessor::setParam (int id, float value)
{
    if (auto* prm = apvts.getParameter (paramId (id)))
        prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

void FaslasetProcessor::loadPreset (int index)
{
    int n = 0;
    const auto* ps = presets (n);
    if (index < 0 || index >= n) return;
    for (int i = 0; i < NUM_P; ++i)
        setParam (i, ps[index].v[i]);
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
    float v[NUM_P];
    readCurrent (v);
    juce::Random r;
    DiceRanges::kick (v, r, get (PLage) < 0.5f, (float) uiBpm.load());
    for (int i : { PKStart, PKSlut, PKSvep, PKLangd, PKDriv }) setParam (i, v[i]);
    currentPreset = -1;
}

void FaslasetProcessor::diceBass()
{
    float v[NUM_P];
    readCurrent (v);
    juce::Random r;
    DiceRanges::bass (v, r, get (PLage) < 0.5f);
    for (int i : { PBTon, PBForm, PBFilter, PBReso, PBEnv, PBEnvTid, PBDecay, PBBett }) setParam (i, v[i]);
    currentPreset = -1;
}

void FaslasetProcessor::diceAll()
{
    diceKick();
    diceBass();
}

// ---------------------------------------------------------------- state
void FaslasetProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void FaslasetProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            currentPreset = (int) tree.getProperty ("preset", -1);
            apvts.replaceState (tree);
        }
}

juce::AudioProcessorEditor* FaslasetProcessor::createEditor() { return new FaslasetEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FaslasetProcessor(); }
