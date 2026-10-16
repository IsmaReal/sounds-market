// Pruebas del procesador: simula un host que reproduce a 120 BPM y verifica
// el rack de módulos, el volumen sincronizado, la reverb y el guardado del estado.
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

    bool near (float a, float b, float tolerance = 1.0e-3f) { return std::abs (a - b) <= tolerance; }

    const std::vector<beatshaper::Node> halfGate { { 0.0, 1.0f, 0.0f }, { 0.5, 1.0f, 0.0f },
                                                   { 0.5, 0.0f, 0.0f }, { 1.0, 0.0f, 0.0f } };
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 480; // 10 ms

    FlowProcessor processor;
    FakePlayHead playHead;
    processor.setPlayHead (&playHead);
    processor.setPlayConfigDetails (2, 2, sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);
    processor.setCurveMode (0); // Hard: transiciones casi instantáneas

    juce::AudioBuffer<float> buffer (2, blockSize);
    juce::MidiBuffer midi;

    // Bloque con señal constante en 1.0; devuelve una muestra del medio.
    auto runConstantAt = [&] (double ppq) {
        playHead.ppq = ppq;
        for (int ch = 0; ch < 2; ++ch)
            juce::FloatVectorOperations::fill (buffer.getWritePointer (ch), 1.0f, blockSize);
        processor.processBlock (buffer, midi);
        return buffer.getSample (0, blockSize / 2);
    };

    //==============================================================================
    // Rack por defecto: un único módulo de volumen.
    auto rack = processor.getRack();
    check (rack.size() == 1 && rack[0]->getType() == flow::ModuleType::volume, "rack por defecto con Volume");

    rack[0]->envelope.setNodes (halfGate);

    runConstantAt (0.0); // toma la cadena y la tabla nueva
    check (near (runConstantAt (0.5), 1.0f), "volume: beat 1 abierto");
    check (near (runConstantAt (2.5), 0.0f), "volume: beat 3 cerrado");
    check (near (runConstantAt (4.5), 1.0f), "volume: el patrón se repite cada 4 beats");

    rack[0]->mix.store (0.5f);
    check (near (runConstantAt (2.5), 0.5f), "volume: mix 50 %");

    rack[0]->enabled.store (false);
    check (near (runConstantAt (2.5), 1.0f), "volume apagado no procesa");
    rack[0]->enabled.store (true);
    rack[0]->mix.store (1.0f);

    // Largo de patrón de 2 beats: el beat 3 vuelve a ser el comienzo.
    processor.setPatternBeats (2);
    check (near (runConstantAt (2.5), 1.0f), "grid de 2 beats");
    processor.setPatternBeats (4);

    //==============================================================================
    // Reverb: agrega cola donde la envolvente está alta.
    check (processor.addModule (flow::ModuleType::reverb), "agregar reverb");
    rack = processor.getRack();
    check (rack.size() == 2 && rack[1]->getType() == flow::ModuleType::reverb, "reverb al final del rack");
    check (processor.getSelectedModule() == 1, "el módulo nuevo queda seleccionado");

    rack[0]->envelope.setNodes ({ { 0.0, 1.0f, 0.0f }, { 1.0, 1.0f, 0.0f } }); // volumen plano
    rack[1]->envelope.setNodes (halfGate);

    auto runImpulseAt = [&] (double ppq, bool impulse) {
        playHead.ppq = ppq;
        buffer.clear();
        if (impulse)
            for (int ch = 0; ch < 2; ++ch)
                buffer.setSample (ch, 0, 1.0f);
        processor.processBlock (buffer, midi);
        return buffer.getMagnitude (0, blockSize);
    };

    runImpulseAt (0.0, false);
    runImpulseAt (0.02, true);
    auto tail = 0.0f;
    for (int i = 1; i < 20; ++i)
        tail = juce::jmax (tail, runImpulseAt (0.02 + i * 0.02, false));
    check (tail > 1.0e-3f, "reverb: hay cola cuando la envolvente está arriba");

    // Con la envolvente en cero (beats 3-4) la reverb no se escucha.
    for (int i = 0; i < 400; ++i)
        runImpulseAt (2.0 + i * 0.002, false); // vaciamos la cola
    runImpulseAt (2.1, true);
    auto silentTail = 0.0f;
    for (int i = 1; i < 20; ++i)
        silentTail = juce::jmax (silentTail, runImpulseAt (2.1 + i * 0.02, false));
    check (silentTail < 1.0e-4f, "reverb: silencio cuando la envolvente está en cero");

    //==============================================================================
    // Reordenar, guardar y recuperar el estado.
    rack[1]->setParameter (flow::ReverbModule::size, 0.25f);
    rack[1]->mix.store (0.6f);
    processor.moveModule (1, 0);
    rack = processor.getRack();
    check (rack[0]->getType() == flow::ModuleType::reverb, "mover reverb arriba");
    check (processor.getSelectedModule() == 0, "la selección sigue al módulo movido");

    processor.setSnapPerBeat (2);

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    FlowProcessor restored;
    restored.setStateInformation (state.getData(), (int) state.getSize());
    const auto restoredRack = restored.getRack();
    check (restoredRack.size() == 2, "estado: cantidad de módulos");
    check (restoredRack.size() == 2 && restoredRack[0]->getType() == flow::ModuleType::reverb
               && restoredRack[1]->getType() == flow::ModuleType::volume, "estado: orden del rack");
    check (! restoredRack.empty() && near (restoredRack[0]->getParameter (flow::ReverbModule::size), 0.25f), "estado: parámetro de reverb");
    check (! restoredRack.empty() && near (restoredRack[0]->mix.load(), 0.6f), "estado: mix");
    check (! restoredRack.empty() && restoredRack[0]->envelope.getNodes().size() == 4, "estado: nodos");
    check (restored.getSnapPerBeat() == 2, "estado: snap");

    // Quitar módulos.
    processor.removeModule (0);
    check (processor.getRack().size() == 1, "quitar módulo");
    processor.removeModule (0);
    check (processor.getRack().empty(), "rack vacío permitido");
    check (near (runConstantAt (1.0), 1.0f), "rack vacío deja pasar la señal");

    for (int i = 0; i < FlowProcessor::maxModules; ++i)
        processor.addModule (flow::ModuleType::volume);
    check (! processor.addModule (flow::ModuleType::reverb), "límite de módulos");

    processor.collectGarbage();
    processor.releaseResources();

    if (failures == 0)
        std::printf ("Todos los tests del procesador pasaron.\n");

    return failures == 0 ? 0 : 1;
}
