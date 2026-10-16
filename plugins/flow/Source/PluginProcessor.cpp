#include "PluginProcessor.h"
#include "PluginEditor.h"

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
    resetRack();
}

FlowProcessor::~FlowProcessor() = default;

//==============================================================================
void FlowProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    maxBlockSize = juce::jmax (1, samplesPerBlock);
    phaseBuffer.assign ((size_t) maxBlockSize, 0.0);
    internalBeatPosition = 0.0;

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

    // Posición en beats: seguimos al host cuando reproduce; si está detenido,
    // corremos con un reloj interno al tempo del proyecto.
    bool playing = false;
    double beatPosition = internalBeatPosition;

    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto position = hostPlayHead->getPosition())
        {
            if (auto bpm = position->getBpm())
                lastBpm = juce::jlimit (20.0, 999.0, *bpm);

            if (position->getIsPlaying())
            {
                if (auto ppq = position->getPpqPosition())
                {
                    playing = true;
                    beatPosition = *ppq;
                }
            }
        }
    }

    hostPlaying.store (playing);

    const auto beatsPerSample = lastBpm / (60.0 * currentSampleRate);
    const auto beats = (double) patternBeats.load();
    const auto smoothingMs = curveOptionsMs[juce::jlimit (0, 2, curveMode.load())];
    const auto smoothing = (float) std::exp (-1.0 / (smoothingMs * 0.001 * currentSampleRate));

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

        if (activeChain != nullptr)
        {
            juce::AudioBuffer<float> section (buffer.getArrayOfWritePointers(), numChannels, start, length);

            for (auto& module : *activeChain)
                module->process (section, phaseBuffer.data(), length, smoothing);
        }

        displayPhase.store ((float) phaseBuffer[(size_t) length - 1]);
    }

    internalBeatPosition = std::fmod (beatPosition, beats);
}

//==============================================================================
FlowProcessor::ModulePtr FlowProcessor::createPreparedModule (flow::ModuleType type) const
{
    auto module = flow::createModule (type);

    if (module != nullptr)
        module->prepare (currentSampleRate, maxBlockSize, getTotalNumOutputChannels());

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

    const juce::ScopedLock lock (rackLock);
    rack = std::move (newRack);
    selectedModule.store (juce::jlimit (0, (int) rack.size() - 1, (int) state.getProperty (selectedId, 0)));
    publishRack();
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
