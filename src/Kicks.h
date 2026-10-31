#pragma once

#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine.h"
#include "BinaryData.h"

namespace fl
{
// BETONG:s 29 kickar, inbakade i pluginet. Avkodas en gång och ändras aldrig sen,
// så ljudtråden kan läsa dem utan lås.
class KickBank
{
public:
    KickBank()
    {
        juce::FlacAudioFormat flac;
        juce::StringArray res;
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            if (juce::String (BinaryData::originalFilenames[i]).startsWithChar ('K'))
                res.add (BinaryData::namedResourceList[i]);
        // sortera efter filnamn (K01, K02 ...)
        std::vector<std::pair<juce::String, juce::String>> files;
        for (auto& r : res)
            files.push_back ({ juce::String (BinaryData::getNamedResourceOriginalFilename (r.toRawUTF8())), r });
        std::sort (files.begin(), files.end());

        for (auto& f : files)
        {
            int size = 0;
            const char* data = BinaryData::getNamedResource (f.second.toRawUTF8(), size);
            if (data == nullptr || size <= 0) continue;
            auto* stream = new juce::MemoryInputStream (data, (size_t) size, false);
            std::unique_ptr<juce::AudioFormatReader> reader (flac.createReaderFor (stream, true));
            if (reader == nullptr || reader->lengthInSamples < 64) continue;

            const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 3.0));
            juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
            reader->read (&buf, 0, len, 0, true, true);

            Kick k;
            k.name = f.first;
            k.sampleRate = reader->sampleRate;
            k.data.assign ((size_t) len, 0.0f);
            for (int ch = 0; ch < buf.getNumChannels(); ++ch)
                for (int i = 0; i < len; ++i)
                    k.data[(size_t) i] += buf.getSample (ch, i) / (float) buf.getNumChannels();
            prepare (k);
            if (! k.data.empty()) kicks.push_back (std::move (k));
        }
    }

    int size() const { return (int) kicks.size(); }
    KickSample get (int i) const
    {
        if (kicks.empty()) return {};
        const auto& k = kicks[(size_t) juce::jlimit (0, (int) kicks.size() - 1, i)];
        return { k.data.data(), (int) k.data.size(), k.sampleRate };
    }

private:
    struct Kick
    {
        juce::String name;
        std::vector<float> data;
        double sampleRate = 48000.0;
    };

    // Gör samplet säkert för faslåsningen: ingen DC, börjar och slutar på noll,
    // samma toppnivå på alla kickar.
    static void prepare (Kick& k)
    {
        auto& d = k.data;
        // DC bort (högpass 8 Hz, en pol) så att svansen verkligen korsar noll
        const double a = std::exp (-kTwoPi * 8.0 / k.sampleRate);
        double x1 = 0.0, y1 = 0.0;
        for (auto& v : d)
        {
            const double y = a * (y1 + (double) v - x1);
            x1 = v;
            y1 = y;
            v = (float) y;
        }
        // tystnad i början bort
        size_t start = 0;
        while (start < d.size() && std::fabs (d[start]) < 1.0e-3f) ++start;
        start = start > 2 ? start - 2 : 0;
        d.erase (d.begin(), d.begin() + (long) start);
        // svansen under -66 dB bort
        float peak = 0.0f;
        for (float v : d) peak = std::max (peak, std::fabs (v));
        if (peak <= 1.0e-6f || d.size() < 64) { d.clear(); return; }
        size_t end = d.size();
        while (end > 64 && std::fabs (d[end - 1]) < peak * 5.0e-4f) --end;
        d.resize (end);
        // normalisera: alla kickar lika feta
        const float g = 0.97f / peak;
        for (auto& v : d) v *= g;
        // mjuk in- och uttoning (8 sampel in, 4 ms ut)
        for (size_t i = 0; i < std::min<size_t> (8, d.size()); ++i) d[i] *= (float) i / 8.0f;
        const size_t fade = std::min<size_t> ((size_t) (0.004 * k.sampleRate), d.size() / 2);
        for (size_t i = 0; i < fade; ++i)
            d[d.size() - 1 - i] *= (float) (0.5 - 0.5 * std::cos (kPi * (double) i / (double) fade));
    }

    std::vector<Kick> kicks;
};
} // namespace fl
