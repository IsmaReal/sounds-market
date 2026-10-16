#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Presets.h"

namespace
{
    const juce::Identifier patternId { "Pattern" };
    const juce::Identifier nodeId { "Node" };
    const juce::Identifier xId { "x" };
    const juce::Identifier yId { "y" };
    const juce::Identifier curveId { "curve" };
    const juce::Identifier snapId { "snap" };
}

BeatShaperProcessor::BeatShaperProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "BeatShaper", createParameterLayout())
{
    mixParam = parameters.getRawParameterValue ("mix");
    smoothParam = parameters.getRawParameterValue ("smooth");

    // Arrancamos con un sidechain clásico para que el efecto se escuche de entrada.
    setEnvelopeNodes (beatshaper::getFactoryPresets()[1].nodes);
}

juce::AudioProcessorValueTreeState::ParameterLayout BeatShaperProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "mix", 1 }, "Mix",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f,
        juce::AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "smooth", 1 }, "Smooth",
        juce::NormalisableRange<float> (0.0f, 50.0f, 0.01f, 0.5f), 2.0f,
        juce::AudioParameterFloatAttributes().withLabel ("ms")));

    return layout;
}

//==============================================================================
void BeatShaperProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    gainBuffer.assign ((size_t) juce::jmax (1, samplesPerBlock), 1.0f);
    internalBeatPosition = 0.0;
    currentGain = 1.0f;
    tableDirty.store (true);
}

bool BeatShaperProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == out;
}

float BeatShaperProcessor::lookupGain (double phase) const noexcept
{
    const auto position = phase * tableSize;
    const auto index = juce::jlimit (0, tableSize - 1, (int) position);
    const auto frac = (float) (position - index);
    return audioTable[(size_t) index] + frac * (audioTable[(size_t) index + 1] - audioTable[(size_t) index]);
}

void BeatShaperProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = getTotalNumInputChannels();

    for (auto ch = numChannels; ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    if (numSamples == 0)
        return;

    // Traemos la última versión de la envolvente si el editor la cambió.
    if (tableDirty.load())
    {
        const juce::SpinLock::ScopedTryLockType lock (tableLock);

        if (lock.isLocked())
        {
            audioTable = sharedTable;
            tableDirty.store (false);
        }
    }

    // Posición en beats: seguimos al host cuando está reproduciendo y,
    // si está detenido, corremos con un reloj interno al tempo del host.
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
    const auto patternBeats = (double) beatshaper::Envelope::patternBeats;
    const auto mix = juce::jlimit (0.0f, 1.0f, mixParam->load() * 0.01f);
    const auto smoothMs = smoothParam->load();
    const auto coefficient = smoothMs <= 0.0f
                                 ? 0.0f
                                 : (float) std::exp (-1.0 / (smoothMs * 0.001 * currentSampleRate));

    if ((int) gainBuffer.size() < numSamples)
        gainBuffer.resize ((size_t) numSamples); // solo si el host excede el tamaño anunciado

    double phase = 0.0;

    for (int i = 0; i < numSamples; ++i)
    {
        auto beatsInPattern = std::fmod (beatPosition, patternBeats);

        if (beatsInPattern < 0.0)
            beatsInPattern += patternBeats;

        phase = beatsInPattern / patternBeats;

        const auto target = 1.0f - mix + mix * lookupGain (phase);
        currentGain = target + coefficient * (currentGain - target);
        gainBuffer[(size_t) i] = currentGain;

        beatPosition += beatsPerSample;
    }

    // Mantenemos el reloj interno en un rango acotado para no perder precisión.
    internalBeatPosition = std::fmod (beatPosition, patternBeats);

    for (int ch = 0; ch < numChannels; ++ch)
        juce::FloatVectorOperations::multiply (buffer.getWritePointer (ch), gainBuffer.data(), numSamples);

    displayPhase.store ((float) phase);
    displayGain.store (currentGain);
}

//==============================================================================
std::vector<beatshaper::Node> BeatShaperProcessor::getEnvelopeNodes() const
{
    const juce::ScopedLock lock (envelopeLock);
    return envelope.getNodes();
}

void BeatShaperProcessor::setEnvelopeNodes (std::vector<beatshaper::Node> nodes)
{
    std::array<float, tableSize + 1> table {};

    {
        const juce::ScopedLock lock (envelopeLock);
        envelope.setNodes (std::move (nodes));
        envelope.renderTable (table.data(), tableSize);
    }

    {
        const juce::SpinLock::ScopedLockType lock (tableLock);
        sharedTable = table;
    }

    tableDirty.store (true);
    ++envelopeVersion;
}

juce::ValueTree BeatShaperProcessor::nodesToTree (const std::vector<beatshaper::Node>& nodes)
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

std::vector<beatshaper::Node> BeatShaperProcessor::treeToNodes (const juce::ValueTree& tree)
{
    std::vector<beatshaper::Node> nodes;

    for (const auto& child : tree)
    {
        if (! child.hasType (nodeId))
            continue;

        nodes.push_back ({ (double) child.getProperty (xId, 0.0),
                           (float) child.getProperty (yId, 1.0f),
                           (float) child.getProperty (curveId, 0.0f) });
    }

    return nodes;
}

void BeatShaperProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.removeChild (state.getChildWithName (patternId), nullptr);
    state.appendChild (nodesToTree (getEnvelopeNodes()), nullptr);
    state.setProperty (snapId, snapDivisions.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void BeatShaperProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (parameters.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    const auto pattern = state.getChildWithName (patternId);

    if (pattern.isValid())
        setEnvelopeNodes (treeToNodes (pattern));

    snapDivisions.store ((int) state.getProperty (snapId, 16));

    state.removeChild (pattern, nullptr);
    parameters.replaceState (state);
}

//==============================================================================
juce::AudioProcessorEditor* BeatShaperProcessor::createEditor()
{
    return new BeatShaperEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BeatShaperProcessor();
}
