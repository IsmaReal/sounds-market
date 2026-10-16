#include "ModuleControls.h"
#include "Presets.h"

namespace flow::ui
{

namespace
{
    constexpr int titleHeight = 64;
    constexpr int nameWidth = 140;
}

ModuleControls::ModuleControls()
{
    hint.setColour (juce::Label::textColourId, colours::textMuted);
    hint.setFont (juce::FontOptions (14.0f));
    hint.setJustificationType (juce::Justification::centredLeft);
    addChildComponent (hint);

    presetSelector.onSelect = [this] (int itemId) {
        if (module == nullptr)
            return;

        const auto& list = getModulePresets (module->getType());

        if (juce::isPositiveAndBelow (itemId - 1, (int) list.size()))
            applyPreset (list[(size_t) itemId - 1].name);
    };
    addChildComponent (presetSelector);
}

void ModuleControls::applyPreset (const juce::String& name)
{
    if (module == nullptr)
        return;

    module->applyPreset (name, getPatternBeats ? getPatternBeats() : 4);
    refresh();
    refreshPreset();

    if (onPresetApplied)
        onPresetApplied();
}

void ModuleControls::refreshPreset()
{
    if (module != nullptr)
        presetSelector.showName (module->presetName);
}

void ModuleControls::setModule (std::shared_ptr<Module> moduleToShow)
{
    if (module == moduleToShow)
    {
        refresh();
        return;
    }

    module = std::move (moduleToShow);
    controls.clear();
    presetSelector.setVisible (module != nullptr);

    auto& combo = presetSelector.getComboBox();
    combo.clear (juce::dontSendNotification);

    if (module != nullptr)
    {
        // "Init" arriba y un submenú por categoría: básicos primero, creativos después.
        const auto& list = getModulePresets (module->getType());
        auto* root = combo.getRootMenu();

        for (size_t i = 0; i < list.size();)
        {
            const auto& category = list[i].category;

            if (category == "Basic")
            {
                root->addItem ((int) i + 1, list[i].name);
                ++i;
                continue;
            }

            juce::PopupMenu sub;

            for (; i < list.size() && list[i].category == category; ++i)
                sub.addItem ((int) i + 1, list[i].name);

            root->addSubMenu (category, sub);
        }
    }

    if (module != nullptr)
    {
        const auto specs = module->getParameterSpecs();

        for (int i = 0; i < (int) specs.size(); ++i)
        {
            auto control = std::make_unique<Control>();
            control->name = juce::String (specs[(size_t) i].name).toUpperCase();

            auto& slider = control->slider;
            slider.setSliderStyle (juce::Slider::LinearHorizontal);
            slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
            slider.setRange (0.0, 100.0, 1.0);
            slider.setTextValueSuffix ("%");
            slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            slider.setDoubleClickReturnValue (true, specs[(size_t) i].defaultValue * 100.0);
            slider.onValueChange = [this, i, &slider] {
                if (module != nullptr)
                    module->setParameter (i, (float) slider.getValue() * 0.01f);
            };
            addAndMakeVisible (slider);
            controls.push_back (std::move (control));
        }

        hint.setText (module->getType() == ModuleType::volume
                          ? "Draw the volume envelope on the grid. MIX blends the original and the shaped signal."
                          : "Draw on the grid how much of the effect you hear across the pattern.",
                      juce::dontSendNotification);
        hint.setVisible (controls.empty());
    }

    refresh();
    refreshPreset();
    resized();
    repaint();
}

void ModuleControls::refresh()
{
    if (module == nullptr)
        return;

    for (int i = 0; i < (int) controls.size(); ++i)
    {
        controls[(size_t) i]->slider.setValue (module->getParameter (i) * 100.0, juce::dontSendNotification);
        controls[(size_t) i]->slider.setEnabled (module->enabled.load());
    }
}

void ModuleControls::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::panel);
    g.fillRoundedRectangle (bounds, 8.0f);

    auto title = getLocalBounds().removeFromTop (titleHeight).reduced (18, 0);

    g.setColour (colours::accent);
    g.fillRoundedRectangle (juce::Rectangle<float> ((float) title.getX(), (float) title.getCentreY() - 13.0f, 4.0f, 26.0f), 2.0f);

    g.setColour (colours::text);
    g.setFont (spacedFont (19.0f, true, 0.2f));
    g.drawText (module != nullptr ? juce::String (module->getName()).toUpperCase() : juce::String(),
                title.withTrimmedLeft (24).withWidth (nameWidth), juce::Justification::centredLeft);

    if (! labelArea.isEmpty())
    {
        g.setColour (colours::textMuted);
        g.setFont (spacedFont (12.0f, false, 0.3f));
        g.drawText ("MODULE CONTROLS", labelArea, juce::Justification::centredRight);
    }

    g.setColour (colours::outline);
    g.drawHorizontalLine (titleHeight, 1.0f, (float) getWidth() - 1.0f);

    // Nombres de los parámetros
    g.setColour (colours::text.withAlpha (0.85f));
    g.setFont (spacedFont (14.0f, false, 0.15f));

    for (const auto& control : controls)
        g.drawText (control->name, control->slider.getBounds().withY (titleHeight + 18).withHeight (20),
                    juce::Justification::centredLeft);
}

void ModuleControls::resized()
{
    // Encabezado: nombre | selector de presets | "MODULE CONTROLS"
    auto title = getLocalBounds().removeFromTop (titleHeight).reduced (18, 0);
    title.removeFromLeft (24 + nameWidth);
    labelArea = title.getWidth() > 520 ? title.removeFromRight (170) : juce::Rectangle<int>();
    const auto selectorWidth = juce::jmin (340, title.getWidth() - 10);
    presetSelector.setBounds (title.removeFromLeft (selectorWidth).withSizeKeepingCentre (selectorWidth, 38));

    auto area = getLocalBounds().withTrimmedTop (titleHeight).reduced (26, 14);
    hint.setBounds (area);

    if (controls.empty())
        return;

    const auto columnGap = 40;
    const auto columns = juce::jmax (3, (int) controls.size());
    const auto columnWidth = (area.getWidth() - columnGap * (columns - 1)) / columns;

    for (int i = 0; i < (int) controls.size(); ++i)
    {
        const auto x = area.getX() + i * (columnWidth + columnGap);
        controls[(size_t) i]->slider.setBounds (x, area.getY() + 26, columnWidth, 32);
    }
}

} // namespace flow::ui
