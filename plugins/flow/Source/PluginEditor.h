#pragma once

#include "EnvelopeEditor.h"
#include "FlowLookAndFeel.h"
#include "ModuleControls.h"
#include "PluginProcessor.h"
#include "RackPanel.h"

class FlowEditor final : public juce::AudioProcessorEditor,
                         private juce::Timer
{
public:
    explicit FlowEditor (FlowProcessor&);
    ~FlowEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    void rebuildRack();
    void selectModule (int index);
    void showSelectedModule();
    std::shared_ptr<flow::Module> getSelectedModule() const;

    void applyPreset (int index);
    void updatePresetBox();
    void showMenu();
    void savePresetFile();
    void loadPresetFile();

    FlowProcessor& processor;
    flow::ui::FlowLookAndFeel lookAndFeel;

    // Encabezado
    juce::ComboBox presetBox;
    flow::ui::IconButton previousPresetButton { flow::ui::IconButton::Icon::previous, "Previous preset" };
    flow::ui::IconButton saveButton { flow::ui::IconButton::Icon::save, "Save" };
    flow::ui::IconButton menuButton { flow::ui::IconButton::Icon::menu, "Menu" };

    // Rack
    juce::Viewport rackViewport;
    flow::ui::RackPanel rackPanel;

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
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowEditor)
};
