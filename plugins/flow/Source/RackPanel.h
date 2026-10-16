#pragma once

#include "FlowLookAndFeel.h"
#include "Modules.h"

#include <functional>
#include <memory>

namespace flow::ui
{

// Tarjeta de un módulo en el rack: encendido, nombre, mix y manija para reordenar.
class ModuleCard final : public juce::Component
{
public:
    ModuleCard (std::shared_ptr<Module> moduleToShow, int indexInRack, int rackSize);

    void setSelected (bool shouldBeSelected);
    void refresh();
    int getIndex() const noexcept { return index; }

    std::function<void()> onSelect;
    std::function<void()> onRemove;
    std::function<void (int)> onMoveTo;     // menú contextual (nuevo índice)
    std::function<void (int)> onDrag;       // desplazamiento vertical mientras se arrastra
    std::function<void (int)> onDragEnd;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> getHandleArea() const noexcept;

    std::shared_ptr<Module> module;
    int index = 0, numModules = 1;
    bool selected = false;
    bool draggingHandle = false;

    IconButton powerButton { IconButton::Icon::power, "Power" };
    IconButton removeButton { IconButton::Icon::close, "Remove" };
    juce::Slider mixSlider;
};

// Botón punteado "+ ADD MODULE".
class AddModuleButton final : public juce::Button
{
public:
    AddModuleButton();

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// Panel flotante para elegir qué módulo agregar.
class ModulePicker final : public juce::Component
{
public:
    explicit ModulePicker (int width);

    std::function<void (ModuleType)> onPick;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> getTileBounds (int index) const noexcept;
    int getTileAt (juce::Point<int> position) const noexcept;

    static constexpr int headerHeight = 36;
    static constexpr int tileHeight = 78;
    static constexpr int gap = 8;

    std::vector<ModuleType> types;
    int hovered = -1;
};

// Lista vertical de módulos (va dentro de un Viewport).
class RackPanel final : public juce::Component
{
public:
    RackPanel();

    void setRack (const std::vector<std::shared_ptr<Module>>& modules, int selectedIndex, bool canAddMore);
    void refreshCards();
    void setSelectedIndex (int selectedIndex);
    void layoutForWidth (int width);

    std::function<void (int)> onSelect;
    std::function<void (int)> onRemove;
    std::function<void (int, int)> onMove;
    std::function<void (ModuleType)> onAdd;

    void resized() override;

private:
    juce::Point<int> slotPosition (int slot) const noexcept;
    void handleDrag (ModuleCard& card, int dy);
    void handleDragEnd (ModuleCard& card, int dy);

    std::vector<std::unique_ptr<ModuleCard>> cards;
    AddModuleButton addButton;

    static constexpr int cardHeight = 122;
    static constexpr int gap = 14;
    static constexpr int addHeight = 135;
};

} // namespace flow::ui
