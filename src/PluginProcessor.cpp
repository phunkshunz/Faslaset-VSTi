#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct Def { const char* id; const char* namn; float def; };
// samma ordning som i Presets.h
const Def kDefs[] = {
    { "tryck", "TRYCK", 0.5f }, { "fart", "FART", 0.5f }, { "rytt", "RYTT", 0.4f },
    { "lacka", "LÄCKA", 0.2f }, { "liv", "LIV", 0.5f }, { "bredd", "BREDD", 0.6f },
    { "anga", "ÅNGA", 0.8f }, { "pys", "PYS", 0.5f }, { "kompressor", "KOMPRESSOR", 0.4f },
    { "knack", "KNACK", 0.5f }, { "klick", "KLICK", 0.3f }, { "skrammel", "SKRAMMEL", 0.2f }, { "vissla", "VISSLA", 0.1f },
    { "ton", "TON", 0.5f }, { "rum", "RUM", 0.3f }, { "volym", "VOLYM", 0.8f },
};
const char* kTypId[5] { "angaTyp", "pysTyp", "knackTyp", "klickTyp", "skrammelTyp" };
const char* kTypRubrik[5] { "ÅNGA TYP", "PYS TYP", "KNACK TYP", "KLICK TYP", "SKRAMMEL TYP" };
}

const juce::StringArray& VevhusProcessor::typNamn (int k)
{
    static const juce::StringArray t[5] {
        juce::StringArray { "Torr", "Bred", juce::String::fromUTF8 ("Våt"), "Tung" },
        juce::StringArray { "Kort", juce::String::fromUTF8 ("Lång"), juce::String::fromUTF8 ("Sprätt"), "Vissel" },
        juce::StringArray { "Vevstake", "Ventil", "Hammare", "Kedja", juce::String::fromUTF8 ("Rör") },
        juce::StringArray { juce::String::fromUTF8 ("Relä"), "Tick", juce::String::fromUTF8 ("Trä"), juce::String::fromUTF8 ("Spärr") },
        juce::StringArray { "Broms", "Koppel", "Droppe" } };
    return t[juce::jlimit (0, 4, k)];
}

juce::AudioProcessorValueTreeState::ParameterLayout VevhusProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& d : kDefs)
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { d.id, 1 },
                                                            juce::String::fromUTF8 (d.namn),
                                                            juce::NormalisableRange<float> (0.0f, 1.0f), d.def));
    for (int k = 0; k < 5; ++k)
        l.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { kTypId[k], 1 },
                                                             juce::String::fromUTF8 (kTypRubrik[k]), typNamn (k), 0));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "avgang", 1 },
                                                       juce::String::fromUTF8 ("AVGÅNG"), false));
    return l;
}

VevhusProcessor::VevhusProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VEVHUS", layout())
{
    setCurrentProgram (0);
}

void VevhusProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

pr::Params VevhusProcessor::las()
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    pr::Params p;
    p.tryck = v ("tryck"); p.fart = v ("fart"); p.rytt = v ("rytt"); p.lacka = v ("lacka"); p.liv = v ("liv"); p.bredd = v ("bredd");
    p.anga = v ("anga"); p.pys = v ("pys"); p.kompressor = v ("kompressor"); p.knack = v ("knack");
    p.klick = v ("klick"); p.skrammel = v ("skrammel");
    p.vissla = v ("vissla"); p.ton = v ("ton"); p.rum = v ("rum"); p.volym = v ("volym");
    p.angaTyp = (int) v ("angaTyp"); p.pysTyp = (int) v ("pysTyp"); p.knackTyp = (int) v ("knackTyp");
    p.klickTyp = (int) v ("klickTyp"); p.skrammelTyp = (int) v ("skrammelTyp");
    p.avgang = v ("avgang") > 0.5f;
    return p;
}

void VevhusProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (buffer.getNumChannels() < 2 || n == 0) return;

    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    bpm = juce::jlimit (30.0, 300.0, bpm);
    if (! playing) ppq = egenPos;
    egenPos = ppq + n * bpm / 60.0 / getSampleRate();

    const auto p = las();
    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (1);
    const double bps = bpm / 60.0 / getSampleRate();

    // Dela blocket vid varje MIDI-not så att handspelade slag hamnar rätt
    int start = 0;
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        const int at = juce::jlimit (0, n, m.samplePosition);
        if (at > start) { engine.process (p, bpm, ppq + start * bps, playing, L + start, R + start, at - start); start = at; }
        if (msg.isNoteOn())
        {
            // C ånga, D pys, E knack, F klick, G skrammel, A knack, B vissla (svarta tangenter: grannen)
            static const int karta[12] { pr::Anga, pr::Anga, pr::Pys, pr::Pys, pr::Knack, pr::Klick,
                                         pr::Klick, pr::Skrammel, pr::Skrammel, pr::Knack, pr::Vissla, pr::Vissla };
            engine.hit (p, karta[msg.getNoteNumber() % 12], msg.getFloatVelocity());
        }
    }
    if (start < n) engine.process (p, bpm, ppq + start * bps, playing, L + start, R + start, n - start);
}

void VevhusProcessor::slumpa()
{
    for (auto& d : kDefs)
    {
        const juce::String id (d.id);
        if (id == "volym") continue;
        float x = 0.15f + 0.75f * slump.nextFloat();
        if (id == "vissla") x *= 0.5f;
        if (id == "lacka" || id == "rum") x *= 0.7f;
        if (auto* prm = apvts.getParameter (d.id)) prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, x));
    }
    for (auto* id : kTypId)
        if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (slump.nextFloat());
}

void VevhusProcessor::setCurrentProgram (int i)
{
    if (! juce::isPositiveAndBelow (i, kNumPresets)) return;
    aktuell = i;
    const auto& ps = kPresets[i];
    for (int k = 0; k < 16; ++k)
        if (auto* prm = apvts.getParameter (kDefs[k].id)) prm->setValueNotifyingHost (ps.v[k]);
    for (int k = 0; k < 5; ++k)
        if (auto* prm = apvts.getParameter (kTypId[k])) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) ps.typ[k]));
    if (auto* prm = apvts.getParameter ("avgang")) prm->setValueNotifyingHost (ps.avgang ? 1.0f : 0.0f);
}

void VevhusProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void VevhusProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* VevhusProcessor::createEditor() { return new VevhusEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new VevhusProcessor(); }
