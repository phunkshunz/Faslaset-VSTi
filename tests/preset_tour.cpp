// MOTSTÅND — renders a tour of presets through the real plugin to a WAV.
//   MotstandTour out.wav [seconds-per-preset] [doom]
#include "../src/PluginProcessor.h"
#include "../src/Presets.h"
#include <juce_audio_formats/juce_audio_formats.h>

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "motstand_tour.wav");
    const double secs = argc > 2 ? juce::String (argv[2]).getDoubleValue() : 7.5;
    const double sr = 48000; const int bs = 512;
    // one from every category (or the end of the world with "doom"), and a pattern change halfway through each
    const bool doom = argc > 3 && juce::String (argv[3]) == "doom";
    const std::vector<int> tour = doom ? std::vector<int> { 49, 50, 51, 53, 54, 56, 57, 59, 61, 64 }
                                       : std::vector<int> { 1, 7, 13, 19, 25, 31, 37, 43, 48 };

    juce::AudioBuffer<float> all (2, 0);
    for (int idx : tour)
    {
        MotstandProcessor p;
        p.loadPreset (idx);
        auto* pat = p.apvts.getParameter (motstand::pid::pattern);
        p.prepareToPlay (sr, bs);
        juce::AudioBuffer<float> buf (p.getTotalNumOutputChannels(), bs);
        const int blocks = (int) (secs * sr / bs);
        const int start = all.getNumSamples();
        all.setSize (2, start + blocks * bs, true, true);
        for (int b = 0; b < blocks; ++b)
        {
            if (b == blocks / 2) pat->setValueNotifyingHost (pat->convertTo0to1 (7.f));   // to H (peak) on the next bar
            juce::MidiBuffer m; p.processBlock (buf, m);
            for (int c = 0; c < 2; ++c) all.copyFrom (c, start + b * bs, buf, c, 0, bs);
        }
        // short fade at the seams
        const int f = 2400;
        for (int c = 0; c < 2; ++c) { all.applyGainRamp (c, start, f, 0.f, 1.f); all.applyGainRamp (c, all.getNumSamples() - f, f, 1.f, 0.f); }
        std::printf ("%-32s %.1f s\n", motstand::presetName (idx).toRawUTF8(), secs);
    }
    const float peak = all.getMagnitude (0, all.getNumSamples());
    if (peak > 0.f) all.applyGain (0.89f / peak);
    out.deleteFile();
    std::unique_ptr<juce::FileOutputStream> os (out.createOutputStream());
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) return 1;
    os.release();
    w->writeFromAudioSampleBuffer (all, 0, all.getNumSamples());
    std::printf ("wrote %s (%.1f s, raw peak %.2f)\n", out.getFullPathName().toRawUTF8(), all.getNumSamples() / sr, peak);
    return 0;
}
