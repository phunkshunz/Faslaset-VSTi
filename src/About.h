#pragma once

#include "Gui.h"

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "1.0.0"
#endif

// Om-sidan som öppnas när man klickar på loggan. Klick var som helst stänger.
class AboutOverlay : public juce::Component
{
public:
    void paint (juce::Graphics& g) override
    {
        using namespace ArmColours;
        g.fillAll (ground.withAlpha (0.92f));
        auto card = getLocalBounds().withSizeKeepingCentre (660, 470);
        g.setColour (panel);
        g.fillRoundedRectangle (card.toFloat(), 8.0f);
        g.setColour (sodium);
        g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 8.0f, 1.2f);

        // tre fluorband i överkant: kick, UV, bas
        auto stripe = card.removeFromTop (8).toFloat().reduced (1.0f, 0.0f);
        const float w3 = stripe.getWidth() / 3.0f;
        g.setColour (kick);    g.fillRect (stripe.removeFromLeft (w3));
        g.setColour (sodium);  g.fillRect (stripe.removeFromLeft (w3));
        g.setColour (bass);    g.fillRect (stripe);

        auto r = card.reduced (44, 26);
        g.setColour (sodium);
        g.setFont (juce::FontOptions (46.0f, juce::Font::bold).withHorizontalScale (0.82f));
        g.drawText (juce::String::fromUTF8 ("FASLÅSET"), r.removeFromTop (50), juce::Justification::centredLeft);
        g.setColour (muted);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText (juce::String::fromUTF8 ("PHUNKSHUNZ AUDIO  ·  KICK + BAS  ·  NOLL FASFEL"), r.removeFromTop (16), juce::Justification::centredLeft);
        r.removeFromTop (24);

        g.setColour (text);
        g.setFont (juce::FontOptions (23.0f, juce::Font::bold));
        g.drawText (juce::String::fromUTF8 ("Sluta zooma i vågformer."), r.removeFromTop (30), juce::Justification::centredLeft);
        r.removeFromTop (12);

        juce::AttributedString body;
        body.setLineSpacing (5.0f);
        const auto bodyFont = juce::Font (juce::FontOptions (14.5f));
        const auto boldFont = juce::Font (juce::FontOptions (14.5f, juce::Font::bold));
        auto add = [&] (const char* s, juce::Colour c) { body.append (juce::String::fromUTF8 (s), c == text ? bodyFont : boldFont, c); };
        add ("Kick och bas som krockar är den vanligaste orsaken till en grötig botten. Sidechain är silvertejp. ", text);
        add ("FASLÅSET", sodium);
        add (" bygger dem i samma maskin: ", text);
        add ("kicken", kick);
        add (" stängs först när dess svängning passerar noll, och ", text);
        add ("basen", bass);
        add (" startar med fas noll på exakt den punkten, ner på bråkdelen av ett sample. "
             "Innan nästa slag huggs basen av med en giljotin som landar på exakt noll.\n\n"
             "Ingen överlappning. Ingen utsläckning. Det du ser i oscilloskopet är det som kommer ut ur högtalarna.", text);
        juce::TextLayout layout;
        layout.createLayout (body, (float) r.getWidth());
        layout.draw (g, r.removeFromTop ((int) layout.getHeight() + 4).toFloat());

        auto foot = card.reduced (44, 20).removeFromBottom (18);
        g.setColour (muted);
        g.setFont (juce::FontOptions (10.5f));
        g.drawText ("VERSION " + juce::String (JucePlugin_VersionString), foot, juce::Justification::centredLeft);
        g.drawText (juce::String::fromUTF8 ("Klicka för att stänga"), foot, juce::Justification::centredRight);
    }

    void mouseDown (const juce::MouseEvent&) override { setVisible (false); }
};
