#include "PluginEditor.h"

using namespace fl;

namespace
{
// Loggan: klickbar, öppnar om-sidan.
class Logo : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void()> onClick;
    Logo() { setMouseCursor (juce::MouseCursor::PointingHandCursor); setTooltip (juce::String::fromUTF8 ("Om FASLÅSET")); }
    void paint (juce::Graphics& g) override
    {
        using namespace ArmColours;
        auto r = getLocalBounds().toFloat();
        g.setColour (sodium);
        g.setFont (juce::FontOptions (30.0f, juce::Font::bold).withHorizontalScale (0.82f));
        g.drawText (juce::String::fromUTF8 ("FASLÅSET"), r.removeFromTop (32), juce::Justification::centredLeft);
        g.setColour (muted);
        g.setFont (juce::FontOptions (9.5f));
        g.drawText ("PHUNKSHUNZ AUDIO", r, juce::Justification::centredLeft);
    }
    void mouseUp (const juce::MouseEvent&) override { if (onClick) onClick(); }
};
}

FaslasetEditor::FaslasetEditor (FaslasetProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p)
{
    using namespace ArmColours;
    setLookAndFeel (&lnf);

    auto* lg = new Logo();
    lg->onClick = [this] { about.setVisible (true); about.toFront (false); };
    logo.reset (lg);
    addAndMakeVisible (*logo);

    for (auto* b : { &prevBtn, &nextBtn, &presetBtn, &spikBtn, &slapBtn, &diceAllBtn })
        addAndMakeVisible (*b);
    prevBtn.onClick = [this] {
        const int n = numPresets();
        proc.loadPreset ((juce::jmax (0, proc.currentPreset.load()) + n - 1) % n);
    };
    nextBtn.onClick = [this] {
        const int n = numPresets();
        proc.loadPreset ((proc.currentPreset.load() + 1) % n);
    };
    presetBtn.onClick = [this] { showPresetMenu(); };
    presetBtn.setTooltip ("Alla presets");
    spikBtn.onClick = [this] { setMode (0); };
    slapBtn.onClick = [this] { setMode (1); };
    spikBtn.setTooltip (juce::String::fromUTF8 ("Psy: kick + tre basslag per slag (K-B-B-B), varje 16-del huggs av"));
    slapBtn.setTooltip (juce::String::fromUTF8 ("Techno: en lång sub per slag som rullar efter kicken"));
    diceAllBtn.onClick = [this] { proc.diceAll(); };
    diceAllBtn.setTooltip (juce::String::fromUTF8 ("Slumpa kick och bas (läget och nivåerna står kvar)"));

    spela.attach (proc.apvts, "spela");
    spela.setTooltip (juce::String::fromUTF8 ("Egen klocka när DAW:en står still"));
    addAndMakeVisible (spela);
    tempo.attach (proc.apvts, paramId (PTempo), "TEMPO");
    addAndMakeVisible (tempo);

    addAndMakeVisible (scope);

    auto setup = [&] (Knob& k, int id, const char* name, juce::Colour c) {
        k.attach (proc.apvts, paramId (id), juce::String::fromUTF8 (name));
        k.setAccent (c);
        addAndMakeVisible (k);
    };
    setup (kStart, PKStart, "START", kick);
    setup (kSlut, PKSlut, "SLUT", kick);
    setup (kSvep, PKSvep, "SVEP", kick);
    setup (kLangd, PKLangd, "LÄNGD", kick);
    setup (kDriv, PKDriv, "DRIV", kick);
    setup (kNiva, PKNiva, "NIVÅ", kick);
    setup (bTon, PBTon, "TON", bass);
    setup (bForm, PBForm, "FORM", bass);
    setup (bFilter, PBFilter, "FILTER", bass);
    setup (bReso, PBReso, "RESO", bass);
    setup (bEnv, PBEnv, "ENV", bass);
    setup (bEnvTid, PBEnvTid, "ENV TID", bass);
    setup (bDecay, PBDecay, "DECAY", bass);
    setup (bBett, PBBett, "BETT", bass);
    setup (bNiva, PBNiva, "NIVÅ", bass);
    setup (giljotin, PGiljotin, "GILJOTIN", sodium);
    setup (volym, PVolym, "VOLYM", sodium);

    kickDice.accent = kick;
    bassDice.accent = bass;
    kickDice.onClick = [this] { proc.diceKick(); };
    bassDice.onClick = [this] { proc.diceBass(); };
    addAndMakeVisible (kickDice);
    addAndMakeVisible (bassDice);

    addChildComponent (about);

    setSize (980, 560);
    timerCallback();
    startTimerHz (30);
}

FaslasetEditor::~FaslasetEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FaslasetEditor::setMode (int m)
{
    if (auto* prm = proc.apvts.getParameter (paramId (PLage)))
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) m));
}

void FaslasetEditor::showPresetMenu()
{
    juce::PopupMenu menu;
    int n = 0;
    const auto* ps = presets (n);
    menu.addSectionHeader ("SPIKRAKT (PSY)");
    for (int i = 0; i < n; ++i)
    {
        if (i > 0 && ps[i].v[PLage] > 0.5f && ps[i - 1].v[PLage] < 0.5f)
            menu.addSectionHeader (juce::String::fromUTF8 ("SLÄP (TECHNO)"));
        menu.addItem (i + 1, juce::String::fromUTF8 (ps[i].name), true, i == proc.currentPreset.load());
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn),
                        [this] (int r) { if (r > 0) proc.loadPreset (r - 1); });
}

void FaslasetEditor::timerCallback()
{
    const double bpm = juce::jlimit (30.0, 300.0, proc.uiBpm.load());
    scope.pull (proc.scope(), proc.uiSr.load() * 60.0 / bpm);

    const bool spik = proc.apvts.getRawParameterValue (paramId (PLage))->load() < 0.5f;
    spikBtn.setToggleState (spik, juce::dontSendNotification);
    slapBtn.setToggleState (! spik, juce::dontSendNotification);
    scope.setLabels (spik);

    const auto label = proc.presetLabel();
    if (presetBtn.getButtonText() != label) presetBtn.setButtonText (label);

    repaint (scopeArea.getRight() - 242, scopeArea.getY() - 24, 242, 24);
}

void FaslasetEditor::paint (juce::Graphics& g)
{
    using namespace ArmColours;
    g.fillAll (ground);

    drawPanel (g, kickArea, "KICK");
    drawPanel (g, bassArea, "BAS");
    drawPanel (g, masterArea, "MASTER");
    g.setColour (muted);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (juce::String::fromUTF8 ("stängs på nollgenomgången"), kickArea.getX() + 56, kickArea.getY() + 8, 200, 16, juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("startar på fas 0, huggs av före nästa slag"), bassArea.getX() + 50, bassArea.getY() + 8, 300, 16, juce::Justification::centredLeft);

    // färgband under rubrikerna
    g.setColour (kick);  g.fillRect (kickArea.getX() + 12, kickArea.getY() + 28, 28, 2);
    g.setColour (bass);  g.fillRect (bassArea.getX() + 12, bassArea.getY() + 28, 28, 2);
    g.setColour (sodium); g.fillRect (masterArea.getX() + 12, masterArea.getY() + 28, 28, 2);

    // statusbrickan: bevis på att inget krockar
    const auto ov = proc.overlaps();
    const auto fa = proc.faults();
    auto badge = juce::Rectangle<int> (scopeArea.getRight() - 240, scopeArea.getY() - 22, 240, 20).reduced (1);
    const bool clean = ov == 0 && fa == 0;
    const auto col = clean ? kick : concrete;
    g.setColour (col.withAlpha (0.14f));
    g.fillRoundedRectangle (badge.toFloat(), 4.0f);
    g.setColour (col);
    g.drawRoundedRectangle (badge.toFloat().reduced (0.5f), 4.0f, 1.0f);
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    juce::String s = clean ? juce::String::fromUTF8 ("● NOLL FASFEL")
                           : juce::String::fromUTF8 ("● FASFEL ") + juce::String ((int) ov) + juce::String::fromUTF8 ("  ·  SPÄRR ") + juce::String ((int) fa);
    if (! proc.uiPlaying.load()) s += juce::String::fromUTF8 ("  ·  STOPP");
    g.drawText (s, badge, juce::Justification::centred);
}

void FaslasetEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    topArea = r.removeFromTop (48);
    r.removeFromTop (14);

    {
        auto t = topArea;
        logo->setBounds (t.removeFromLeft (190));
        t.removeFromLeft (10);
        auto pre = t.removeFromLeft (290).withSizeKeepingCentre (290, 30);
        prevBtn.setBounds (pre.removeFromLeft (30));
        nextBtn.setBounds (pre.removeFromRight (30));
        presetBtn.setBounds (pre.reduced (4, 0));
        t.removeFromLeft (16);
        auto mode = t.removeFromLeft (200).withSizeKeepingCentre (200, 30);
        spikBtn.setBounds (mode.removeFromLeft (98));
        slapBtn.setBounds (mode.removeFromRight (98));
        tempo.setBounds (t.removeFromRight (64).withTrimmedTop (-6).withTrimmedBottom (-6));
        t.removeFromRight (8);
        spela.setBounds (t.removeFromRight (78).withSizeKeepingCentre (78, 30));
        t.removeFromRight (8);
        diceAllBtn.setBounds (t.removeFromRight (78).withSizeKeepingCentre (78, 30));
    }

    scopeArea = r.removeFromTop (220).withTrimmedTop (22);
    scope.setBounds (scopeArea);
    r.removeFromTop (14);

    kickArea = r.removeFromLeft (296);
    r.removeFromLeft (12);
    masterArea = r.removeFromRight (120);
    r.removeFromRight (12);
    bassArea = r;

    auto grid = [] (juce::Rectangle<int> area, int cols, std::initializer_list<Knob*> knobs) {
        auto body = area.reduced (10, 0).withTrimmedTop (36).withTrimmedBottom (8);
        const int rows = ((int) knobs.size() + cols - 1) / cols;
        const int cw = body.getWidth() / cols, rh = body.getHeight() / rows;
        int i = 0;
        for (auto* k : knobs)
        {
            const int c = i % cols, row = i / cols;
            k->setBounds (body.getX() + c * cw, body.getY() + row * rh, cw, rh);
            ++i;
        }
    };
    grid (kickArea, 3, { &kStart, &kSlut, &kSvep, &kLangd, &kDriv, &kNiva });
    grid (bassArea, 5, { &bTon, &bForm, &bFilter, &bReso, &bEnv, &bEnvTid, &bDecay, &bBett, &bNiva });
    grid (masterArea, 1, { &giljotin, &volym });

    kickDice.setBounds (kickArea.getRight() - 34, kickArea.getY() + 7, 22, 22);
    bassDice.setBounds (bassArea.getRight() - 34, bassArea.getY() + 7, 22, 22);

    about.setBounds (getLocalBounds());
}
