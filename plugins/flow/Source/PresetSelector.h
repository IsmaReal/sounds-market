#pragma once

#include "FlowLookAndFeel.h"

#include <functional>

namespace flow::ui
{

// ComboBox que avisa antes de abrir la lista (para refrescar los presets del usuario).
class RefreshingComboBox final : public juce::ComboBox
{
public:
    std::function<void()> onBeforePopup;

    void showPopup() override
    {
        if (onBeforePopup)
            onBeforePopup();

        juce::ComboBox::showPopup();
    }
};

// Selector de presets: [<] [nombre ▾] [>] dentro de una caja.
class PresetSelector final : public juce::Component
{
public:
    PresetSelector();

    RefreshingComboBox& getComboBox() noexcept { return combo; }

    // Selecciona el ítem con ese texto, o muestra el texto tal cual si no está en la lista.
    void showName (const juce::String& name);

    // Pasa al ítem anterior / siguiente de la lista (saltando los títulos de sección).
    void step (int delta);

    std::function<void (int itemId)> onSelect;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    RefreshingComboBox combo;
    IconButton previousButton { IconButton::Icon::previous, "Previous preset" };
    IconButton nextButton { IconButton::Icon::next, "Next preset" };
};

} // namespace flow::ui
