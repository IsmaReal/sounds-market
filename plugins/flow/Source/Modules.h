#pragma once

#include "EnvelopeState.h"
#include "ModuleType.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <memory>

namespace flow
{

// Parámetro propio de un módulo (se muestra en "Module Controls").
struct ModuleParameterSpec
{
    const char* id;
    const char* name;
    float defaultValue; // [0, 1]
};

// Un módulo del rack: tiene encendido, mix, una envolvente dibujable con nodos
// y parámetros propios. Los valores son atómicos para poder tocarlos desde la UI
// mientras el hilo de audio procesa.
class Module
{
public:
    static constexpr int maxParameters = 4;

    virtual ~Module() = default;

    virtual ModuleType getType() const noexcept = 0;
    virtual const char* getName() const noexcept = 0;
    virtual std::vector<ModuleParameterSpec> getParameterSpecs() const = 0;
    virtual const char* getDefaultPresetName() const noexcept = 0;

    // Se llama en el hilo de mensajes (o en prepareToPlay), nunca en paralelo con process().
    void prepare (double sampleRate, int maxBlockSize, int numChannels);

    // Procesa en el lugar. `phases` tiene la posición en el patrón [0, 1) de cada muestra.
    void process (juce::AudioBuffer<float>& buffer, const double* phases, int numSamples, float smoothing) noexcept;

    std::atomic<bool> enabled { true };
    std::atomic<float> mix { 1.0f };
    EnvelopeState envelope;

    // Nombre del preset de módulo cargado (vacío si se editó a mano). Solo hilo de mensajes.
    juce::String presetName;

    // Aplica un preset de módulo (patrón + parámetros). Solo hilo de mensajes.
    bool applyPreset (const juce::String& name, int patternBeats);

    // Si el preset cargado sigue la grilla (Gate 1/8, etc.), regenera el patrón para
    // el nuevo largo sin tocar los parámetros. Solo hilo de mensajes.
    void updatePatternForGrid (int patternBeats);

    float getParameter (int index) const noexcept { return parameters[(size_t) index].load(); }
    void setParameter (int index, float value) noexcept { parameters[(size_t) index].store (juce::jlimit (0.0f, 1.0f, value)); }

    // Último valor de la envolvente aplicado (para la UI).
    float getCurrentEnvelopeValue() const noexcept { return currentEnvelopeValue.load(); }

protected:
    void initialise(); // parámetros por defecto y preset inicial

    virtual void prepareEffect (double sampleRate, int maxBlockSize, int numChannels) = 0;
    virtual void processEffect (juce::AudioBuffer<float>& buffer, const float* envelopeValues, int numSamples) noexcept = 0;

    std::array<std::atomic<float>, maxParameters> parameters {};
    double sampleRate = 44100.0;

private:
    std::vector<float> envelopeBuffer;
    float smoothedValue = -1.0f;
    std::atomic<float> currentEnvelopeValue { 0.0f };
};

//==============================================================================
// Volumen: la envolvente es la ganancia de la señal.
class VolumeModule final : public Module
{
public:
    VolumeModule();

    ModuleType getType() const noexcept override { return ModuleType::volume; }
    const char* getName() const noexcept override { return "Volume"; }
    const char* getDefaultPresetName() const noexcept override { return "Sidechain 1/4"; }
    std::vector<ModuleParameterSpec> getParameterSpecs() const override { return {}; }

private:
    void prepareEffect (double, int, int) override {}
    void processEffect (juce::AudioBuffer<float>&, const float*, int) noexcept override;
};

//==============================================================================
// Reverb: la envolvente dibuja cuánta reverb se escucha en cada momento del patrón.
class ReverbModule final : public Module
{
public:
    enum Parameter { size, decay, tone };

    ReverbModule();

    ModuleType getType() const noexcept override { return ModuleType::reverb; }
    const char* getName() const noexcept override { return "Reverb"; }
    const char* getDefaultPresetName() const noexcept override { return "Swells"; }
    std::vector<ModuleParameterSpec> getParameterSpecs() const override;

private:
    void prepareEffect (double sampleRate, int maxBlockSize, int numChannels) override;
    void processEffect (juce::AudioBuffer<float>&, const float*, int) noexcept override;
    void updateReverbParameters() noexcept;

    juce::Reverb reverb;
    juce::AudioBuffer<float> wetBuffer;
    std::array<float, 2> toneState {};
    std::array<float, 3> lastParameters { -1.0f, -1.0f, -1.0f };
    float toneCoefficient = 0.0f;
};

//==============================================================================
std::shared_ptr<Module> createModule (ModuleType type);
const char* moduleTypeToId (ModuleType type) noexcept;
bool moduleTypeFromId (const juce::String& id, ModuleType& type) noexcept;
std::vector<ModuleType> getAvailableModuleTypes();
const char* getModuleTypeName (ModuleType type) noexcept;
const char* getModuleTypeDescription (ModuleType type) noexcept;

} // namespace flow
