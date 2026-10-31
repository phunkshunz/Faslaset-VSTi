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
    : juce::AudioProcessorEditor (&p), proc (p), grid (p)
{
    using namespace ArmColours;
    setLookAndFeel (&lnf);

    auto* lg = new Logo();
    lg->onClick = [this] { about.setVisible (true); about.toFront (false); };
    logo.reset (lg);
    addAndMakeVisible (*logo);

    for (auto* b : { &prevBtn, &nextBtn, &presetBtn, &spikBtn, &slapBtn, &undoBtn, &diceAllBtn, &tonerBtn, &rytmBtn, &klangBtn })
        addAndMakeVisible (*b);
    prevBtn.onClick = [this] {
        const int n = numPresets();
        proc.loadPreset ((juce::jmax (0, proc.currentPreset.load()) + n - 1) % n);
    };
    nextBtn.onClick = [this] { proc.loadPreset ((proc.currentPreset.load() + 1) % numPresets()); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    presetBtn.setTooltip ("Alla presets");
    spikBtn.onClick = [this] { setMode (0); };
    slapBtn.onClick = [this] { setMode (1); };
    spikBtn.setTooltip (juce::String::fromUTF8 ("Psy: kick + basnoter på 16-delarna efter (K-B-B-B)"));
    slapBtn.setTooltip (juce::String::fromUTF8 ("Techno: en lång basnot per slag som rullar efter kicken"));
    undoBtn.onClick = [this] { proc.undo(); };
    undoBtn.setTooltip (juce::String::fromUTF8 ("Ångra senaste tärning eller ändring"));
    diceAllBtn.onClick = [this] { proc.diceAll(); };
    diceAllBtn.setTooltip (juce::String::fromUTF8 ("Slumpa allt: kick, tonart, skala, stil, längd och basgång"));
    tonerBtn.onClick = [this] { proc.diceNotes(); };
    tonerBtn.setTooltip (juce::String::fromUTF8 ("Nya toner, samma rytm"));
    rytmBtn.onClick = [this] { proc.diceRhythm(); };
    rytmBtn.setTooltip (juce::String::fromUTF8 ("Ny rytm, samma toner"));
    klangBtn.onClick = [this] { proc.diceSound(); };
    klangBtn.setTooltip (juce::String::fromUTF8 ("Slumpa basens klang: form, filter, bett"));

    spela.attach (proc.apvts, "spela");
    spela.setTooltip (juce::String::fromUTF8 ("Egen klocka när DAW:en står still"));
    addAndMakeVisible (spela);
    tempo.attach (proc.apvts, paramId (PTempo), "TEMPO");
    addAndMakeVisible (tempo);

    addAndMakeVisible (scope);
    addAndMakeVisible (grid);

    kickBox.attach (proc.apvts, paramId (PKick));
    kickBox.setTooltip (juce::String::fromUTF8 ("Kicken: BETONG:s 29 samplade kickar"));
    addAndMakeVisible (kickBox);
    skala.attach (proc.apvts, paramId (PBSkala));
    stil.attach (proc.apvts, paramId (PBStil));
    stil.setTooltip (juce::String::fromUTF8 ("Hur tärningen bygger basgången"));
    addAndMakeVisible (skala);
    addAndMakeVisible (stil);

    auto setup = [&] (Knob& k, int id, const char* name, juce::Colour c) {
        k.attach (proc.apvts, paramId (id), juce::String::fromUTF8 (name));
        k.setAccent (c);
        addAndMakeVisible (k);
    };
    setup (kStam, PKStam, "STÄMNING", kick);
    setup (kLangd, PKLangd, "LÄNGD", kick);
    setup (kDriv, PKDriv, "DRIV", kick);
    setup (kNiva, PKNiva, "NIVÅ", kick);
    setup (bTon, PBTon, "GRUNDTON", bass);
    setup (bSlag, PBSlag, "SLAG", bass);
    setup (bForm, PBForm, "FORM", bass);
    setup (bFilter, PBFilter, "FILTER", bass);
    setup (bReso, PBReso, "RESO", bass);
    setup (bEnv, PBEnv, "ENV", bass);
    setup (bEnvTid, PBEnvTid, "ENV TID", bass);
    setup (bDecay, PBDecay, "DECAY", bass);
    setup (bLangd, PBLangd, "LÄNGD", bass);
    setup (bBett, PBBett, "BETT", bass);
    setup (bNiva, PBNiva, "NIVÅ", bass);
    setup (utkling, PUtkling, "UTKLING", sodium);
    setup (volym, PVolym, "VOLYM", sodium);

    kickDice.accent = kick;
    bassDice.accent = bass;
    kickDice.onClick = [this] { proc.diceKick(); };
    bassDice.onClick = [this] { proc.diceBass(); };
    addAndMakeVisible (kickDice);
    addAndMakeVisible (bassDice);

    addChildComponent (about);

    setSize (1060, 640);
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
    {
        proc.pushUndo();
        prm->setValueNotifyingHost (prm->convertTo0to1 ((float) m));
        proc.edited();
    }
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
    grid.refresh();

    const bool spik = proc.get (PLage) < 0.5f;
    spikBtn.setToggleState (spik, juce::dontSendNotification);
    slapBtn.setToggleState (! spik, juce::dontSendNotification);
    scope.setLabels (spik);
    undoBtn.setEnabled (proc.canUndo());

    const auto label = proc.presetLabel();
    if (presetBtn.getButtonText() != label) presetBtn.setButtonText (label);

    repaint (scopeArea.getRight() - 262, scopeArea.getY() - 24, 262, 24);
}

void FaslasetEditor::paint (juce::Graphics& g)
{
    using namespace ArmColours;
    g.fillAll (ground);

    drawPanel (g, kickArea, "KICK");
    drawPanel (g, bassArea, juce::String::fromUTF8 ("BASGÅNG"));
    drawPanel (g, masterArea, "MASTER");

    g.setColour (kick);   g.fillRect (kickArea.getX() + 12, kickArea.getY() + 28, 28, 2);
    g.setColour (bass);   g.fillRect (bassArea.getX() + 12, bassArea.getY() + 28, 28, 2);
    g.setColour (sodium); g.fillRect (masterArea.getX() + 12, masterArea.getY() + 28, 28, 2);

    g.setColour (muted);
    g.setFont (juce::FontOptions (9.5f));
    g.drawText ("SAMPLE", kickLabel, juce::Justification::centredLeft);
    g.drawText ("SKALA", skalaLabel, juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("STIL (FÖR TÄRNINGEN)"), stilLabel, juce::Justification::centredLeft);
    g.setFont (juce::FontOptions (10.0f));
    g.drawText (juce::String::fromUTF8 ("stängs på nollgenomgången"), kickArea.getX() + 56, kickArea.getY() + 8, 170, 16, juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 ("klick på/av · dra = ton · dubbelklick = grundton · högerklick = oktav"),
                bassArea.getX() + 94, bassArea.getY() + 8, 340, 16, juce::Justification::centredLeft);

    // statusbrickan
    const auto ov = proc.overlaps();
    const auto fa = proc.faults();
    auto badge = juce::Rectangle<int> (scopeArea.getRight() - 260, scopeArea.getY() - 22, 260, 20).reduced (1);
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
    r.removeFromTop (12);

    {
        auto t = topArea;
        logo->setBounds (t.removeFromLeft (180));
        auto pre = t.removeFromLeft (290).withSizeKeepingCentre (290, 30);
        prevBtn.setBounds (pre.removeFromLeft (30));
        nextBtn.setBounds (pre.removeFromRight (30));
        presetBtn.setBounds (pre.reduced (4, 0));
        t.removeFromLeft (14);
        auto mode = t.removeFromLeft (196).withSizeKeepingCentre (196, 30);
        spikBtn.setBounds (mode.removeFromLeft (96));
        slapBtn.setBounds (mode.removeFromRight (96));
        tempo.setBounds (t.removeFromRight (64).withTrimmedTop (-6).withTrimmedBottom (-6));
        t.removeFromRight (8);
        spela.setBounds (t.removeFromRight (76).withSizeKeepingCentre (76, 30));
        t.removeFromRight (8);
        diceAllBtn.setBounds (t.removeFromRight (76).withSizeKeepingCentre (76, 30));
        t.removeFromRight (8);
        undoBtn.setBounds (t.removeFromRight (76).withSizeKeepingCentre (76, 30));
    }

    scopeArea = r.removeFromTop (160).withTrimmedTop (24);
    scope.setBounds (scopeArea);
    r.removeFromTop (12);

    kickArea = r.removeFromLeft (232);
    r.removeFromLeft (12);
    masterArea = r.removeFromRight (104);
    r.removeFromRight (12);
    bassArea = r;

    // ---- kick
    {
        auto b = kickArea.reduced (12, 0).withTrimmedTop (38).withTrimmedBottom (10);
        kickLabel = b.removeFromTop (13);
        kickBox.setBounds (b.removeFromTop (28));
        b.removeFromTop (10);
        const int cw = b.getWidth() / 2, rh = b.getHeight() / 2;
        kStam.setBounds (b.getX(), b.getY(), cw, rh);
        kLangd.setBounds (b.getX() + cw, b.getY(), cw, rh);
        kDriv.setBounds (b.getX(), b.getY() + rh, cw, rh);
        kNiva.setBounds (b.getX() + cw, b.getY() + rh, cw, rh);
        kickDice.setBounds (kickArea.getRight() - 34, kickArea.getY() + 7, 22, 22);
    }

    // ---- bas
    {
        auto b = bassArea.reduced (12, 0).withTrimmedTop (36).withTrimmedBottom (8);
        bassDice.setBounds (bassArea.getRight() - 34, bassArea.getY() + 7, 22, 22);
        auto hb = juce::Rectangle<int> (bassArea.getRight() - 42 - 3 * 66, bassArea.getY() + 6, 3 * 66, 24);
        tonerBtn.setBounds (hb.removeFromLeft (62)); hb.removeFromLeft (4);
        rytmBtn.setBounds (hb.removeFromLeft (62));  hb.removeFromLeft (4);
        klangBtn.setBounds (hb.removeFromLeft (62));

        auto row = b.removeFromTop (60);
        bTon.setBounds (row.removeFromLeft (72));
        bSlag.setBounds (row.removeFromLeft (72));
        row.removeFromLeft (10);
        auto sk = row.removeFromLeft (150).withTrimmedTop (8);
        skalaLabel = sk.removeFromTop (13);
        skala.setBounds (sk.removeFromTop (28));
        row.removeFromLeft (10);
        auto st = row.removeFromLeft (150).withTrimmedTop (8);
        stilLabel = st.removeFromTop (13);
        stil.setBounds (st.removeFromTop (28));

        auto knobs = b.removeFromBottom (78);
        Knob* ks[] = { &bForm, &bFilter, &bReso, &bEnv, &bEnvTid, &bDecay, &bLangd, &bBett, &bNiva };
        const int kw = knobs.getWidth() / 9;
        for (int i = 0; i < 9; ++i) ks[i]->setBounds (knobs.getX() + i * kw, knobs.getY(), kw, knobs.getHeight());

        b.removeFromTop (4);
        grid.setBounds (b);
    }

    // ---- master
    {
        auto b = masterArea.reduced (8, 0).withTrimmedTop (38).withTrimmedBottom (10);
        const int rh = juce::jmin (100, b.getHeight() / 2);
        utkling.setBounds (b.removeFromTop (rh));
        volym.setBounds (b.removeFromTop (rh));
    }

    about.setBounds (getLocalBounds());
}
