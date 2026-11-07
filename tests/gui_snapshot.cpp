// MOTSTÅND — renders the panel to PNG (no window needed). Usage: MotstandSnapshot out.png [sv|en] [scale] [pattern 0-7] [preset] [doom|studio] [locked]
#include "../src/PluginProcessor.h"
#include "../src/PluginEditor.h"
#include "../src/Licens.h"

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile (argc > 1 ? argv[1] : "motstand_panel.png");
    const bool en = argc > 2 && juce::String (argv[2]) == "en";
    const float scale = argc > 3 ? juce::String (argv[3]).getFloatValue() : 1.f;
    const int pattern = argc > 4 ? juce::String (argv[4]).getIntValue() : 0;     // 0..7 = A..H
    const int preset  = argc > 5 ? juce::String (argv[5]).getIntValue() : 0;     // 0 = FABRIK
    const bool doomPage = argc > 6 && juce::String (argv[6]) == "doom";            // show the UNDERGÅNG tab
    const bool locked = argc > 7 && juce::String (argv[7]) == "locked";            // show the licence box

    MotstandProcessor p;
    if (preset > 0) p.loadPreset (preset);
    sprak::sattEngelska (en);   // SV | EN lives in the shared MASKINHALLEN settings file
    // show some life: a bit of VINKELSLIP and a playing machine
    auto set = [&] (const juce::String& id, float v) { auto* q = p.apvts.getParameter (id); q->setValueNotifyingHost (q->convertTo0to1 (v)); };
    set (motstand::pid::pattern, (float) pattern);
    if (preset == 0)
    {
        set (motstand::pid::inst (motstand::KICK, "slipDrive"), 0.35f);
        set (motstand::pid::inst (motstand::TOM_LO, "slipDrive"), 0.5f);
        set (motstand::pid::inst (motstand::OH, "slipDrive"), 0.4f);
    }
    p.prepareToPlay (48000, 512);
    juce::AudioBuffer<float> buf (p.getTotalNumOutputChannels(), 512);
    for (int b = 0; b < 300; ++b) { juce::MidiBuffer m; p.processBlock (buf, m); }   // ~3 s of playing

    if (locked) licens::upplast().store (false);
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    if (doomPage)   // click the tab, like a user would
        for (auto* c : ed->getChildren())
            for (auto* cc : c->getChildren())
                if (auto* b = dynamic_cast<juce::TextButton*> (cc))
                    if (b->getButtonText() == juce::String::fromUTF8 (u8"UNDERGÅNG") || b->getButtonText() == "DOOM") b->triggerClick();
    ed->setSize (juce::roundToInt (dock::Root::W * scale), juce::roundToInt (dock::Root::H * scale));
    for (int i = 0; i < 20; ++i) { juce::MidiBuffer m; p.processBlock (buf, m); if (auto* me = dynamic_cast<MotstandEditor*> (ed.get())) juce::ignoreUnused (me); }
    // let the editor tick a few times so meters and strobe settle
    for (int i = 0; i < 12; ++i) { juce::MidiBuffer m; p.processBlock (buf, m); juce::MessageManager::getInstance()->runDispatchLoopUntil (35); }

    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.f);
    juce::PNGImageFormat png;
    out.deleteFile();
    juce::FileOutputStream os (out);
    if (! png.writeImageToStream (img, os)) { std::printf ("could not write %s\n", out.getFullPathName().toRawUTF8()); return 1; }
    std::printf ("wrote %s (%dx%d)\n", out.getFullPathName().toRawUTF8(), img.getWidth(), img.getHeight());
    return 0;
}
