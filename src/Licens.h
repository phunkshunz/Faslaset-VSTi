#pragma once

// MASKINHALLEN · licens
//
// Samma fil i alla maskiner. Det enda som skiljer är LicensPool.h, som har
// maskinens egna nycklar (som hashar) och bundle-nycklarna.
//
// Så funkar det:
//  - Köparen får en nyckel från Payhip, t.ex. 7KQ2-M9XA-4TRD-HB3V.
//  - Nyckeln klistras in en gång, i vilken maskin som helst. Den sparas i den
//    gemensamma filen MASKINHALLEN.settings (samma fil som SV | EN), så alla
//    maskiner på datorn hittar den.
//  - En bundle-nyckel låser upp alla maskiner. En maskin-nyckel låser upp sin maskin.
//  - Pluginet innehåller bara SHA-256-hashar av nycklarna, inte nycklarna själva,
//    så det går inte att räkna fram nya nycklar ur pluginet.
//  - Utan giltig nyckel är fullversionen tyst (instrument) eller rakt igenom (effekt),
//    och en ruta ber om nyckeln. Demoversionerna rörs aldrig.

#include <juce_gui_basics/juce_gui_basics.h>
#include <atomic>
#include <array>
#include <cstring>
#include "Sprak.h"
#include "LicensPool.h"

namespace licens
{
// ---------------------------------------------------------------- SHA-256
struct Sha256
{
    static std::array<uint8_t, 32> of (const uint8_t* data, size_t len)
    {
        static const uint32_t k[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };
        uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };

        auto rotr = [] (uint32_t x, int n) { return (x >> n) | (x << (32 - n)); };
        const uint64_t bitLen = (uint64_t) len * 8;
        const size_t total = ((len + 9 + 63) / 64) * 64;
        std::vector<uint8_t> m (total, 0);
        if (len > 0) std::memcpy (m.data(), data, len);
        m[len] = 0x80;
        for (int i = 0; i < 8; ++i) m[total - 1 - (size_t) i] = (uint8_t) (bitLen >> (8 * i));

        for (size_t off = 0; off < total; off += 64)
        {
            uint32_t w[64];
            for (int i = 0; i < 16; ++i)
                w[i] = (uint32_t) m[off + (size_t) i * 4] << 24 | (uint32_t) m[off + (size_t) i * 4 + 1] << 16
                     | (uint32_t) m[off + (size_t) i * 4 + 2] << 8 | (uint32_t) m[off + (size_t) i * 4 + 3];
            for (int i = 16; i < 64; ++i)
            {
                const uint32_t s0 = rotr (w[i - 15], 7) ^ rotr (w[i - 15], 18) ^ (w[i - 15] >> 3);
                const uint32_t s1 = rotr (w[i - 2], 17) ^ rotr (w[i - 2], 19) ^ (w[i - 2] >> 10);
                w[i] = w[i - 16] + s0 + w[i - 7] + s1;
            }
            uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
            for (int i = 0; i < 64; ++i)
            {
                const uint32_t S1 = rotr (e, 6) ^ rotr (e, 11) ^ rotr (e, 25);
                const uint32_t ch = (e & f) ^ (~e & g);
                const uint32_t t1 = hh + S1 + ch + k[i] + w[i];
                const uint32_t S0 = rotr (a, 2) ^ rotr (a, 13) ^ rotr (a, 22);
                const uint32_t mj = (a & b) ^ (a & c) ^ (b & c);
                const uint32_t t2 = S0 + mj;
                hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
            }
            h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
        }
        std::array<uint8_t, 32> out {};
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 4; ++j) out[(size_t) (i * 4 + j)] = (uint8_t) (h[i] >> (24 - 8 * j));
        return out;
    }
};

// ---------------------------------------------------------------- nycklar
// Versaler, bara 0-9 och A-Z, utan bindestreck och mellanslag. I och L blir 1, O blir 0
// (samma alfabet som nycklarna skapas med, så att felläsningar ändå fungerar).
inline juce::String normalisera (const juce::String& key)
{
    juce::String out;
    for (auto c : key.toUpperCase())
    {
        if (c == 'I' || c == 'L') c = '1';
        else if (c == 'O') c = '0';
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) out << juce::String::charToString (c);
    }
    return out;
}

inline bool iPool (const std::array<uint8_t, 32>& h, const uint8_t* pool, int n)
{
    for (int i = 0; i < n; ++i)
        if (std::memcmp (pool + (size_t) i * kHashBytes, h.data(), (size_t) kHashBytes) == 0) return true;
    return false;
}

inline bool giltig (const juce::String& key)
{
    const auto norm = normalisera (key);
    if (norm.length() != 16) return false;
    const auto text = juce::String ("MASKINHALLEN-LICENS-v1:") + norm;
    const auto utf8 = text.toStdString();
    const auto h = Sha256::of ((const uint8_t*) utf8.data(), utf8.size());
    return iPool (h, kPool, kPoolN) || iPool (h, kBundle, kBundleN);
}

// Upplåst eller inte. Läses av ljudtråden, så bara en atomic.
inline std::atomic<bool>& upplast()
{
    static std::atomic<bool> u { false };
    return u;
}

// Läser de sparade nycklarna och ser om någon passar den här maskinen.
inline bool kontrollera()
{
    juce::PropertiesFile f (sprak::installningar());
    const auto keys = juce::StringArray::fromLines (f.getValue ("licenser"));
    bool ok = false;
    for (const auto& k : keys) if (k.isNotEmpty() && giltig (k)) { ok = true; break; }
    upplast().store (ok);
    return ok;
}

// Sparar en ny nyckel om den är giltig. Gamla nycklar ligger kvar (andra maskiner kan behöva dem).
inline bool laggTill (const juce::String& key)
{
    if (! giltig (key)) return false;
    juce::PropertiesFile f (sprak::installningar());
    auto keys = juce::StringArray::fromLines (f.getValue ("licenser"));
    keys.removeEmptyStrings();
    keys.addIfNotAlreadyThere (normalisera (key));
    f.setValue ("licenser", keys.joinIntoString ("\n"));
    f.saveIfNeeded();
    upplast().store (true);
    return true;
}

inline juce::String sv_en (const char* sv, const char* en) { return juce::String::fromUTF8 (sprak::engelska() ? en : sv); }

// ---------------------------------------------------------------- rutan
// Läggs överst i pluginfönstret. Syns bara i fullversionen när ingen giltig nyckel finns.
class Ruta : public juce::Component, private juce::Timer
{
public:
    Ruta()
    {
        falt.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 18.0f, juce::Font::bold));
        falt.setJustification (juce::Justification::centred);
        falt.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff0b0c0d));
        falt.setColour (juce::TextEditor::textColourId, juce::Colour (0xfff2f4f6));
        falt.setColour (juce::TextEditor::outlineColourId, juce::Colour (0xff3a3e45));
        falt.setColour (juce::TextEditor::focusedOutlineColourId, accent);
        falt.setTextToShowWhenEmpty ("XXXX-XXXX-XXXX-XXXX", juce::Colour (0xff5e636a));
        falt.onReturnKey = [this] { forsok(); };
        addAndMakeVisible (falt);

        lasUpp.setColour (juce::TextButton::buttonColourId, accent);
        lasUpp.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        lasUpp.onClick = [this] { forsok(); };
        addAndMakeVisible (lasUpp);

        kop.onClick = [] { juce::URL (kKopUrl).launchInDefaultBrowser(); };
        demo.onClick = [] { juce::URL (kDemoUrl).launchInDefaultBrowser(); };
        for (auto* b : { &kop, &demo })
        {
            b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c1e22));
            b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd5d9dd));
            addAndMakeVisible (*b);
        }
        texter();
        setVisible (! upplast().load());
        startTimerHz (1);
    }

    std::function<void()> onUpplast;

    void texter()
    {
        lasUpp.setButtonText (sv_en ("LÅS UPP", "UNLOCK"));
        kop.setButtonText (sv_en ("Köp nyckel", "Buy a key"));
        demo.setButtonText (sv_en ("Gratis demo", "Free demo"));
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xf00e0f11));
        const auto k = kort.toFloat();
        g.setColour (juce::Colour (0xff17181b));
        g.fillRoundedRectangle (k, 14.0f);
        g.setColour (accent.withAlpha (0.8f));
        g.drawRoundedRectangle (k.reduced (0.5f), 14.0f, 1.5f);

        auto r = kort.reduced (28, 22);
        g.setColour (accent);
        g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
        g.drawText (sv_en ("LÅS UPP ", "UNLOCK ") + juce::String::fromUTF8 (kProdukt), r.removeFromTop (38), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffd5d9dd));
        g.setFont (juce::FontOptions (14.0f));
        g.drawFittedText (sv_en ("Klistra in nyckeln från ditt köp. Det räcker att göra det en gång: alla MASKINHALLEN-maskiner på datorn hittar den. En bundle-nyckel låser upp allt.",
                                 "Paste the key from your purchase. You only need to do this once: every MASKINHALLEN machine on this computer will find it. A bundle key unlocks everything."),
                          r.removeFromTop (60), juce::Justification::topLeft, 3);
        g.setColour (fel ? juce::Colour (0xffff6b5b) : juce::Colour (0xff8a9097));
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (status, statusYta, juce::Justification::centred);
    }

    void resized() override
    {
        kort = getLocalBounds().withSizeKeepingCentre (juce::jmin (520, getWidth() - 24), 300);
        auto r = kort.reduced (28, 22);
        r.removeFromTop (38 + 64);
        falt.setBounds (r.removeFromTop (40));
        r.removeFromTop (10);
        auto rad = r.removeFromTop (38);
        lasUpp.setBounds (rad.removeFromLeft (rad.getWidth() / 2 - 5));
        rad.removeFromLeft (10);
        kop.setBounds (rad.removeFromLeft (rad.getWidth() / 2 - 4));
        rad.removeFromLeft (8);
        demo.setBounds (rad);
        r.removeFromTop (6);
        statusYta = r.removeFromTop (22);
    }

private:
    void forsok()
    {
        if (laggTill (falt.getText()))
        {
            status = {};
            setVisible (false);
            if (onUpplast) onUpplast();
        }
        else
        {
            fel = true;
            status = sv_en ("Den nyckeln fungerar inte här. Kolla att hela nyckeln kom med.",
                            "That key doesn't work here. Check that the whole key was pasted.");
            repaint();
        }
    }

    // Låstes maskinen upp i en annan maskin? Då försvinner rutan här också.
    void timerCallback() override
    {
        if (! isVisible()) return;
        if (kontrollera()) { setVisible (false); if (onUpplast) onUpplast(); }
    }

    const juce::Colour accent { 0xffffb23f };
    juce::TextEditor falt;
    juce::TextButton lasUpp, kop, demo;
    juce::Rectangle<int> kort, statusYta;
    juce::String status;
    bool fel = false;
};
} // namespace licens
