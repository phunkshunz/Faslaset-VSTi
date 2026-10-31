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

    std::printf ("kickar inlästa: %d\n", proc.numKicks());
    CHECK (proc.numKicks() == kNumKicks);

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

    // ---- varje kick låter
    for (int k = 0; k < proc.numKicks(); ++k)
    {
        if (auto* prm = proc.apvts.getParameter ("kick")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) k));
        double peak = 0.0;
        bool finite = true;
        const double e = runSeconds (proc, host, 0.6, 256, peak, finite);
        CHECK (finite && e > 1.0);
    }

    // ---- tärningar (alla sorter) och ångra
    for (int i = 0; i < 60; ++i)
    {
        switch (i % 6) { case 0: proc.diceAll(); break; case 1: proc.diceBass(); break; case 2: proc.diceNotes(); break;
                         case 3: proc.diceRhythm(); break; case 4: proc.diceSound(); break; default: proc.diceKick(); }
        double peak = 0.0;
        bool finite = true;
        runSeconds (proc, host, 0.5, 512, peak, finite);
        CHECK (finite);
    }
    CHECK (proc.currentPreset.load() == -1);
    CHECK (proc.overlaps() == 0);
    CHECK (proc.faults() == 0);
    {
        const auto before = proc.pattern.get (0, 1).pack() ^ proc.pattern.get (3, 2).pack();
        const float filt = proc.apvts.getRawParameterValue ("b_filter")->load();
        proc.diceAll();
        CHECK (proc.undo());
        CHECK ((proc.pattern.get (0, 1).pack() ^ proc.pattern.get (3, 2).pack()) == before);
        CHECK (std::fabs (proc.apvts.getRawParameterValue ("b_filter")->load() - filt) < 0.5f);
    }

    // ---- state: spara, ändra, ladda tillbaka
    {
        proc.setCurrentProgram (4);
        proc.diceNotes();
        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        const float before = proc.apvts.getRawParameterValue ("b_filter")->load();
        uint32_t pat[24];
        for (int i = 0; i < 24; ++i) pat[i] = proc.pattern.get (i / 3, i % 3).pack();
        proc.setCurrentProgram (9);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());
        CHECK (std::fabs (proc.apvts.getRawParameterValue ("b_filter")->load() - before) < 0.5f);
        for (int i = 0; i < 24; ++i) CHECK (proc.pattern.get (i / 3, i % 3).pack() == pat[i]);
    }

    // ---- stoppad DAW och SPELA av = tystnad
    {
        host.playing = false;
        double peak = 0.0;
        bool finite = true;
        runSeconds (proc, host, 0.1, 256, peak, finite);   // tonar ut
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

    // ---- ljudprov: 8 takter av två presets till WAV (valfritt, argument 2)
    if (argc > 2)
    {
        const double sr = 48000.0;
        juce::File wav { juce::String (argv[2]) };
        wav.deleteFile();
        std::unique_ptr<juce::FileOutputStream> fos (wav.createOutputStream());
        juce::WavAudioFormat fmt;
        std::unique_ptr<juce::AudioFormatWriter> w (fmt.createWriterFor (fos.get(), sr, 2, 24, {}, 0));
        if (w != nullptr)
        {
            fos.release();
            for (int pr : { 0, 10 })
            {
                proc.setCurrentProgram (pr);
                host.ppq = 0.0;
                host.bpm = pr == 0 ? 145.0 : 130.0;
                proc.prepareToPlay (sr, 256);
                juce::AudioBuffer<float> buf (2, 256);
                juce::MidiBuffer midi;
                const int blocks = (int) (8 * 4 * 60.0 / host.bpm * sr / 256);
                for (int b = 0; b < blocks; ++b)
                {
                    proc.processBlock (buf, midi);
                    w->writeFromAudioSampleBuffer (buf, 0, 256);
                    host.ppq += 256 * host.bpm / 60.0 / sr;
                }
            }
            std::printf ("ljudprov skrivet till %s\n", argv[2]);
        }
    }

    if (fails == 0) std::printf ("\nPLUGINTESTET GICK IGENOM.\n");
    else std::printf ("\n%d FEL\n", fails);
    return fails == 0 ? 0 : 1;
}
