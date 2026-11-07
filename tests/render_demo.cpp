// Renders LÄCKAGET demos: ./render_demo outdir
#include "Lackaget.h"
#include "TestLoop.h"
#include <cstdio>
#include <functional>
#include <string>

using namespace lackaget;
using testloop::Stereo;

static constexpr double SR = 48000.0, BPM = 128.0;

static Stereo render (const Stereo& in, const Settings& s, std::function<void (Lackaget&, double)> cue = {})
{
    Lackaget m; m.prepare (SR, 256); m.setSettings (s);
    Stereo o = in;
    for (size_t p = 0; p < in.size(); p += 256)
    {
        const int n = (int) std::min<size_t> (256, in.size() - p);
        if (cue) cue (m, (double) p / SR);
        m.setTransport (BPM, (double) p * BPM / 60.0 / SR, true);
        m.process (o.L.data() + p, o.R.data() + p, n);
    }
    return o;
}

int main (int argc, char** argv)
{
    const std::string dir = argc > 1 ? argv[1] : ".";
    const Stereo loop = testloop::techno (SR, BPM, 15.0);
    testloop::writeWav (dir + "/LACKAGET-0-torr.wav", loop, (int) SR);

    Settings std_;                                                     // standard: Kat 1, R134a
    testloop::writeWav (dir + "/LACKAGET-1-standard.wav", render (loop, std_), (int) SR);

    Settings sl = std_; sl.oljeretur = 0.6f; sl.hunting = 0.4f; sl.ventil = 0.3f; sl.forangare = 0.45f; sl.ton = 0.7f; sl.resonans = 0.4f;
    testloop::writeWav (dir + "/LACKAGET-2-oljeretur-slap.wav", render (loop, sl), (int) SR);

    Settings co2; co2.medium = 1; co2.behorighet = 0.85f; co2.hunting = 0.8f; co2.ventil = 0.6f; co2.forangare = 0.7f; co2.ton = 0.6f;
    co2.resonans = 0.55f; co2.siktglas = 0.45f; co2.last = 0.7f; co2.borvarde = (int) Rate::Sextondel;
    testloop::writeWav (dir + "/LACKAGET-3-R744-larling.wav", render (loop, co2), (int) SR);

    Settings nh3; nh3.medium = 3; nh3.kompressor = 0.85f; nh3.hunting = 0.6f; nh3.behorighet = 0.5f; nh3.forangare = 0.5f; nh3.ton = 0.55f;
    nh3.resonans = 0.6f; nh3.last = 0.8f; nh3.ventil = 0.35f; nh3.siktglas = 0.25f;
    testloop::writeWav (dir + "/LACKAGET-4-R717-ammoniak.wav", render (loop, nh3), (int) SR);

    Settings h2o; h2o.medium = 17; h2o.ventil = 0.8f; h2o.forangare = 0.8f; h2o.ton = 0.45f; h2o.resonans = 0.5f; h2o.hunting = 0.7f;
    h2o.siktglas = 0.3f; h2o.borvarde = (int) Rate::Halv;
    testloop::writeWav (dir + "/LACKAGET-5-R718-vatten.wav", render (loop, h2o), (int) SR);

    Settings stop = std_; stop.stopptid = 0.8f; stop.starttid = 1.2f; stop.kompressor = 0.4f;
    testloop::writeWav (dir + "/LACKAGET-6-nodstopp.wav", render (loop, stop, [] (Lackaget& m, double t) {
        if (t >= 5.0 && t < 5.01) m.nodstopp (true);
        if (t >= 7.0 && t < 7.01) m.nodstopp (false);
        if (t >= 11.25 && t < 11.26) m.nodstopp (true);
        if (t >= 12.0 && t < 12.01) m.nodstopp (false);
    }), (int) SR);
    std::printf ("rendered 7 files to %s\n", dir.c_str());
    return 0;
}
