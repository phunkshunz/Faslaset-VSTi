#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
struct Def { const char* id; const char* namn; float def; };
const Def kDefs[] = {
    { "tryck", "TRYCK", 0.5f }, { "fart", "FART", 0.5f }, { "rytt", "RYTT", 0.4f },
    { "lacka", "LÄCKA", 0.2f }, { "liv", "LIV", 0.5f },
    { "anga", "ÅNGA", 0.8f }, { "pys", "PYS", 0.5f }, { "kompressor", "KOMPRESSOR", 0.4f },
    { "knack", "KNACK", 0.5f }, { "vissla", "VISSLA", 0.2f },
    { "ton", "TON", 0.5f }, { "rum", "RUM", 0.3f }, { "volym", "VOLYM", 0.8f },
};
}

juce::AudioProcessorValueTreeState::ParameterLayout VevhusProcessor::layout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& d : kDefs)
        l.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { d.id, 1 },
                                                            juce::String::fromUTF8 (d.namn),
                                                            juce::NormalisableRange<float> (0.0f, 1.0f), d.def));
    l.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "avgang", 1 },
                                                       juce::String::fromUTF8 ("AVGÅNG"), false));
    return l;
}

VevhusProcessor::VevhusProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "VEVHUS", layout())
{
}

void VevhusProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

pr::Params VevhusProcessor::las()
{
    auto v = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };
    pr::Params p;
    p.tryck = v ("tryck"); p.fart = v ("fart"); p.rytt = v ("rytt"); p.lacka = v ("lacka"); p.liv = v ("liv");
    p.anga = v ("anga"); p.pys = v ("pys"); p.kompressor = v ("kompressor"); p.knack = v ("knack");
    p.vissla = v ("vissla"); p.ton = v ("ton"); p.rum = v ("rum"); p.volym = v ("volym");
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
            static const int karta[12] { pr::Anga, pr::Anga, pr::Pys, pr::Pys, pr::Knack, pr::Vissla,
                                         pr::Anga, pr::Pys, pr::Pys, pr::Knack, pr::Knack, pr::Vissla };
            engine.hit (p, karta[msg.getNoteNumber() % 12], msg.getFloatVelocity());
        }
    }
    if (start < n) engine.process (p, bpm, ppq + start * bps, playing, L + start, R + start, n - start);
}

void VevhusProcessor::slumpa()
{
    for (auto& d : kDefs)
    {
        if (juce::String (d.id) == "volym") continue;
        if (auto* prm = apvts.getParameter (d.id))
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, 0.15f + 0.75f * slump.nextFloat()));
    }
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
