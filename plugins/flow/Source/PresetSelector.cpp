#include "PresetSelector.h"

namespace flow::ui
{

PresetSelector::PresetSelector()
{
    combo.setTextWhenNothingSelected ("Custom");
    combo.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    combo.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    combo.onChange = [this] {
        if (combo.getSelectedId() != 0 && onSelect)
            onSelect (combo.getSelectedId());
    };
    addAndMakeVisible (combo);

    previousButton.setTooltip ("Previous preset");
    previousButton.onClick = [this] { step (-1); };
    addAndMakeVisible (previousButton);

    nextButton.setTooltip ("Next preset");
    nextButton.onClick = [this] { step (1); };
    addAndMakeVisible (nextButton);
}

void PresetSelector::showName (const juce::String& name)
{
    for (int i = 0; i < combo.getNumItems(); ++i)
    {
        if (combo.getItemText (i) == name)
        {
            combo.setSelectedItemIndex (i, juce::dontSendNotification);
            return;
        }
    }

    combo.setSelectedId (0, juce::dontSendNotification);
    combo.setText (name.isEmpty() ? juce::String ("Custom") : name, juce::dontSendNotification);
}

void PresetSelector::step (int delta)
{
    if (combo.getNumItems() == 0)
        return;

    if (combo.onBeforePopup)
        combo.onBeforePopup(); // la lista puede haber cambiado (presets del usuario)

    const auto count = combo.getNumItems();
    auto index = combo.getSelectedItemIndex();

    if (index < 0)
        index = delta > 0 ? -1 : 0;

    index = ((index + delta) % count + count) % count;
    combo.setSelectedItemIndex (index, juce::dontSendNotification);

    if (onSelect)
        onSelect (combo.getSelectedId());
}

void PresetSelector::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::field);
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
    g.drawVerticalLine (previousButton.getRight(), bounds.getY() + 1.0f, bounds.getBottom() - 1.0f);
    g.drawVerticalLine (nextButton.getX(), bounds.getY() + 1.0f, bounds.getBottom() - 1.0f);
}

void PresetSelector::resized()
{
    auto area = getLocalBounds();
    const auto buttonWidth = juce::jmin (48, area.getHeight() + 6);
    previousButton.setBounds (area.removeFromLeft (buttonWidth));
    nextButton.setBounds (area.removeFromRight (buttonWidth));
    combo.setBounds (area.reduced (4, 1));
}

} // namespace flow::ui
