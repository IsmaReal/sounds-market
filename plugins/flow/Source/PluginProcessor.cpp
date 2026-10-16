#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "RackPresets.h"

namespace
{
    const juce::Identifier stateId { "FlowState" };
    const juce::Identifier moduleId { "Module" };
    const juce::Identifier patternId { "Pattern" };
    const juce::Identifier nodeId { "Node" };
    const juce::Identifier typeId { "type" };
    const juce::Identifier enabledId { "enabled" };
    const juce::Identifier mixId { "mix" };
    const juce::Identifier xId { "x" };
    const juce::Identifier yId { "y" };
    const juce::Identifier curveId { "curve" };
    const juce::Identifier gridId { "grid" };
    const juce::Identifier snapId { "snap" };
    const juce::Identifier curveModeId { "curveMode" };
    const juce::Identifier selectedId { "selected" };
    const juce::Identifier versionId { "version" };
    const juce::Identifier presetId { "preset" };
    const juce::Identifier rackPresetId { "rackPreset" };
    const juce::Identifier dryWetId { "dryWet" };

    juce::ValueTree nodesToTree (const std::vector<beatshaper::Node>& nodes)
    {
        juce::ValueTree pattern (patternId);

        for (const auto& n : nodes)
        {
            juce::ValueTree node (nodeId);
            node.setProperty (xId, n.x, nullptr);
            node.setProperty (yId, n.y, nullptr);
            node.setProperty (curveId, n.curve, nullptr);
            pattern.appendChild (node, nullptr);
        }

        return pattern;
    }

    std::vector<beatshaper::Node> treeToNodes (const juce::ValueTree& tree)
    {
        std::vector<beatshaper::Node> nodes;

        for (const auto& child : tree)
            if (child.hasType (nodeId))
                nodes.push_back ({ (double) child.getProperty (xId, 0.0),
                                   (float) child.getProperty (yId, 1.0f),
                                   (float) child.getProperty (curveId, 0.0f) });

        return nodes;
    }

    juce::Identifier parameterIdentifier (int index)
    {
        return juce::Identifier ("p" + juce::String (index));
    }
}

FlowProcessor::FlowProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    auto parameter = std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "dryWet", 1 }, "Dry/Wet",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%"));
    dryWet = parameter.get();
    addParameter (parameter.release());

    resetRack();
}

FlowProcessor::~FlowProcessor() = default;

//==============================================================================
void FlowProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = juce::jmax (1, samplesPerBlock);
    phaseBuffer.assign ((size_t) maxBlockSize, 0.0);
    dryBuffer.setSize (juce::jmax (1, getTotalNumOutputChannels()), maxBlockSize);
    dryWetSmoothed.reset (sampleRate, 0.02);
    dryWetSmoothed.setCurrentAndTargetValue (dryWet->get() * 0.01f);
    internalBeatPosition = 0.0;
    activeGain = 1.0f;
    modulesCleared = false;

    const juce::ScopedLock lock (rackLock);

    for (auto& module : rack)
        module->prepare (currentSampleRate, maxBlockSize, getTotalNumOutputChannels());

    publishRack();
}

bool FlowProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

void FlowProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = getTotalNumInputChannels();

    for (auto ch = numChannels; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    // Tomamos la última versión del rack, si hay una nueva y la anterior ya se liberó.
    {
        const juce::SpinLock::ScopedTryLockType lock (chainLock);

        if (lock.isLocked() && pendingChain != nullptr && retiredChain == nullptr)
        {
            retiredChain = std::move (activeChain);
            activeChain = std::move (pendingChain);
        }
    }

    if (numSamples == 0 || phaseBuffer.empty())
        return;

    // Posición en beats: seguimos al host. Si el DAW está en pausa el patrón queda
    // quieto en la posición del cursor; solo corre con reloj propio cuando no hay
    // transporte (por ejemplo, en la versión Standalone).
    bool hasTransport = false;
    bool playing = false;
    double beatPosition = internalBeatPosition;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto position = hostPlayHead->getPosition())
        {
            hasTransport = true;
            playing = position->getIsPlaying();

            if (auto bpm = position->getBpm())
                lastBpm = juce::jlimit (20.0, 999.0, *bpm);

            if (auto ppq = position->getPpqPosition())
                beatPosition = *ppq;
        }
    }

    const auto running = playing || ! hasTransport;
    clockRunning.store (running);

    const auto beatsPerSample = running ? lastBpm / (60.0 * currentSampleRate) : 0.0;
    const auto beats = (double) patternBeats.load();
    const auto smoothingMs = curveOptionsMs[juce::jlimit (0, 2, curveMode.load())];
    const auto smoothing = (float) std::exp (-1.0 / (smoothingMs * 0.001 * currentSampleRate));

    // Si el plugin estuvo puenteado, arrancamos sin restos y con un fundido de entrada.
    if (resetPending.exchange (false))
    {
        if (activeChain != nullptr)
            for (auto& module : *activeChain)
                module->reset();

        activeGain = 0.0f;
    }

    // En pausa los efectos se apagan (fundido de ~10 ms) y se borran sus colas.
    const auto activeTarget = running ? 1.0f : 0.0f;
    const auto activeStep = (float) (1.0 / (0.01 * currentSampleRate));
    dryWetSmoothed.setTargetValue (dryWet->get() * 0.01f);

    // Procesamos en tramos de como mucho maxBlockSize por si el host manda bloques más grandes.
    for (int start = 0; start < numSamples; start += maxBlockSize)
    {
        const auto length = juce::jmin (maxBlockSize, numSamples - start);

        for (int i = 0; i < length; ++i)
        {
            auto inPattern = std::fmod (beatPosition, beats);

            if (inPattern < 0.0)
                inPattern += beats;

            phaseBuffer[(size_t) i] = inPattern / beats;
            beatPosition += beatsPerSample;
        }

        displayPhase.store ((float) phaseBuffer[(size_t) length - 1]);

        const auto fullyOff = activeGain <= 0.0f && activeTarget <= 0.0f;

        if (fullyOff || activeChain == nullptr)
        {
            // Los efectos no actúan: la señal pasa limpia. Borramos las colas una sola vez.
            if (fullyOff && ! modulesCleared && activeChain != nullptr)
            {
                for (auto& module : *activeChain)
                    module->reset();

                modulesCleared = true;
            }

            dryWetSmoothed.skip (length);
            continue;
        }

        modulesCleared = false;
        juce::AudioBuffer<float> section (buffer.getArrayOfWritePointers(), numChannels, start, length);

        for (int ch = 0; ch < numChannels; ++ch)
            dryBuffer.copyFrom (ch, 0, section, ch, 0, length);

        for (auto& module : *activeChain)
            module->process (section, phaseBuffer.data(), length, smoothing);

        // Mezcla final: seco + (procesado - seco) * Dry/Wet * fundido de pausa.
        for (int i = 0; i < length; ++i)
        {
            if (activeGain < activeTarget)
                activeGain = juce::jmin (activeTarget, activeGain + activeStep);
            else if (activeGain > activeTarget)
                activeGain = juce::jmax (activeTarget, activeGain - activeStep);

            const auto wet = dryWetSmoothed.getNextValue() * activeGain;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                const auto dry = dryBuffer.getSample (ch, i);
                auto* out = section.getWritePointer (ch);
                out[i] = dry + (out[i] - dry) * wet;
            }
        }
    }

    internalBeatPosition = std::fmod (beatPosition, beats);
}

void FlowProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    // Puenteado: la señal pasa tal cual y, al volver, los efectos arrancan sin colas viejas.
    for (auto ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    resetPending.store (true);
}

//==============================================================================
FlowProcessor::ModulePtr FlowProcessor::createPreparedModule (flow::ModuleType type) const
{
    auto module = flow::createModule (type);

    if (module != nullptr)
    {
        module->updatePatternForGrid (getPatternBeats());
        module->prepare (currentSampleRate, maxBlockSize, getTotalNumOutputChannels());
    }

    return module;
}

void FlowProcessor::publishRack()
{
    auto next = std::make_unique<Chain> (rack);
    std::unique_ptr<Chain> unusedPending, finishedRetired;

    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        unusedPending = std::move (pendingChain);
        finishedRetired = std::move (retiredChain);
        pendingChain = std::move (next);
    }

    // unusedPending y finishedRetired se destruyen acá, fuera del hilo de audio.
    ++rackVersion;
}

void FlowProcessor::collectGarbage()
{
    std::unique_ptr<Chain> finishedRetired;

    {
        const juce::SpinLock::ScopedLockType lock (chainLock);
        finishedRetired = std::move (retiredChain);
    }
}

FlowProcessor::Chain FlowProcessor::getRack() const
{
    const juce::ScopedLock lock (rackLock);
    return rack;
}

bool FlowProcessor::addModule (flow::ModuleType type)
{
    const juce::ScopedLock lock (rackLock);

    if ((int) rack.size() >= maxModules)
        return false;

    auto module = createPreparedModule (type);

    if (module == nullptr)
        return false;

    rack.push_back (std::move (module));
    selectedModule.store ((int) rack.size() - 1);
    publishRack();
    return true;
}

void FlowProcessor::removeModule (int index)
{
    const juce::ScopedLock lock (rackLock);

    if (! juce::isPositiveAndBelow (index, (int) rack.size()))
        return;

    rack.erase (rack.begin() + index);

    const auto selected = selectedModule.load();

    if (selected > index || selected >= (int) rack.size())
        selectedModule.store (juce::jmax (0, selected - 1));

    publishRack();
}

void FlowProcessor::moveModule (int fromIndex, int toIndex)
{
    const juce::ScopedLock lock (rackLock);

    const auto size = (int) rack.size();
    toIndex = juce::jlimit (0, size - 1, toIndex);

    if (! juce::isPositiveAndBelow (fromIndex, size) || fromIndex == toIndex)
        return;

    auto module = rack[(size_t) fromIndex];
    rack.erase (rack.begin() + fromIndex);
    rack.insert (rack.begin() + toIndex, module);

    // La selección sigue al módulo que estaba seleccionado.
    const auto selected = selectedModule.load();

    if (selected == fromIndex)
        selectedModule.store (toIndex);
    else if (fromIndex < selected && toIndex >= selected)
        selectedModule.store (selected - 1);
    else if (fromIndex > selected && toIndex <= selected)
        selectedModule.store (selected + 1);

    publishRack();
}

void FlowProcessor::resetRack()
{
    const juce::ScopedLock lock (rackLock);
    rackPresetName = "Init";
    rack.clear();
    rack.push_back (createPreparedModule (flow::ModuleType::volume));
    selectedModule.store (0);
    publishRack();
}

void FlowProcessor::setSelectedModule (int index) noexcept
{
    const juce::ScopedLock lock (rackLock);

    if (juce::isPositiveAndBelow (index, (int) rack.size()))
        selectedModule.store (index);
}

//==============================================================================
juce::ValueTree FlowProcessor::createStateTree() const
{
    juce::ValueTree state (stateId);
    state.setProperty (versionId, 1, nullptr);
    state.setProperty (gridId, patternBeats.load(), nullptr);
    state.setProperty (snapId, snapPerBeat.load(), nullptr);
    state.setProperty (curveModeId, curveMode.load(), nullptr);
    state.setProperty (selectedId, selectedModule.load(), nullptr);
    state.setProperty (rackPresetId, getRackPresetName(), nullptr);
    state.setProperty (dryWetId, dryWet->get(), nullptr);

    for (const auto& module : getRack())
    {
        juce::ValueTree m (moduleId);
        m.setProperty (typeId, flow::moduleTypeToId (module->getType()), nullptr);
        m.setProperty (enabledId, module->enabled.load(), nullptr);
        m.setProperty (mixId, module->mix.load(), nullptr);

        for (int i = 0; i < (int) module->getParameterSpecs().size(); ++i)
            m.setProperty (parameterIdentifier (i), module->getParameter (i), nullptr);

        m.setProperty (presetId, module->presetName, nullptr);
        m.appendChild (nodesToTree (module->envelope.getNodes()), nullptr);
        state.appendChild (m, nullptr);
    }

    return state;
}

void FlowProcessor::restoreStateTree (const juce::ValueTree& state)
{
    if (! state.hasType (stateId))
        return;

    Chain newRack;

    for (const auto& m : state)
    {
        auto type = flow::ModuleType::volume;

        if (! m.hasType (moduleId) || ! flow::moduleTypeFromId (m.getProperty (typeId).toString(), type))
            continue;

        auto module = createPreparedModule (type);
        module->enabled.store ((bool) m.getProperty (enabledId, true));
        module->mix.store (juce::jlimit (0.0f, 1.0f, (float) m.getProperty (mixId, 1.0f)));

        for (int i = 0; i < (int) module->getParameterSpecs().size(); ++i)
            module->setParameter (i, (float) m.getProperty (parameterIdentifier (i), module->getParameter (i)));

        const auto pattern = m.getChildWithName (patternId);

        if (pattern.isValid())
        {
            module->envelope.setNodes (treeToNodes (pattern));
            module->presetName = m.getProperty (presetId).toString();
        }

        newRack.push_back (std::move (module));

        if ((int) newRack.size() >= maxModules)
            break;
    }

    if (newRack.empty())
        newRack.push_back (createPreparedModule (flow::ModuleType::volume));

    setPatternBeats ((int) state.getProperty (gridId, 4));
    setSnapPerBeat ((int) state.getProperty (snapId, 4));
    setCurveMode ((int) state.getProperty (curveModeId, 1));
    dryWet->setValueNotifyingHost (dryWet->convertTo0to1 ((float) state.getProperty (dryWetId, 100.0f)));

    const juce::ScopedLock lock (rackLock);
    rackPresetName = state.getProperty (rackPresetId, "Init").toString();
    rack = std::move (newRack);
    selectedModule.store (juce::jlimit (0, (int) rack.size() - 1, (int) state.getProperty (selectedId, 0)));
    publishRack();
}

void FlowProcessor::loadFactoryRackPreset (int index)
{
    const auto& presets = flow::getFactoryRackPresets();

    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;

    const auto& preset = presets[(size_t) index];

    if (preset.xml.isEmpty())
        resetRack();
    else if (auto xml = juce::parseXML (preset.xml))
        restoreStateTree (juce::ValueTree::fromXml (*xml));

    setRackPresetName (preset.name);
}

juce::String FlowProcessor::getRackPresetName() const
{
    const juce::ScopedLock lock (rackLock);
    return rackPresetName;
}

void FlowProcessor::setRackPresetName (const juce::String& name)
{
    const juce::ScopedLock lock (rackLock);
    rackPresetName = name;
}

void FlowProcessor::changePatternBeats (int beats)
{
    setPatternBeats (beats);

    const juce::ScopedLock lock (rackLock);

    for (auto& module : rack)
        module->updatePatternForGrid (getPatternBeats());
}

void FlowProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void FlowProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        restoreStateTree (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessorEditor* FlowProcessor::createEditor()
{
    return new FlowEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FlowProcessor();
}
