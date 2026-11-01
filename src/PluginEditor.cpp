#include "PluginEditor.h"

namespace
{
const juce::Colour kBg (0xff141210), kPanel (0xff201c18), kMassing (0xffc8893a), kText (0xffe6dccf), kDim (0xff7a6f63);
struct R { const char* id; const char* namn; const char* tips; };
// rad 1: maskin (6), rad 2: källor (7), rad 3: slut (3). Samma ordning som i processorn.
const R kRatter[] = {
    { "tryck", "TRYCK", "Ångtryck: ljusare, hårdare slag och fler pys" },
    { "fart", "FART", "Hur fort hjulen går. 0 = loket står still på perrongen" },
    { "rytt", "RYTT", "Rytm: fler knack och klick, längre metallklang" },
    { "lacka", "LÄCKA", "Ständigt väsande läckage under allt" },
    { "liv", "LIV", "Slump per slag: anslag, ton, panorering och släpande tajming" },
    { "bredd", "BREDD", "Hur brett ljuden sprids i stereo" },
    { "anga", "ÅNGA", "Tjoff från cylindrarna" },
    { "pys", "PYS", "Ventilen som släpper ut ånga" },
    { "kompressor", "KOMPRESSOR", "Lågt dunkande maskinvarv" },
    { "knack", "KNACK", "Metallknack: vevstake, ventil, hammare, kedja eller rör" },
    { "klick", "KLICK", "Småtick: relä, tick, trä eller spärrhake" },
    { "skrammel", "SKRAMMEL", "Bromsgnissel, kopplingar och droppar" },
    { "vissla", "VISSLA", "Ångvisslan, då och då" },
    { "ton", "TON", "Grundton för allt, en oktav upp eller ner" },
    { "rum", "RUM", "Stor plåthall" },
    { "volym", "VOLYM", "Utvolym" },
};
const char* kTypId[5] { "angaTyp", "pysTyp", "knackTyp", "klickTyp", "skrammelTyp" };
// vilken ratt i rad 2 som har en typväljare under sig
const int kTypUnder[5] { 0, 1, 3, 4, 5 };

constexpr int kW = 860, kRadY0 = 78, kRadH = 140, kKol = 112, kX0 = 26;
}

VevhusEditor::Look::Look()
{
    setColour (juce::ComboBox::backgroundColourId, kBg);
    setColour (juce::ComboBox::outlineColourId, kDim.withAlpha (0.5f));
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::arrowColourId, kMassing);
    setColour (juce::PopupMenu::backgroundColourId, kPanel);
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kMassing.darker (0.3f));
    setColour (juce::TextButton::buttonColourId, kPanel.brighter (0.1f));
    setColour (juce::TextButton::textColourOffId, kText);
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
    setLookAndFeel (&look);
    for (auto& d : kRatter)
    {
        auto rt = std::make_unique<Ratt>();
        rt->s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        rt->s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        rt->s.setTooltip (juce::String::fromUTF8 (d.tips));
        rt->s.setDoubleClickReturnValue (true, proc.apvts.getParameterRange (d.id).convertFrom0to1 (proc.apvts.getParameter (d.id)->getDefaultValue()));
        rt->namn.setText (juce::String::fromUTF8 (d.namn), juce::dontSendNotification);
        rt->namn.setJustificationType (juce::Justification::centred);
        rt->namn.setColour (juce::Label::textColourId, kText);
        rt->namn.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        rt->att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, d.id, rt->s);
        addAndMakeVisible (rt->s);
        addAndMakeVisible (rt->namn);
        ratter.push_back (std::move (rt));
    }
    for (int k = 0; k < 5; ++k)
    {
        auto& t = typer[(size_t) k];
        t.box.addItemList (VevhusProcessor::typNamn (k), 1);
        t.box.setJustificationType (juce::Justification::centred);
        t.box.setTooltip ("Klangtyp");
        t.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, kTypId[k], t.box);
        addAndMakeVisible (t.box);
    }

    avgang.setButtonText (juce::String::fromUTF8 ("AVGÅNG"));
    avgang.setTooltip (juce::String::fromUTF8 ("Loket drar igång från stillastående och når FART efter 8 takter"));
    avgang.setColour (juce::ToggleButton::textColourId, kText);
    avgang.setColour (juce::ToggleButton::tickColourId, kMassing);
    avgangAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "avgang", avgang);
    addAndMakeVisible (avgang);

    slumpa.setColour (juce::TextButton::buttonColourId, kMassing.darker (0.4f));
    slumpa.setTooltip ("Slumpa alla rattar och typer utom volym");
    slumpa.onClick = [this] { proc.slumpa(); };
    addAndMakeVisible (slumpa);

    for (int i = 0; i < proc.getNumPrograms(); ++i) presetBox.addItem (proc.getProgramName (i), i + 1);
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this]
    {
        const int i = presetBox.getSelectedId() - 1;
        if (i >= 0 && i != proc.getCurrentProgram()) { proc.setCurrentProgram (i); proc.updateHostDisplay(); }
        visadPreset = i;
    };
    forra.onClick = [this] { const int n = proc.getNumPrograms(); presetBox.setSelectedId ((proc.getCurrentProgram() + n - 1) % n + 1); };
    nasta.onClick = [this] { const int n = proc.getNumPrograms(); presetBox.setSelectedId ((proc.getCurrentProgram() + 1) % n + 1); };
    addAndMakeVisible (presetBox); addAndMakeVisible (forra); addAndMakeVisible (nasta);
    presetBox.setSelectedId (proc.getCurrentProgram() + 1, juce::dontSendNotification);
    visadPreset = proc.getCurrentProgram();

    setSize (kW, kRadY0 + 3 * kRadH + 10);
    startTimerHz (5);
}

VevhusEditor::~VevhusEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void VevhusEditor::timerCallback()
{
    // om värden byter preset ska rutan följa med
    if (proc.getCurrentProgram() != visadPreset)
    {
        visadPreset = proc.getCurrentProgram();
        presetBox.setSelectedId (visadPreset + 1, juce::dontSendNotification);
    }
}

void VevhusEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);
    g.setColour (kMassing);
    g.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    g.drawText ("VEVHUS", 22, 12, 300, 36, juce::Justification::left);
    g.setColour (kDim);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (juce::String::fromUTF8 ("MASKINHALLEN · Phunkshunz AUDIO"), 24, 46, 400, 16, juce::Justification::left);
    const char* rubriker[] { "MASKIN", "KÄLLOR", "SLUT" };
    for (int i = 0; i < 3; ++i)
    {
        const int y = kRadY0 + i * kRadH;
        g.setColour (kPanel);
        g.fillRoundedRectangle (14.0f, (float) y, (float) (kW - 28), (float) (kRadH - 10), 6.0f);
        g.setColour (kDim);
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 (rubriker[i]), 24, y + 5, 100, 12, juce::Justification::left);
    }
}

void VevhusEditor::resized()
{
    auto lagg = [&] (int idx, int rad, int kol)
    {
        auto& r = *ratter[(size_t) idx];
        const int x = kX0 + kol * kKol, y = kRadY0 + rad * kRadH + 18;
        r.s.setBounds (x + 18, y, 70, 70);
        r.namn.setBounds (x, y + 70, kKol - 6, 16);
    };
    for (int i = 0; i < 6; ++i) lagg (i, 0, i);
    for (int i = 0; i < 7; ++i) lagg (6 + i, 1, i);
    for (int i = 0; i < 3; ++i) lagg (13 + i, 2, i);
    for (int k = 0; k < 5; ++k)
        typer[(size_t) k].box.setBounds (kX0 + kTypUnder[k] * kKol + 4, kRadY0 + kRadH + 18 + 90, kKol - 14, 22);

    const int y3 = kRadY0 + 2 * kRadH;
    avgang.setBounds (kX0 + 3 * kKol + 10, y3 + 30, 110, 26);
    slumpa.setBounds (kX0 + 3 * kKol + 10, y3 + 70, 100, 28);
    presetBox.setBounds (kX0 + 5 * kKol - 10, y3 + 50, 200, 28);
    forra.setBounds (presetBox.getX() - 32, y3 + 50, 28, 28);
    nasta.setBounds (presetBox.getRight() + 4, y3 + 50, 28, 28);

}
