#pragma once

#include "FlowLookAndFeel.h"
#include "Modules.h"
#include "PresetSelector.h"

#include <memory>

namespace flow::ui
{

// Panel inferior "MODULE CONTROLS": presets del módulo seleccionado y sus parámetros propios.
class ModuleControls final : public juce::Component
{
public:
    ModuleControls();

    void setModule (std::shared_ptr<Module> moduleToShow);
    void refresh();
    void refreshPreset();

    // Aplica un preset de módulo por nombre (por ejemplo "Init" para limpiar el patrón).
    void applyPreset (const juce::String& name);

    std::function<int()> getPatternBeats;   // largo actual del patrón
    std::function<void()> onPresetApplied;  // para refrescar la grilla

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::String name;
        juce::Slider slider;
    };

    std::shared_ptr<Module> module;
    std::vector<std::unique_ptr<Control>> controls;
    juce::Label hint;
    PresetSelector presetSelector;
    juce::Rectangle<int> labelArea;
};

} // namespace flow::ui
