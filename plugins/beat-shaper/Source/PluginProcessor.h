#pragma once

#include "Envelope.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>

class BeatShaperProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int tableSize = 4096;

    BeatShaperProcessor();
    ~BeatShaperProcessor() override = default;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Acceso a la envolvente desde el hilo de mensajes (editor / estado).
    std::vector<beatshaper::Node> getEnvelopeNodes() const;
    void setEnvelopeNodes (std::vector<beatshaper::Node> nodes);

    // Se incrementa cada vez que cambia la envolvente, para que el editor se refresque.
    int getEnvelopeVersion() const noexcept { return envelopeVersion.load(); }

    // Posición actual dentro del patrón [0, 1) y ganancia aplicada, para la UI.
    float getDisplayPhase() const noexcept { return displayPhase.load(); }
    float getDisplayGain() const noexcept { return displayGain.load(); }
    bool isHostPlaying() const noexcept { return hostPlaying.load(); }

    // Ajustes de la UI que se guardan junto con el proyecto.
    int getSnapDivisions() const noexcept { return snapDivisions.load(); }
    void setSnapDivisions (int divisions) noexcept { snapDivisions.store (divisions); }

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    static juce::ValueTree nodesToTree (const std::vector<beatshaper::Node>& nodes);
    static std::vector<beatshaper::Node> treeToNodes (const juce::ValueTree& tree);

    float lookupGain (double phase) const noexcept;

    // Envolvente (solo hilo de mensajes, protegida por envelopeLock).
    mutable juce::CriticalSection envelopeLock;
    beatshaper::Envelope envelope;

    // Tabla compartida: la escribe el hilo de mensajes y la copia el de audio.
    juce::SpinLock tableLock;
    std::array<float, tableSize + 1> sharedTable {};
    std::atomic<bool> tableDirty { true };

    // Copia privada del hilo de audio.
    std::array<float, tableSize + 1> audioTable {};

    std::atomic<float>* mixParam = nullptr;
    std::atomic<float>* smoothParam = nullptr;

    double currentSampleRate = 44100.0;
    double internalBeatPosition = 0.0;
    double lastBpm = 120.0;
    float currentGain = 1.0f;
    std::vector<float> gainBuffer;

    std::atomic<float> displayPhase { 0.0f };
    std::atomic<float> displayGain { 1.0f };
    std::atomic<bool> hostPlaying { false };
    std::atomic<int> envelopeVersion { 0 };
    std::atomic<int> snapDivisions { 16 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BeatShaperProcessor)
};
