#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    namespace colours = flow::ui::colours;

    constexpr int headerHeight = 100;
    constexpr int toolbarHeight = 70;
    constexpr int controlsHeight = 178;
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

    // Encabezado
    const auto& presets = flow::getFactoryPresets();

    for (int i = 0; i < (int) presets.size(); ++i)
        presetBox.addItem (presets[(size_t) i].name, i + 1);

    presetBox.setTextWhenNothingSelected ("Custom");
    presetBox.onChange = [this] {
        if (presetBox.getSelectedId() > 0)
            applyPreset (presetBox.getSelectedId() - 1);
    };
    presetBox.setColour (juce::ComboBox::outlineColourId, juce::Colours::transparentBlack);
    presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (presetBox);

    previousPresetButton.setTooltip ("Previous pattern");
    previousPresetButton.onClick = [this] {
        const auto count = (int) flow::getFactoryPresets().size();
        const auto current = presetBox.getSelectedId() - 1;
        applyPreset (current <= 0 ? count - 1 : current - 1);
    };
    addAndMakeVisible (previousPresetButton);

    saveButton.setTooltip ("Save preset file");
    saveButton.onClick = [this] { savePresetFile(); };
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
        processor.setPatternBeats (FlowProcessor::gridOptions[juce::jmax (0, gridBox.getSelectedItemIndex())]);
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
            updatePresetBox();
        }
    };
    addAndMakeVisible (envelopeEditor);
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

    updatePresetBox();
    repaint();
}

//==============================================================================
void FlowEditor::applyPreset (int index)
{
    const auto& presets = flow::getFactoryPresets();
    const auto module = getSelectedModule();

    if (module == nullptr || ! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    module->envelope.setNodes (presets[(size_t) index].nodes);
    module->presetName = presets[(size_t) index].name;
    envelopeEditor.setNodes (module->envelope.getNodes());
    shownEnvelopeVersion = module->envelope.getVersion();
    updatePresetBox();
}

void FlowEditor::updatePresetBox()
{
    const auto module = getSelectedModule();
    const auto& presets = flow::getFactoryPresets();
    auto id = 0;

    if (module != nullptr)
        for (int i = 0; i < (int) presets.size(); ++i)
            if (module->presetName == juce::String (presets[(size_t) i].name))
                id = i + 1;

    presetBox.setSelectedId (id, juce::dontSendNotification);
    presetBox.setEnabled (module != nullptr);
    previousPresetButton.setEnabled (module != nullptr);
}

void FlowEditor::showMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&lookAndFeel);
    menu.addItem (1, "Load preset file...");
    menu.addItem (2, "Save preset file...");
    menu.addSeparator();
    menu.addItem (3, "Clear pattern of selected module", getSelectedModule() != nullptr);
    menu.addItem (4, "Reset rack");

    juce::Component::SafePointer<FlowEditor> safeThis (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuButton), [safeThis] (int result) {
        if (safeThis == nullptr)
            return;

        switch (result)
        {
            case 1: safeThis->loadPresetFile(); break;
            case 2: safeThis->savePresetFile(); break;
            case 3: safeThis->applyPreset (0); break;
            case 4: safeThis->processor.resetRack(); safeThis->rebuildRack(); break;
            default: break;
        }
    });
}

void FlowEditor::savePresetFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Save Flow preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Flow preset.flowpreset"),
        "*.flowpreset");

    juce::Component::SafePointer<FlowEditor> safeThis (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis] (const juce::FileChooser& chooser) {
                                  const auto file = chooser.getResult();

                                  if (safeThis == nullptr || file == juce::File())
                                      return;

                                  if (auto xml = safeThis->processor.createStateTree().createXml())
                                      xml->writeTo (file.withFileExtension ("flowpreset"));
                              });
}

void FlowEditor::loadPresetFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Load Flow preset",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
        "*.flowpreset");

    juce::Component::SafePointer<FlowEditor> safeThis (this);
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser) {
                                  const auto file = chooser.getResult();

                                  if (safeThis == nullptr || ! file.existsAsFile())
                                      return;

                                  if (auto xml = juce::XmlDocument::parse (file))
                                  {
                                      safeThis->processor.restoreStateTree (juce::ValueTree::fromXml (*xml));
                                      safeThis->rebuildRack();
                                  }
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
                                    module->enabled.load());
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
    const auto dividerX = (float) previousPresetButton.getX() - 130.0f;
    g.drawLine (dividerX, 28.0f, dividerX, (float) headerHeight - 28.0f, 1.0f);
    const auto secondDivider = (float) menuButton.getX() - 26.0f;
    g.drawLine (secondDivider, 28.0f, secondDivider, (float) headerHeight - 28.0f, 1.0f);

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (19.0f));
    g.drawText ("Preset", juce::Rectangle<float> (dividerX + 30.0f, 0.0f, 100.0f, (float) headerHeight),
                juce::Justification::centredLeft);

    // Caja del preset (flecha + combo)
    const auto presetArea = previousPresetButton.getBounds().getUnion (presetBox.getBounds()).toFloat();
    g.setColour (colours::field);
    g.fillRoundedRectangle (presetArea, 5.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (presetArea.reduced (0.5f), 5.0f, 1.0f);
    g.drawVerticalLine (previousPresetButton.getRight(), presetArea.getY() + 1.0f, presetArea.getBottom() - 1.0f);

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
    auto presetArea = header.removeFromRight (presetWidth).withSizeKeepingCentre (presetWidth, 50);
    previousPresetButton.setBounds (presetArea.removeFromLeft (52));
    presetBox.setBounds (presetArea.withTrimmedLeft (10));

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
