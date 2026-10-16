#pragma once

#include "Modules.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>

// Procesador de Flow: un rack de módulos en serie, cada uno con su envolvente
// sincronizada al tempo del host.
class FlowProcessor final : public juce::AudioProcessor
{
public:
    using ModulePtr = std::shared_ptr<flow::Module>;
    using Chain = std::vector<ModulePtr>;

    static constexpr int maxModules = 8;

    FlowProcessor();
    ~FlowProcessor() override;

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
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Rack (hilo de mensajes)
    Chain getRack() const;
    int getRackVersion() const noexcept { return rackVersion.load(); }

    bool addModule (flow::ModuleType type);
    void removeModule (int index);
    void moveModule (int fromIndex, int toIndex);
    void resetRack();

    int getSelectedModule() const noexcept { return selectedModule.load(); }
    void setSelectedModule (int index) noexcept;

    // Guardar / cargar todo el rack (presets generales).
    juce::ValueTree createStateTree() const;
    void restoreStateTree (const juce::ValueTree& state);
    void loadFactoryRackPreset (int index);

    juce::String getRackPresetName() const;
    void setRackPresetName (const juce::String& name);

    // Libera en el hilo de mensajes las cadenas que el audio ya dejó de usar.
    void collectGarbage();

    //==============================================================================
    // Ajustes globales
    static constexpr int gridOptions[] = { 1, 2, 4, 8 };      // largo del patrón en beats
    static constexpr int snapOptions[] = { 0, 1, 2, 4, 8, 3, 6 }; // divisiones por beat (0 = sin snap)
    static constexpr float curveOptionsMs[] = { 0.5f, 3.0f, 15.0f }; // Hard, Smooth, Soft

    int getPatternBeats() const noexcept { return patternBeats.load(); }
    void setPatternBeats (int beats) noexcept { patternBeats.store (juce::jlimit (1, 16, beats)); }

    // Cambia el largo del patrón y regenera los presets de módulo que siguen la grilla.
    void changePatternBeats (int beats);

    int getSnapPerBeat() const noexcept { return snapPerBeat.load(); }
    void setSnapPerBeat (int divisions) noexcept { snapPerBeat.store (juce::jmax (0, divisions)); }

    int getCurveMode() const noexcept { return curveMode.load(); }
    void setCurveMode (int mode) noexcept { curveMode.store (juce::jlimit (0, 2, mode)); }

    // Posición actual en el patrón [0, 1) y si el patrón está avanzando, para la UI.
    float getDisplayPhase() const noexcept { return displayPhase.load(); }
    bool isClockRunning() const noexcept { return clockRunning.load(); }

private:
    void publishRack(); // requiere rackLock tomado
    ModulePtr createPreparedModule (flow::ModuleType type) const;

    // Rack autoritativo (UI / estado).
    mutable juce::CriticalSection rackLock;
    Chain rack;
    juce::String rackPresetName { "Init" };

    // Entrega de la cadena al hilo de audio sin bloquearlo.
    juce::SpinLock chainLock;
    std::unique_ptr<Chain> pendingChain, retiredChain;
    std::unique_ptr<Chain> activeChain; // solo hilo de audio

    std::atomic<int> rackVersion { 0 };
    std::atomic<int> selectedModule { 0 };
    std::atomic<int> patternBeats { 4 };
    std::atomic<int> snapPerBeat { 4 };
    std::atomic<int> curveMode { 1 };

    double currentSampleRate = 44100.0;
    int maxBlockSize = 512;
    double internalBeatPosition = 0.0;
    double lastBpm = 120.0;
    std::vector<double> phaseBuffer;

    std::atomic<float> displayPhase { 0.0f };
    std::atomic<bool> clockRunning { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowProcessor)
};
