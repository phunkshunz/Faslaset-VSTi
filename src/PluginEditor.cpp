#include "PluginEditor.h"

namespace
{
const juce::Colour kBg (0xff141210), kPanel (0xff201c18), kMassing (0xffc8893a), kText (0xffe6dccf), kDim (0xff7a6f63);
struct R { const char* id; const char* namn; const char* tips; };
// rad 1: makron, rad 2: källor, rad 3: ton och slut
const R kRatter[] = {
    { "tryck", "TRYCK", "Ångtryck: ljusare, hårdare tjoff och fler pys" },
    { "fart", "FART", "Hur fort hjulen går. 0 = loket står still på perrongen" },
    { "rytt", "RYTT", "Rytm i knacket: fler och längre metallslag" },
    { "lacka", "LÄCKA", "Ständigt väsande läckage under allt" },
    { "liv", "LIV", "Slump per slag: panorering, anslag, ton och släpande tajming" },
    { "anga", "ÅNGA", "Tjoff-ljudet från cylindrarna" },
    { "pys", "PYS", "Ventilen som släpper ut ånga" },
    { "kompressor", "KOMPRESSOR", "Lågt dunkande maskinvarv" },
    { "knack", "KNACK", "Metallknack från vevstaken" },
    { "vissla", "VISSLA", "Ångvisslan, då och då" },
    { "ton", "TON", "Grundton för allt, ett plus minus en oktav" },
    { "rum", "RUM", "Kort stålhall" },
    { "volym", "VOLYM", "Utvolym" },
};
}

void VevhusEditor::Look::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                           float a0, float a1, juce::Slider&)
{
    const auto b = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (6.0f);
    const float r = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    g.setColour (kPanel.brighter (0.15f));
    g.fillEllipse (c.x - r, c.y - r, r * 2, r * 2);
    juce::Path bak, fram;
    bak.addCentredArc (c.x, c.y, r - 3, r - 3, 0, a0, a1, true);
    fram.addCentredArc (c.x, c.y, r - 3, r - 3, 0, a0, a0 + pos * (a1 - a0), true);
    g.setColour (kDim.withAlpha (0.4f)); g.strokePath (bak, juce::PathStrokeType (3.0f));
    g.setColour (kMassing); g.strokePath (fram, juce::PathStrokeType (3.0f));
    const float a = a0 + pos * (a1 - a0);
    g.setColour (kText);
    g.drawLine (c.x, c.y, c.x + (r - 8) * std::sin (a), c.y - (r - 8) * std::cos (a), 2.5f);
}

VevhusEditor::VevhusEditor (VevhusProcessor& p) : AudioProcessorEditor (p), proc (p)
{
    for (auto& d : kRatter)
    {
        auto rt = std::make_unique<Ratt>();
        rt->s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        rt->s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        rt->s.setLookAndFeel (&look);
        rt->s.setTooltip (juce::String::fromUTF8 (d.tips));
        rt->s.setDoubleClickReturnValue (true, proc.apvts.getParameter (d.id)->getDefaultValue());
        rt->namn.setText (juce::String::fromUTF8 (d.namn), juce::dontSendNotification);
        rt->namn.setJustificationType (juce::Justification::centred);
        rt->namn.setColour (juce::Label::textColourId, kText);
        rt->namn.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        rt->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, d.id, rt->s);
        addAndMakeVisible (rt->s);
        addAndMakeVisible (rt->namn);
        ratter.push_back (std::move (rt));
    }
    avgang.setButtonText (juce::String::fromUTF8 ("AVGÅNG"));
    avgang.setTooltip (juce::String::fromUTF8 ("Loket drar igång från stillastående och når FART efter 8 takter"));
    avgang.setColour (juce::ToggleButton::textColourId, kText);
    avgang.setColour (juce::ToggleButton::tickColourId, kMassing);
    avgangAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "avgang", avgang);
    addAndMakeVisible (avgang);

    slumpa.setColour (juce::TextButton::buttonColourId, kMassing.darker (0.4f));
    slumpa.setColour (juce::TextButton::textColourOffId, kText);
    slumpa.setTooltip ("Slumpa alla rattar utom volym");
    slumpa.onClick = [this] { proc.slumpa(); };
    addAndMakeVisible (slumpa);

    setSize (640, 430);
}

VevhusEditor::~VevhusEditor()
{
    for (auto& r : ratter) r->s.setLookAndFeel (nullptr);
}

void VevhusEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kMassing);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("VEVHUS", 20, 12, 300, 36, juce::Justification::left);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String::fromUTF8 ("MASKINHALLEN · Phunkshunz AUDIO · prototyp"), 22, 44, 400, 16, juce::Justification::left);
    const char* rubriker[] { "MASKIN", "KÄLLOR", "SLUT" };
    for (int i = 0; i < 3; ++i)
    {
        const int y = 74 + i * 116;
        g.setColour (kPanel);
        g.fillRoundedRectangle (14.0f, (float) y, 612.0f, 108.0f, 6.0f);
        g.setColour (kDim);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 (rubriker[i]), 24, y + 4, 100, 12, juce::Justification::left);
    }
}

void VevhusEditor::resized()
{
    const int bredd = 100;
    auto lagg = [&] (int idx, int rad, int kol)
    {
        auto& r = *ratter[(size_t) idx];
        const int x = 30 + kol * (bredd + 16), y = 74 + rad * 116 + 16;
        r.s.setBounds (x + 15, y, 70, 70);
        r.namn.setBounds (x, y + 72, bredd, 16);
    };
    for (int i = 0; i < 5; ++i) lagg (i, 0, i);
    for (int i = 0; i < 5; ++i) lagg (5 + i, 1, i);
    for (int i = 0; i < 3; ++i) lagg (10 + i, 2, i);
    avgang.setBounds (30 + 3 * (bredd + 16) + 10, 74 + 2 * 116 + 26, 110, 26);
    slumpa.setBounds (30 + 3 * (bredd + 16) + 10, 74 + 2 * 116 + 60, 100, 28);
}
