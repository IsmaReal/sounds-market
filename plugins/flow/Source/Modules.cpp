#include "Modules.h"
#include "Presets.h"

namespace flow
{

//==============================================================================
void Module::initialiseParameters()
{
    const auto specs = getParameterSpecs();

    for (size_t i = 0; i < specs.size() && i < (size_t) maxParameters; ++i)
        parameters[i].store (specs[i].defaultValue);
}

void Module::prepare (double newSampleRate, int maxBlockSize, int numChannels)
{
    sampleRate = newSampleRate;
    envelopeBuffer.assign ((size_t) juce::jmax (1, maxBlockSize), 0.0f);
    smoothedValue = -1.0f;
    prepareEffect (newSampleRate, juce::jmax (1, maxBlockSize), juce::jmax (1, numChannels));
}

void Module::process (juce::AudioBuffer<float>& buffer, const double* phases, int numSamples, float smoothing) noexcept
{
    envelope.pullTable();

    if (! enabled.load() || numSamples <= 0 || numSamples > (int) envelopeBuffer.size())
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const auto target = envelope.lookup (phases[i]);

        if (smoothedValue < 0.0f)
            smoothedValue = target;

        smoothedValue = target + smoothing * (smoothedValue - target);
        envelopeBuffer[(size_t) i] = smoothedValue;
    }

    currentEnvelopeValue.store (smoothedValue);
    processEffect (buffer, envelopeBuffer.data(), numSamples);
}

//==============================================================================
VolumeModule::VolumeModule()
{
    initialiseParameters();
    envelope.setNodes (getDefaultPattern());
    presetName = "Sidechain Pulse";
}

std::vector<beatshaper::Node> VolumeModule::getDefaultPattern() const
{
    return getFactoryPresets()[(size_t) findPreset ("Sidechain Pulse")].nodes;
}

void VolumeModule::processEffect (juce::AudioBuffer<float>& buffer, const float* envelopeValues, int numSamples) noexcept
{
    const auto amount = mix.load();
    const auto numChannels = buffer.getNumChannels();

    for (int i = 0; i < numSamples; ++i)
    {
        const auto gain = 1.0f - amount + amount * envelopeValues[i];

        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] *= gain;
    }
}

//==============================================================================
ReverbModule::ReverbModule()
{
    initialiseParameters();
    envelope.setNodes (getDefaultPattern());
    presetName = "Swells";
}

std::vector<ModuleParameterSpec> ReverbModule::getParameterSpecs() const
{
    return { { "size", "Size", 0.5f },
             { "decay", "Decay", 0.7f },
             { "tone", "Tone", 0.3f } };
}

std::vector<beatshaper::Node> ReverbModule::getDefaultPattern() const
{
    return getFactoryPresets()[(size_t) findPreset ("Swells")].nodes;
}

void ReverbModule::prepareEffect (double newSampleRate, int maxBlockSize, int numChannels)
{
    reverb.setSampleRate (newSampleRate);
    reverb.reset();
    wetBuffer.setSize (juce::jmin (2, numChannels), maxBlockSize);
    toneState = {};
    lastParameters = { -1.0f, -1.0f, -1.0f };
    updateReverbParameters();
}

void ReverbModule::updateReverbParameters() noexcept
{
    const std::array<float, 3> current { getParameter (size), getParameter (decay), getParameter (tone) };

    if (current == lastParameters)
        return;

    lastParameters = current;

    juce::Reverb::Parameters p;
    p.roomSize = juce::jmap (0.35f * current[size] + 0.65f * current[decay], 0.15f, 0.97f);
    p.damping = juce::jmap (current[decay], 0.7f, 0.15f);
    p.width = juce::jmap (current[size], 0.4f, 1.0f);
    p.wetLevel = 0.4f;
    p.dryLevel = 0.0f;
    p.freezeMode = 0.0f;
    reverb.setParameters (p);

    // Tone: filtro pasa-bajos sobre la señal húmeda, de 800 Hz a 16 kHz.
    const auto cutoff = 800.0 * std::pow (20.0, (double) current[tone]);
    toneCoefficient = (float) std::exp (-juce::MathConstants<double>::twoPi * cutoff / sampleRate);
}

void ReverbModule::processEffect (juce::AudioBuffer<float>& buffer, const float* envelopeValues, int numSamples) noexcept
{
    updateReverbParameters();

    const auto numChannels = juce::jmin (buffer.getNumChannels(), wetBuffer.getNumChannels());

    if (numChannels == 0 || numSamples > wetBuffer.getNumSamples())
        return;

    for (int ch = 0; ch < numChannels; ++ch)
        wetBuffer.copyFrom (ch, 0, buffer, ch, 0, numSamples);

    if (numChannels == 2)
        reverb.processStereo (wetBuffer.getWritePointer (0), wetBuffer.getWritePointer (1), numSamples);
    else
        reverb.processMono (wetBuffer.getWritePointer (0), numSamples);

    const auto amount = mix.load();

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* wet = wetBuffer.getWritePointer (ch);
        auto* out = buffer.getWritePointer (ch);
        auto state = toneState[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            state = wet[i] + toneCoefficient * (state - wet[i]);
            out[i] += state * envelopeValues[i] * amount;
        }

        toneState[(size_t) ch] = state;
    }
}

//==============================================================================
std::shared_ptr<Module> createModule (ModuleType type)
{
    switch (type)
    {
        case ModuleType::volume: return std::make_shared<VolumeModule>();
        case ModuleType::reverb: return std::make_shared<ReverbModule>();
    }

    return nullptr;
}

const char* moduleTypeToId (ModuleType type) noexcept
{
    switch (type)
    {
        case ModuleType::volume: return "volume";
        case ModuleType::reverb: return "reverb";
    }

    return "";
}

bool moduleTypeFromId (const juce::String& id, ModuleType& type) noexcept
{
    for (auto t : getAvailableModuleTypes())
    {
        if (id == moduleTypeToId (t))
        {
            type = t;
            return true;
        }
    }

    return false;
}

std::vector<ModuleType> getAvailableModuleTypes()
{
    return { ModuleType::volume, ModuleType::reverb };
}

const char* getModuleTypeName (ModuleType type) noexcept
{
    switch (type)
    {
        case ModuleType::volume: return "Volume";
        case ModuleType::reverb: return "Reverb";
    }

    return "";
}

} // namespace flow
