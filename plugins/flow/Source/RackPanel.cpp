#include "RackPanel.h"

namespace flow::ui
{

//==============================================================================
ModuleCard::ModuleCard (std::shared_ptr<Module> moduleToShow, int indexInRack, int rackSize)
    : module (std::move (moduleToShow)), index (indexInRack), numModules (rackSize)
{
    powerButton.onClick = [this] {
        module->enabled.store (! module->enabled.load());
        refresh();
    };
    addAndMakeVisible (powerButton);

    removeButton.setTooltip ("Remove module");
    removeButton.onClick = [this] { if (onRemove) onRemove(); };
    addAndMakeVisible (removeButton);

    mixSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 62, 22);
    mixSlider.setRange (0.0, 100.0, 1.0);
    mixSlider.setTextValueSuffix ("%");
    mixSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    mixSlider.setDoubleClickReturnValue (true, 100.0);
    mixSlider.onValueChange = [this] { module->mix.store ((float) mixSlider.getValue() * 0.01f); };
    addAndMakeVisible (mixSlider);

    // Recibimos también los clicks de los hijos para seleccionar la tarjeta.
    addMouseListener (this, true);
    refresh();
}

void ModuleCard::setSelected (bool shouldBeSelected)
{
    selected = shouldBeSelected;
    repaint();
}

void ModuleCard::refresh()
{
    const auto on = module->enabled.load();
    powerButton.setActive (on);
    mixSlider.setEnabled (on);
    mixSlider.setValue (module->mix.load() * 100.0, juce::dontSendNotification);
    repaint();
}

juce::Rectangle<int> ModuleCard::getHandleArea() const noexcept
{
    return getLocalBounds().removeFromLeft (36);
}

void ModuleCard::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    g.setColour (selected ? colours::cardSelected : colours::card);
    g.fillRoundedRectangle (bounds, 8.0f);

    // Franja de la manija
    auto handle = getHandleArea().toFloat().reduced (1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    g.fillRoundedRectangle (handle.withTrimmedRight (-4.0f), 8.0f);

    g.setColour (colours::textMuted.withAlpha (draggingHandle ? 1.0f : 0.6f));

    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 2; ++col)
            g.fillEllipse (handle.getCentreX() - 5.0f + (float) col * 7.0f,
                           handle.getCentreY() - 13.0f + (float) row * 8.0f, 3.0f, 3.0f);

    if (selected)
    {
        g.setColour (colours::accent.withAlpha (0.12f));
        g.drawRoundedRectangle (bounds.expanded (0.5f), 8.0f, 4.0f);
        g.setColour (colours::accent);
        g.drawRoundedRectangle (bounds, 8.0f, 1.8f);
    }

    // Nombre
    const auto on = module->enabled.load();
    g.setColour (colours::text.withAlpha (on ? 1.0f : 0.45f));
    g.setFont (spacedFont (19.0f, true, 0.2f));
    g.drawText (juce::String (module->getName()).toUpperCase(),
                juce::Rectangle<int> (100, 22, getWidth() - 150, 34), juce::Justification::centredLeft);

    // Etiqueta MIX
    g.setColour (colours::textMuted.withAlpha (on ? 1.0f : 0.5f));
    g.setFont (spacedFont (14.0f, false, 0.15f));
    g.drawText ("MIX", juce::Rectangle<int> (60, mixSlider.getY(), 40, mixSlider.getHeight()),
                juce::Justification::centredLeft);
}

void ModuleCard::resized()
{
    powerButton.setBounds (52, 22, 34, 34);
    removeButton.setBounds (getWidth() - 36, 10, 24, 24);
    mixSlider.setBounds (108, getHeight() - 58, getWidth() - 124, 32);
}

void ModuleCard::mouseDown (const juce::MouseEvent& e)
{
    const auto local = e.getEventRelativeTo (this);

    if (e.eventComponent == this && e.mods.isPopupMenu())
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());
        menu.addItem (1, "Move up", index > 0);
        menu.addItem (2, "Move down", index < numModules - 1);
        menu.addSeparator();
        menu.addItem (3, "Remove");

        juce::Component::SafePointer<ModuleCard> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safeThis] (int result) {
            if (safeThis == nullptr)
                return;

            if (result == 1 && safeThis->onMoveTo) safeThis->onMoveTo (safeThis->index - 1);
            if (result == 2 && safeThis->onMoveTo) safeThis->onMoveTo (safeThis->index + 1);
            if (result == 3 && safeThis->onRemove) safeThis->onRemove();
        });
        return;
    }

    if (e.eventComponent != &removeButton && onSelect)
        onSelect();

    draggingHandle = e.eventComponent == this && getHandleArea().contains (local.getPosition());
    repaint();
}

void ModuleCard::mouseDrag (const juce::MouseEvent& e)
{
    if (draggingHandle && onDrag)
        onDrag (e.getDistanceFromDragStartY());
}

void ModuleCard::mouseUp (const juce::MouseEvent& e)
{
    if (! draggingHandle)
        return;

    draggingHandle = false;
    repaint();

    if (onDragEnd)
        onDragEnd (e.getDistanceFromDragStartY());
}

//==============================================================================
AddModuleButton::AddModuleButton() : juce::Button ("Add module")
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void AddModuleButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    if (! isEnabled())
        return;

    const auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const auto colour = highlighted ? colours::accent : colours::textMuted;

    if (highlighted || down)
    {
        g.setColour (colours::accent.withAlpha (down ? 0.10f : 0.05f));
        g.fillRoundedRectangle (bounds, 8.0f);
    }

    juce::Path outline, dashed;
    outline.addRoundedRectangle (bounds, 8.0f);
    const float dashes[] = { 6.0f, 5.0f };
    juce::PathStrokeType (1.2f).createDashedStroke (dashed, outline, dashes, 2);
    g.setColour (colour.withAlpha (0.7f));
    g.fillPath (dashed);

    // "+"
    const auto centre = bounds.getCentre().translated (0.0f, -16.0f);
    g.setColour (highlighted ? colours::accent : colours::text);
    g.fillRect (juce::Rectangle<float> (28.0f, 2.5f).withCentre (centre));
    g.fillRect (juce::Rectangle<float> (2.5f, 28.0f).withCentre (centre));

    g.setColour (colour);
    g.setFont (spacedFont (15.0f, false, 0.3f));
    g.drawText ("ADD MODULE", bounds.withTop (centre.y + 22.0f).withHeight (24.0f), juce::Justification::centred);
}

//==============================================================================
RackPanel::RackPanel()
{
    addButton.onClick = [this] {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());
        const auto types = getAvailableModuleTypes();

        for (size_t i = 0; i < types.size(); ++i)
            menu.addItem ((int) i + 1, getModuleTypeName (types[i]));

        juce::Component::SafePointer<RackPanel> safeThis (this);
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addButton), [safeThis, types] (int result) {
            if (safeThis != nullptr && result > 0 && safeThis->onAdd)
                safeThis->onAdd (types[(size_t) result - 1]);
        });
    };

    addAndMakeVisible (addButton);
}

void RackPanel::setRack (const std::vector<std::shared_ptr<Module>>& modules, int selectedIndex, bool canAddMore)
{
    cards.clear();

    for (int i = 0; i < (int) modules.size(); ++i)
    {
        auto card = std::make_unique<ModuleCard> (modules[(size_t) i], i, (int) modules.size());
        auto* raw = card.get();

        card->setSelected (i == selectedIndex);
        card->onSelect = [this, i] { if (onSelect) onSelect (i); };
        card->onRemove = [this, i] { if (onRemove) onRemove (i); };
        card->onMoveTo = [this, i] (int to) { if (onMove) onMove (i, to); };
        card->onDrag = [this, raw] (int dy) { handleDrag (*raw, dy); };
        card->onDragEnd = [this, raw] (int dy) { handleDragEnd (*raw, dy); };

        addAndMakeVisible (*card);
        cards.push_back (std::move (card));
    }

    addButton.setVisible (canAddMore);
    layoutForWidth (getWidth());
}

void RackPanel::refreshCards()
{
    for (auto& card : cards)
        card->refresh();
}

void RackPanel::setSelectedIndex (int selectedIndex)
{
    for (auto& card : cards)
        card->setSelected (card->getIndex() == selectedIndex);
}

juce::Point<int> RackPanel::slotPosition (int slot) const noexcept
{
    return { 0, slot * (cardHeight + gap) };
}

void RackPanel::layoutForWidth (int width)
{
    const auto addSpace = addButton.isVisible() ? addHeight : 0;
    const auto height = (int) cards.size() * (cardHeight + gap) + addSpace;
    setSize (width, juce::jmax (height, 1));
    resized();
}

void RackPanel::resized()
{
    for (int i = 0; i < (int) cards.size(); ++i)
        cards[(size_t) i]->setBounds (juce::Rectangle<int> (getWidth(), cardHeight) + slotPosition (i));

    addButton.setBounds (juce::Rectangle<int> (getWidth(), addHeight) + slotPosition ((int) cards.size()));
}

void RackPanel::handleDrag (ModuleCard& card, int dy)
{
    const auto base = slotPosition (card.getIndex());
    const auto maxY = slotPosition ((int) cards.size() - 1).y;
    card.setTopLeftPosition (0, juce::jlimit (0, maxY, base.y + dy));
    card.toFront (false);
}

void RackPanel::handleDragEnd (ModuleCard& card, int dy)
{
    const auto from = card.getIndex();
    const auto to = juce::jlimit (0, (int) cards.size() - 1,
                                  juce::roundToInt ((float) (slotPosition (from).y + dy) / (float) (cardHeight + gap)));

    if (to != from && onMove)
        onMove (from, to); // el rack se reconstruye al cambiar
    else
        resized();
}

} // namespace flow::ui
