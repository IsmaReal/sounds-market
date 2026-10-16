#pragma once

#include "Envelope.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

// Grilla de beats donde se dibuja la envolvente de un módulo con nodos.
//
//  - Click en un espacio vacío: agrega un nodo (y se puede arrastrar enseguida).
//  - Arrastrar un nodo: lo mueve. Con Shift se ignora el snap.
//  - Doble click o click derecho sobre un nodo: lo borra.
//  - Arrastrar el punto de curva (en el medio de cada segmento): curva el segmento.
//  - Doble click sobre el punto de curva: vuelve el segmento a línea recta.
class EnvelopeEditor final : public juce::Component
{
public:
    EnvelopeEditor();

    void setNodes (const std::vector<beatshaper::Node>& newNodes);
    const std::vector<beatshaper::Node>& getNodes() const noexcept { return nodes; }

    // Largo del patrón en beats y divisiones de snap por beat (0 = sin snap).
    void setGrid (int patternBeats, int snapPerBeat);

    // Posición de reproducción [0, 1) y si hay que mostrarla.
    void setPlayhead (float phase, float gain, bool visible);

    // Se llama cada vez que el usuario edita la envolvente.
    std::function<void (const std::vector<beatshaper::Node>&)> onChange;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

private:
    enum class TargetType { none, node, curve };

    struct Target
    {
        TargetType type = TargetType::none;
        int index = -1;
    };

    juce::Rectangle<float> getGridArea() const noexcept;
    juce::Point<float> toScreen (double x, double y) const noexcept;
    double screenToX (float px) const noexcept;
    double screenToY (float py) const noexcept;
    juce::Point<float> curveHandlePosition (int segmentIndex) const noexcept;
    double snapX (double x, bool bypass) const noexcept;

    Target findTarget (juce::Point<float> position) const noexcept;
    void commit();
    void refreshEnvelope();
    void drawGrid (juce::Graphics&, juce::Rectangle<float> area);
    void drawEnvelope (juce::Graphics&, juce::Rectangle<float> area);

    beatshaper::Envelope envelope;     // para evaluar la curva al dibujar
    std::vector<beatshaper::Node> nodes; // copia ordenada que se edita

    int patternBeats = 4;
    int snapDivisions = 16; // divisiones en todo el patrón
    Target hovered, dragging;
    bool createdOnThisClick = false;

    float playheadPhase = 0.0f;
    float playheadGain = 1.0f;
    bool playheadVisible = false;
};
