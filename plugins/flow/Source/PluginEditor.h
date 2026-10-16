#pragma once

#include "EnvelopeEditor.h"
#include "FlowLookAndFeel.h"
#include "ModuleControls.h"
#include "PluginProcessor.h"
#include "PresetSelector.h"
#include "RackPanel.h"

// Toda la interfaz de Flow. El editor la muestra escalada (ver FlowEditor).
class FlowView final : public juce::Component,
                       private juce::Timer
{
public:
    explicit FlowView (FlowProcessor&);
    ~FlowView() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void rebuildRack();
    void selectModule (int index);
    void showSelectedModule();
    std::shared_ptr<flow::Module> getSelectedModule() const;

    // Presets generales (todo el rack)
    void refreshRackPresetList();
    void loadRackPreset (int itemId);
    void loadRackPresetFile (const juce::File& file);
    void saveRackPreset();
    void showMenu();
    void exportPresetFile();
    void importPresetFile();

    FlowProcessor& processor;
    flow::ui::FlowLookAndFeel lookAndFeel;

    // Encabezado
    flow::ui::PresetSelector rackPresetSelector;
    juce::Array<juce::File> userPresetFiles;
    flow::ui::IconButton saveButton { flow::ui::IconButton::Icon::save, "Save" };
    flow::ui::IconButton menuButton { flow::ui::IconButton::Icon::menu, "Menu" };

    // Rack
    juce::Viewport rackViewport;
    flow::ui::RackPanel rackPanel;

    // Dry/Wet general (abajo del rack)
    juce::Slider dryWetSlider;
    std::unique_ptr<juce::SliderParameterAttachment> dryWetAttachment;
    juce::Rectangle<int> masterArea;

    // Grilla
    juce::ComboBox gridBox, snapBox, curveBox;
    EnvelopeEditor envelopeEditor;
    flow::ui::ModuleControls moduleControls;

    juce::Rectangle<int> rightPanelBounds;
    FlowProcessor::Chain rack;
    int shownRackVersion = -1;
    int shownSelected = -1;
    int shownEnvelopeVersion = -1;

    std::unique_ptr<juce::FileChooser> fileChooser;
    std::unique_ptr<juce::AlertWindow> saveDialog;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowView)
};

// Ventana del plugin: muestra FlowView al 90 % de su tamaño de diseño, así todo
// (gráficos, textos y controles) queda proporcionalmente más chico.
class FlowEditor final : public juce::AudioProcessorEditor
{
public:
    static constexpr float uiScale = 0.9f;

    explicit FlowEditor (FlowProcessor&);

    void resized() override;

private:
    FlowView view;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowEditor)
};
