#include "PluginEditor.h"
#include "Presets.h"
#include "RackPresets.h"

namespace
{
    namespace colours = flow::ui::colours;

    constexpr int headerHeight = 100;
    constexpr int toolbarHeight = 70;
    constexpr int controlsHeight = 186;
    constexpr int userPresetIdOffset = 1000;
    constexpr int margin = 18;

    const juce::StringArray gridNames { "1 Beat", "2 Beats", "4 Beats", "8 Beats" };
    const juce::StringArray snapNames { "Off", "1/4", "1/8", "1/16", "1/32", "1/8 T", "1/16 T" };
    const juce::StringArray curveNames { "Hard", "Smooth", "Soft" };

    template <typename Array>
    int indexOf (const Array& options, int value)
    {
        for (int i = 0; i < (int) std::size (options); ++i)
            if ((int) options[i] == value)
                return i;

        return 0;
    }

    void fillCombo (juce::ComboBox& box, const juce::StringArray& names)
    {
        for (int i = 0; i < names.size(); ++i)
            box.addItem (names[i], i + 1);
    }
}

FlowEditor::FlowEditor (FlowProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setLookAndFeel (&lookAndFeel);

    // Encabezado: presets generales (todo el rack)
    rackPresetSelector.getComboBox().onBeforePopup = [this] { refreshRackPresetList(); };
    rackPresetSelector.onSelect = [this] (int itemId) { loadRackPreset (itemId); };
    addAndMakeVisible (rackPresetSelector);
    refreshRackPresetList();

    saveButton.setTooltip ("Save preset");
    saveButton.onClick = [this] { saveRackPreset(); };
    addAndMakeVisible (saveButton);

    menuButton.setTooltip ("Menu");
    menuButton.onClick = [this] { showMenu(); };
    addAndMakeVisible (menuButton);

    // Rack
    rackViewport.setViewedComponent (&rackPanel, false);
    rackViewport.setScrollBarsShown (true, false);
    rackViewport.setScrollBarThickness (8);
    addAndMakeVisible (rackViewport);

    auto later = [this] (std::function<void()> action) {
        // Diferimos los cambios que reconstruyen el rack para no destruir
        // la tarjeta que disparó el evento mientras todavía lo está procesando.
        juce::Component::SafePointer<FlowEditor> safeThis (this);
        juce::MessageManager::callAsync ([safeThis, action] {
            if (safeThis != nullptr)
            {
                action();
                safeThis->rebuildRack();
            }
        });
    };

    rackPanel.onSelect = [this] (int index) { selectModule (index); };
    rackPanel.onRemove = [this, later] (int index) { later ([this, index] { processor.removeModule (index); }); };
    rackPanel.onMove = [this, later] (int from, int to) { later ([this, from, to] { processor.moveModule (from, to); }); };
    rackPanel.onAdd = [this, later] (flow::ModuleType type) { later ([this, type] { processor.addModule (type); }); };

    // Barra de la grilla
    fillCombo (gridBox, gridNames);
    fillCombo (snapBox, snapNames);
    fillCombo (curveBox, curveNames);

    gridBox.onChange = [this] {
        processor.changePatternBeats (FlowProcessor::gridOptions[juce::jmax (0, gridBox.getSelectedItemIndex())]);
        envelopeEditor.setGrid (processor.getPatternBeats(), processor.getSnapPerBeat());
    };
    snapBox.onChange = [this] {
        processor.setSnapPerBeat (FlowProcessor::snapOptions[juce::jmax (0, snapBox.getSelectedItemIndex())]);
        envelopeEditor.setGrid (processor.getPatternBeats(), processor.getSnapPerBeat());
    };
    curveBox.onChange = [this] { processor.setCurveMode (juce::jmax (0, curveBox.getSelectedItemIndex())); };

    curveBox.setTooltip ("How fast the level follows the envelope (Hard = sharp cuts, Soft = smoother transitions)");
    gridBox.setTooltip ("Pattern length");

    for (auto* box : { &gridBox, &snapBox, &curveBox })
        addAndMakeVisible (*box);

    envelopeEditor.onChange = [this] (const std::vector<beatshaper::Node>& nodes) {
        if (auto module = getSelectedModule())
        {
            module->envelope.setNodes (nodes);
            module->presetName = {};
            shownEnvelopeVersion = module->envelope.getVersion();
            moduleControls.refreshPreset();
        }
    };
    addAndMakeVisible (envelopeEditor);

    moduleControls.getPatternBeats = [this] { return processor.getPatternBeats(); };
    moduleControls.onPresetApplied = [this] {
        if (const auto module = getSelectedModule())
        {
            envelopeEditor.setNodes (module->envelope.getNodes());
            shownEnvelopeVersion = module->envelope.getVersion();
        }
    };
    addAndMakeVisible (moduleControls);

    setResizable (true, true);
    setResizeLimits (920, 700, 1800, 1300);
    setSize (1100, 820);

    rebuildRack();
    startTimerHz (30);
}

FlowEditor::~FlowEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
std::shared_ptr<flow::Module> FlowEditor::getSelectedModule() const
{
    const auto index = processor.getSelectedModule();
    return juce::isPositiveAndBelow (index, (int) rack.size()) ? rack[(size_t) index] : nullptr;
}

void FlowEditor::rebuildRack()
{
    rack = processor.getRack();
    shownRackVersion = processor.getRackVersion();

    rackPanel.setRack (rack, processor.getSelectedModule(), (int) rack.size() < FlowProcessor::maxModules);
    rackPanel.layoutForWidth (rackViewport.getMaximumVisibleWidth());

    gridBox.setSelectedItemIndex (indexOf (FlowProcessor::gridOptions, processor.getPatternBeats()), juce::dontSendNotification);
    snapBox.setSelectedItemIndex (indexOf (FlowProcessor::snapOptions, processor.getSnapPerBeat()), juce::dontSendNotification);
    curveBox.setSelectedItemIndex (processor.getCurveMode(), juce::dontSendNotification);
    envelopeEditor.setGrid (processor.getPatternBeats(), processor.getSnapPerBeat());

    showSelectedModule();
}

void FlowEditor::selectModule (int index)
{
    if (index == processor.getSelectedModule())
        return;

    processor.setSelectedModule (index);
    showSelectedModule();
}

void FlowEditor::showSelectedModule()
{
    shownSelected = processor.getSelectedModule();
    rackPanel.setSelectedIndex (shownSelected);

    const auto module = getSelectedModule();
    envelopeEditor.setVisible (module != nullptr);
    moduleControls.setVisible (module != nullptr);
    moduleControls.setModule (module);

    if (module != nullptr)
    {
        envelopeEditor.setNodes (module->envelope.getNodes());
        shownEnvelopeVersion = module->envelope.getVersion();
    }

    rackPresetSelector.showName (processor.getRackPresetName());
    repaint();
}

//==============================================================================
void FlowEditor::refreshRackPresetList()
{
    auto& combo = rackPresetSelector.getComboBox();
    combo.clear (juce::dontSendNotification);

    combo.addSectionHeading ("Factory");
    const auto& factory = flow::getFactoryRackPresets();

    for (int i = 0; i < (int) factory.size(); ++i)
        combo.addItem (factory[(size_t) i].name, i + 1);

    userPresetFiles = flow::getUserRackPresets();

    if (! userPresetFiles.isEmpty())
    {
        combo.addSectionHeading ("User");

        for (int i = 0; i < userPresetFiles.size(); ++i)
            combo.addItem (userPresetFiles[i].getFileNameWithoutExtension(), userPresetIdOffset + i);
    }

    rackPresetSelector.showName (processor.getRackPresetName());
}

void FlowEditor::loadRackPreset (int itemId)
{
    if (itemId >= userPresetIdOffset)
    {
        if (juce::isPositiveAndBelow (itemId - userPresetIdOffset, userPresetFiles.size()))
            loadRackPresetFile (userPresetFiles[itemId - userPresetIdOffset]);
        return;
    }

    processor.loadFactoryRackPreset (itemId - 1);
    rebuildRack();
}

void FlowEditor::loadRackPresetFile (const juce::File& file)
{
    if (auto xml = juce::XmlDocument::parse (file))
    {
        processor.restoreStateTree (juce::ValueTree::fromXml (*xml));
        processor.setRackPresetName (file.getFileNameWithoutExtension());
        rebuildRack();
    }
}

void FlowEditor::saveRackPreset()
{
    // Pide un nombre y guarda el rack en Documentos/Flow/Presets.
    saveDialog = std::make_unique<juce::AlertWindow> ("Save preset", "Name of the preset:", juce::MessageBoxIconType::NoIcon, this);
    saveDialog->setLookAndFeel (&lookAndFeel);

    const auto current = processor.getRackPresetName();
    saveDialog->addTextEditor ("name", current == "Init" ? juce::String ("My preset") : current);
    saveDialog->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveDialog->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<FlowEditor> safeThis (this);
    saveDialog->enterModalState (true, juce::ModalCallbackFunction::create ([safeThis] (int result) {
        if (safeThis == nullptr || safeThis->saveDialog == nullptr)
            return;

        const auto name = juce::File::createLegalFileName (safeThis->saveDialog->getTextEditorContents ("name").trim());
        safeThis->saveDialog.reset();

        if (result != 1 || name.isEmpty())
            return;

        const auto folder = flow::getUserPresetFolder();
        folder.createDirectory();
        safeThis->processor.setRackPresetName (name);

        if (auto xml = safeThis->processor.createStateTree().createXml())
            xml->writeTo (folder.getChildFile (name + flow::presetFileExtension));

        safeThis->refreshRackPresetList();
    }));
}

void FlowEditor::showMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&lookAndFeel);
    menu.addItem (1, "Save preset");
    menu.addItem (2, "Export preset to file...");
    menu.addItem (3, "Import preset from file...");
    menu.addItem (4, "Open presets folder");
    menu.addSeparator();
    menu.addItem (5, "Clear pattern of selected module", getSelectedModule() != nullptr);
    menu.addItem (6, "Reset rack (Init)");

    juce::Component::SafePointer<FlowEditor> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton), [safeThis] (int result) {
        if (safeThis == nullptr)
            return;

        switch (result)
        {
            case 1: safeThis->saveRackPreset(); break;
            case 2: safeThis->exportPresetFile(); break;
            case 3: safeThis->importPresetFile(); break;
            case 4:
            {
                const auto folder = flow::getUserPresetFolder();
                folder.createDirectory();
                folder.startAsProcess();
                break;
            }
            case 5: safeThis->moduleControls.applyPreset ("Init"); break;
            case 6: safeThis->processor.loadFactoryRackPreset (0); safeThis->rebuildRack(); break;
            default: break;
        }
    });
}

void FlowEditor::exportPresetFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Export Flow preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile (processor.getRackPresetName() + flow::presetFileExtension),
        juce::String ("*") + flow::presetFileExtension);

    juce::Component::SafePointer<FlowEditor> safeThis (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis] (const juce::FileChooser& chooser) {
                                  const auto file = chooser.getResult();

                                  if (safeThis == nullptr || file == juce::File())
                                      return;

                                  if (auto xml = safeThis->processor.createStateTree().createXml())
                                      xml->writeTo (file.withFileExtension (flow::presetFileExtension));
                              });
}

void FlowEditor::importPresetFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Import Flow preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        juce::String ("*") + flow::presetFileExtension);

    juce::Component::SafePointer<FlowEditor> safeThis (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser) {
                                  const auto file = chooser.getResult();

                                  if (safeThis != nullptr && file.existsAsFile())
                                      safeThis->loadRackPresetFile (file);
                              });
}

//==============================================================================
void FlowEditor::timerCallback()
{
    processor.collectGarbage();

    // Cambios hechos fuera del editor (estado cargado por el host, etc.).
    if (processor.getRackVersion() != shownRackVersion)
    {
        rebuildRack();
    }
    else if (processor.getSelectedModule() != shownSelected)
    {
        showSelectedModule();
    }

    if (const auto module = getSelectedModule())
    {
        if (module->envelope.getVersion() != shownEnvelopeVersion)
        {
            shownEnvelopeVersion = module->envelope.getVersion();
            envelopeEditor.setNodes (module->envelope.getNodes());
        }

        envelopeEditor.setPlayhead (processor.getDisplayPhase(), module->getCurrentEnvelopeValue(),
                                    module->enabled.load(), processor.isClockRunning());
    }

    moduleControls.refresh();
}

//==============================================================================
void FlowEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background.darker (0.25f));

    const auto frame = getLocalBounds().toFloat().reduced (6.0f);
    g.setColour (colours::background);
    g.fillRoundedRectangle (frame, 10.0f);
    g.setColour (colours::outline.withAlpha (0.6f));
    g.drawRoundedRectangle (frame, 10.0f, 1.0f);

    // Encabezado
    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (margin + 20, 0);
    g.setColour (colours::panel.withAlpha (0.6f));
    g.fillRect (frame.withHeight ((float) headerHeight - 6.0f).reduced (1.0f, 0.0f).withTrimmedTop (1.0f));
    flow::ui::drawLogo (g, header.removeFromLeft (460).toFloat().withSizeKeepingCentre (460.0f, 42.0f));

    g.setColour (colours::outline);
    const auto dividerX = (float) rackPresetSelector.getX() - 130.0f;
    g.drawLine (dividerX, 28.0f, dividerX, (float) headerHeight - 28.0f, 1.0f);
    const auto secondDivider = (float) menuButton.getX() - 26.0f;
    g.drawLine (secondDivider, 28.0f, secondDivider, (float) headerHeight - 28.0f, 1.0f);

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (19.0f));
    g.drawText ("Preset", juce::Rectangle<float> (dividerX + 30.0f, 0.0f, 100.0f, (float) headerHeight),
                juce::Justification::centredLeft);

    g.setColour (colours::outline.withAlpha (0.7f));
    g.drawHorizontalLine (headerHeight - 6, frame.getX(), frame.getRight());

    // Panel derecho y etiquetas de la barra
    const auto rightPanel = rightPanelBounds.toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (rightPanel, 8.0f);

    g.setColour (colours::text.withAlpha (0.85f));
    g.setFont (juce::FontOptions (17.0f));

    for (auto [box, text] : { std::pair { &gridBox, "Grid" }, std::pair { &snapBox, "Snap" }, std::pair { &curveBox, "Curve" } })
        g.drawText (text, box->getBounds().withX (box->getX() - 70).withWidth (60), juce::Justification::centredRight);

    g.setColour (colours::outline);

    for (auto* box : { &snapBox, &curveBox })
        g.drawVerticalLine (box->getX() - 86, (float) box->getY() + 4.0f, (float) box->getBottom() - 4.0f);

    // Separador entre rack y grilla

    if (getSelectedModule() == nullptr)
    {
        g.setColour (colours::textMuted);
        g.setFont (juce::FontOptions (16.0f));
        g.drawText ("Add a module to start shaping", rightPanel, juce::Justification::centred);
    }
}

void FlowEditor::resized()
{
    auto bounds = getLocalBounds().reduced (margin);

    // Encabezado
    auto header = bounds.removeFromTop (headerHeight - margin).withTrimmedTop (4);
    menuButton.setBounds (header.removeFromRight (44).withSizeKeepingCentre (44, 44));
    header.removeFromRight (52);
    saveButton.setBounds (header.removeFromRight (40).withSizeKeepingCentre (40, 40));
    header.removeFromRight (38);
    header.removeFromLeft (460);

    const auto presetWidth = juce::jlimit (260, 620, header.getWidth() - 150);
    rackPresetSelector.setBounds (header.removeFromRight (presetWidth).withSizeKeepingCentre (presetWidth, 50));

    bounds.removeFromTop (margin);

    // Columna izquierda: rack
    const auto rackWidth = juce::jlimit (300, 520, (int) ((float) bounds.getWidth() * 0.34f));
    rackViewport.setBounds (bounds.removeFromLeft (rackWidth));
    rackPanel.layoutForWidth (rackViewport.getMaximumVisibleWidth());
    bounds.removeFromLeft (margin);
    rightPanelBounds = bounds.withTrimmedTop (-6);
    bounds.removeFromLeft (22);

    // Columna derecha
    bounds.removeFromRight (22);
    auto toolbar = bounds.removeFromTop (toolbarHeight).reduced (0, 10);
    const auto comboWidth = juce::jmax (110, (toolbar.getWidth() - 64 - 2 * 86) / 3);

    for (auto* box : { &gridBox, &snapBox, &curveBox })
    {
        toolbar.removeFromLeft (box == &gridBox ? 64 : 86);
        box->setBounds (toolbar.removeFromLeft (comboWidth).withSizeKeepingCentre (comboWidth, 46));
    }

    moduleControls.setBounds (bounds.removeFromBottom (controlsHeight).withTrimmedBottom (4));
    bounds.removeFromBottom (margin);
    envelopeEditor.setBounds (bounds);
}
