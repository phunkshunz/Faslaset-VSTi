// Test av hela pluginet utan DAW: kör processBlock med en låtsas-DAW,
// kontrollerar ljudet, presets, tärningar, state och ritar GUI:t till en PNG.
// Byggs med:  cmake -B build -DFASLASET_TESTS=ON && cmake --build build --target FaslasetTest

#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"

using namespace fl;

static int fails = 0;
#define CHECK(x) do { if (! (x)) { std::printf ("FEL: %s (rad %d)\n", #x, __LINE__); ++fails; } } while (0)

struct FakeHost : juce::AudioPlayHead
{
    double ppq = 0.0, bpm = 145.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setIsPlaying (playing);
        p.setTimeSignature (TimeSignature { 4, 4 });
        return p;
    }
};

static double runSeconds (FaslasetProcessor& proc, FakeHost& host, double seconds, int block, double& peak, bool& finite)
{
    const double sr = 48000.0;
    juce::AudioBuffer<float> buf (2, block);
    juce::MidiBuffer midi;
    double energy = 0.0;
    const int blocks = (int) (seconds * sr / block);
    for (int b = 0; b < blocks; ++b)
    {
        buf.clear();
        proc.processBlock (buf, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < block; ++i)
            {
                const float v = buf.getSample (ch, i);
                finite = finite && std::isfinite (v);
                peak = std::max (peak, (double) std::fabs (v));
                energy += (double) v * v;
            }
        host.ppq += block * host.bpm / 60.0 / sr;
    }
    return energy;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;

    FaslasetProcessor proc;
    FakeHost host;
    proc.setPlayHead (&host);
    proc.setRateAndBufferSizeDetails (48000.0, 256);
    proc.prepareToPlay (48000.0, 256);

    // ---- varje preset låter, utan fel och utan överlapp
    for (int i = 0; i < proc.getNumPrograms(); ++i)
    {
        proc.setCurrentProgram (i);
        double peak = 0.0;
        bool finite = true;
        const double e = runSeconds (proc, host, 3.0, 256, peak, finite);
        std::printf ("%-16s energi %10.1f  topp %.3f\n", proc.getProgramName (i).toRawUTF8(), e, peak);
        CHECK (finite);
        CHECK (peak <= 1.0);
        CHECK (e > 10.0);
    }
    CHECK (proc.overlaps() == 0);
    CHECK (proc.faults() == 0);

    // ---- värden skickar större block än utlovat: ska delas upp, inte krascha
    {
        double peak = 0.0;
        bool finite = true;
        runSeconds (proc, host, 2.0, 2048, peak, finite);
        CHECK (finite);
    }

    // ---- tärningar
    for (int i = 0; i < 50; ++i)
    {
        proc.diceAll();
        double peak = 0.0;
        bool finite = true;
        runSeconds (proc, host, 0.5, 512, peak, finite);
        CHECK (finite);
    }
    CHECK (proc.currentPreset.load() == -1);
    CHECK (proc.overlaps() == 0);

    // ---- state: spara, ändra, ladda tillbaka
    {
        proc.setCurrentProgram (4);
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        const float before = proc.apvts.getRawParameterValue ("b_filter")->load();
        proc.setCurrentProgram (9);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (std::fabs (proc.apvts.getRawParameterValue ("b_filter")->load() - before) < 0.5f);
        CHECK (proc.currentPreset.load() == 4);
    }

    // ---- stoppad DAW och SPELA av = tystnad
    {
        host.playing = false;
        double peak = 0.0;
        bool finite = true;
        const double e = runSeconds (proc, host, 0.5, 256, peak, finite);
        CHECK (e == 0.0);
        host.playing = true;
    }

    // ---- GUI: rita till PNG (med lite ljud i oscilloskopet först)
    {
        proc.setCurrentProgram (0);
        double peak = 0.0;
        bool finite = true;
        std::unique_ptr<juce::AudioProcessorEditor> ed (proc.createEditor());
        runSeconds (proc, host, 0.3, 256, peak, finite);
        for (int i = 0; i < 4; ++i)
        {
            juce::MessageManager::getInstance()->runDispatchLoopUntil (40);
            runSeconds (proc, host, 0.12, 256, peak, finite);
        }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (60);
        if (argc > 1)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::File out { juce::String (argv[1]) };
            out.deleteFile();
            juce::FileOutputStream os (out);
            juce::PNGImageFormat().writeImageToStream (img, os);
            std::printf ("GUI ritat till %s\n", argv[1]);
        }
    }

    if (fails == 0) std::printf ("\nPLUGINTESTET GICK IGENOM.\n");
    else std::printf ("\n%d FEL\n", fails);
    return fails == 0 ? 0 : 1;
}
