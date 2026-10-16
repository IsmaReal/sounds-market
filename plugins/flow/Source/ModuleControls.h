#pragma once

#include "FlowLookAndFeel.h"
#include "Modules.h"

#include <memory>

namespace flow::ui
{

// Panel inferior "MODULE CONTROLS" con los parámetros propios del módulo seleccionado.
class ModuleControls final : public juce::Component
{
public:
    ModuleControls();

    void setModule (std::shared_ptr<Module> moduleToShow);
    void refresh();

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
};

} // namespace flow::ui
