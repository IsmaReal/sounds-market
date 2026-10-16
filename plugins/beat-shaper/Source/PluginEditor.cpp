#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    const juce::Colour panelColour { 0xff101016 };
    const juce::Colour accentColour { 0xffff8a3d };
    const juce::Colour textColour { 0xffd8d8e4 };
    constexpr int headerHeight = 96;

    struct SnapOption
    {
        const char* name;
        int divisions; // divisiones en los 4 beats
    };

    constexpr SnapOption snapOptions[] = {
        { "Sin snap", 0 },
        { "1/4 (negra)", 4 },
        { "1/8 (corchea)", 8 },
        { "1/16 (semicorchea)", 16 },
        { "1/32", 32 },
        { "1/8 tresillo", 12 },
        { "1/16 tresillo", 24 },
    };

    void setupLabel (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, textColour.withAlpha (0.7f));
        label.setFont (juce::FontOptions (12.0f));
    }
}

BeatShaperEditor::BeatShaperEditor (BeatShaperProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    lookAndFeel.setColourScheme (juce::LookAndFeel_V4::getMidnightColourScheme());
    lookAndFeel.setColour (juce::Slider::rotarySliderFillColourId, accentColour);
    lookAndFeel.setColour (juce::Slider::thumbColourId, accentColour);
    lookAndFeel.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1c1c26));
    lookAndFeel.setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a3a4a));
    lookAndFeel.setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1c1c26));
    lookAndFeel.setColour (juce::PopupMenu::highlightedBackgroundColourId, accentColour.withAlpha (0.6f));
    setLookAndFeel (&lookAndFeel);

    // Presets de fábrica.
    const auto& presets = beatshaper::getFactoryPresets();

    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (juce::String::fromUTF8 (presets[(size_t) i].name.c_str()), i + 1);

    presetBox.setTextWhenNothingSelected ("Elegir preset...");
    presetBox.onChange = [this] { loadPreset (presetBox.getSelectedItemIndex()); };
    addAndMakeVisible (presetBox);
    setupLabel (presetLabel, "PRESET");
    addAndMakeVisible (presetLabel);

    // Snap de la grilla.
    for (int i = 0; i < (int) std::size (snapOptions); ++i)
    {
        snapBox.addItem (snapOptions[i].name, i + 1);

        if (snapOptions[i].divisions == processor.getSnapDivisions())
            snapBox.setSelectedItemIndex (i, juce::dontSendNotification);
    }

    snapBox.onChange = [this] {
        const auto divisions = snapOptions[juce::jmax (0, snapBox.getSelectedItemIndex())].divisions;
        processor.setSnapDivisions (divisions);
        envelopeEditor.setSnapDivisions (divisions);
    };
    addAndMakeVisible (snapBox);
    setupLabel (snapLabel, "SNAP");
    addAndMakeVisible (snapLabel);

    // Perillas.
    for (auto* slider : { &mixSlider, &smoothSlider })
    {
        slider->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 16);
        addAndMakeVisible (*slider);
    }

    mixAttachment = std::make_unique<SliderAttachment> (processor.parameters, "mix", mixSlider);
    smoothAttachment = std::make_unique<SliderAttachment> (processor.parameters, "smooth", smoothSlider);
    setupLabel (mixLabel, "MIX");
    setupLabel (smoothLabel, "SMOOTH");
    addAndMakeVisible (mixLabel);
    addAndMakeVisible (smoothLabel);

    setupLabel (hintLabel, "Click: agregar nodo  |  Arrastrar: mover (Shift = sin snap)  |  "
                           "Doble click / click derecho: borrar  |  Punto azul: curvar");
    hintLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (hintLabel);

    // Editor de la envolvente.
    envelopeEditor.setSnapDivisions (processor.getSnapDivisions());
    envelopeEditor.setNodes (processor.getEnvelopeNodes());
    envelopeEditor.onChange = [this] (const std::vector<beatshaper::Node>& nodes) {
        processor.setEnvelopeNodes (nodes);
        lastEnvelopeVersion = processor.getEnvelopeVersion();
        presetBox.setSelectedId (0, juce::dontSendNotification);
    };
    addAndMakeVisible (envelopeEditor);
    lastEnvelopeVersion = processor.getEnvelopeVersion();

    setResizable (true, true);
    setResizeLimits (600, 360, 1600, 900);
    setSize (820, 460);

    startTimerHz (30);
}

BeatShaperEditor::~BeatShaperEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void BeatShaperEditor::loadPreset (int index)
{
    const auto& presets = beatshaper::getFactoryPresets();

    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    processor.setEnvelopeNodes (presets[(size_t) index].nodes);
    envelopeEditor.setNodes (processor.getEnvelopeNodes());
    lastEnvelopeVersion = processor.getEnvelopeVersion();
}

void BeatShaperEditor::timerCallback()
{
    // Si el host cargó otro estado (o cambió la envolvente por fuera), refrescamos.
    if (const auto version = processor.getEnvelopeVersion(); version != lastEnvelopeVersion)
    {
        lastEnvelopeVersion = version;
        envelopeEditor.setNodes (processor.getEnvelopeNodes());

        for (int i = 0; i < (int) std::size (snapOptions); ++i)
            if (snapOptions[i].divisions == processor.getSnapDivisions())
                snapBox.setSelectedItemIndex (i, juce::sendNotification);
    }

    envelopeEditor.setPlayhead (processor.getDisplayPhase(), processor.getDisplayGain(), true);

    if (const auto playing = processor.isHostPlaying(); playing != lastHostPlaying)
    {
        lastHostPlaying = playing;
        repaint (0, 0, getWidth(), headerHeight);
    }
}

//==============================================================================
void BeatShaperEditor::paint (juce::Graphics& g)
{
    g.fillAll (panelColour);

    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (16, 0);
    g.setColour (accentColour);
    g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText ("BEAT SHAPER", header.removeFromLeft (190), juce::Justification::centredLeft);

    g.setColour (textColour.withAlpha (0.5f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (processor.isHostPlaying() ? "SYNC: HOST" : "SYNC: INTERNO",
                header.removeFromLeft (100), juce::Justification::centredLeft);
}

void BeatShaperEditor::resized()
{
    auto bounds = getLocalBounds();
    auto header = bounds.removeFromTop (headerHeight).reduced (16, 6);
    header.removeFromLeft (290); // título + estado de sync

    auto knob = [&] (juce::Slider& slider, juce::Label& label) {
        auto column = header.removeFromRight (76);
        label.setBounds (column.removeFromTop (12));
        slider.setBounds (column);
        header.removeFromRight (8);
    };

    knob (smoothSlider, smoothLabel);
    knob (mixSlider, mixLabel);

    auto combo = [&] (juce::ComboBox& box, juce::Label& label, int width) {
        auto column = header.removeFromLeft (width).withSizeKeepingCentre (width, 44);
        label.setBounds (column.removeFromTop (16));
        box.setBounds (column.removeFromTop (26));
        header.removeFromLeft (12);
    };

    combo (presetBox, presetLabel, juce::jmin (180, header.getWidth() / 2 - 12));
    combo (snapBox, snapLabel, juce::jmin (150, header.getWidth() - 12));

    auto footer = bounds.removeFromBottom (24).reduced (16, 0);
    hintLabel.setBounds (footer);

    envelopeEditor.setBounds (bounds.reduced (12, 0));
}
