// Prueba del procesador: simula un host que reproduce a 120 BPM y verifica
// que la ganancia aplicada siga la envolvente sincronizada al tempo.
#include "PluginProcessor.h"

#include <cstdio>

namespace
{
    struct FakePlayHead final : juce::AudioPlayHead
    {
        double ppq = 0.0;
        double bpm = 120.0;
        bool playing = true;

        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo info;
            info.setBpm (bpm);
            info.setPpqPosition (ppq);
            info.setIsPlaying (playing);
            return info;
        }
    };

    int failures = 0;

    void check (bool condition, const char* what)
    {
        if (! condition)
        {
            std::printf ("FALLO: %s\n", what);
            ++failures;
        }
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 480; // 10 ms

    BeatShaperProcessor processor;
    FakePlayHead playHead;
    processor.setPlayHead (&playHead);
    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    // Gate: abierto los dos primeros beats, cerrado los dos últimos. Sin suavizado.
    processor.setEnvelopeNodes ({ { 0.0, 1.0f, 0.0f }, { 0.5, 1.0f, 0.0f },
                                  { 0.5, 0.0f, 0.0f }, { 1.0, 0.0f, 0.0f } });
    processor.parameters.getParameter ("smooth")->setValueNotifyingHost (0.0f);

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    auto runBlockAt = [&] (double ppq) {
        playHead.ppq = ppq;
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), 1.0f, blockSize);
        processor.processBlock (buffer, midi);
        return buffer.getSample (0, blockSize / 2);
    };

    check (std::abs (runBlockAt (0.5) - 1.0f) < 1.0e-4f, "beat 1: abierto");
    check (std::abs (runBlockAt (2.5) - 0.0f) < 1.0e-4f, "beat 3: cerrado");
    check (std::abs (runBlockAt (4.5) - 1.0f) < 1.0e-4f, "el patrón se repite cada 4 beats");
    check (std::abs (runBlockAt (7.5) - 0.0f) < 1.0e-4f, "beat 4 del segundo compás: cerrado");
    check (std::abs (buffer.getSample (1, 10) - buffer.getSample (0, 10)) < 1.0e-6f, "mismo gain en ambos canales");

    // Mix al 50 %: la parte cerrada queda a la mitad.
    processor.parameters.getParameter ("mix")->setValueNotifyingHost (0.5f);
    runBlockAt (2.5); // deja que el parámetro se aplique
    check (std::abs (runBlockAt (2.5) - 0.5f) < 1.0e-3f, "mix 50 %");

    // Guardar y recuperar el estado.
    juce::MemoryBlock state;
    processor.getStateInformation (state);

    BeatShaperProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    const auto nodes = restored.getEnvelopeNodes();
    check (nodes.size() == 4, "estado: cantidad de nodos");
    check (nodes.size() == 4 && nodes[2].y == 0.0f && nodes[2].x == 0.5, "estado: posición de nodos");
    check (std::abs (restored.parameters.getRawParameterValue ("mix")->load() - 50.0f) < 0.1f, "estado: mix");

    processor.releaseResources();

    if (failures == 0)
        std::printf ("Todos los tests del procesador pasaron.\n");

    return failures == 0 ? 0 : 1;
}
