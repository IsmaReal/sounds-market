#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

// Modelo puro (sin JUCE) de la envolvente de volumen.
// El eje X va de 0 a 1 y cubre el patrón completo (4 beats).
// El eje Y es la ganancia, de 0 (silencio) a 1 (volumen original).
// Cada nodo guarda la curvatura del segmento que sale de él hacia el siguiente.
namespace beatshaper
{

struct Node
{
    double x = 0.0;      // posición en el patrón [0, 1]
    float y = 1.0f;      // ganancia [0, 1]
    float curve = 0.0f;  // curvatura del segmento hacia el próximo nodo [-1, 1]
};

class Envelope
{
public:
    static constexpr int patternBeats = 4;
    static constexpr float maxCurveExponent = 16.0f;

    Envelope() { setFlat (1.0f); }

    const std::vector<Node>& getNodes() const noexcept { return nodes; }

    void setNodes (std::vector<Node> newNodes)
    {
        nodes = std::move (newNodes);
        sanitise();
    }

    void setFlat (float level)
    {
        nodes = { { 0.0, level, 0.0f }, { 1.0, level, 0.0f } };
    }

    // Garantiza las invariantes: al menos dos nodos, ordenados por X,
    // el primero en X = 0 y el último en X = 1, valores dentro de rango.
    void sanitise()
    {
        for (auto& n : nodes)
        {
            n.x = std::clamp (n.x, 0.0, 1.0);
            n.y = std::clamp (n.y, 0.0f, 1.0f);
            n.curve = std::clamp (n.curve, -1.0f, 1.0f);
        }

        std::stable_sort (nodes.begin(), nodes.end(),
                          [] (const Node& a, const Node& b) { return a.x < b.x; });

        if (nodes.empty())
        {
            setFlat (1.0f);
            return;
        }

        if (nodes.size() == 1)
            nodes.push_back (nodes.front());

        nodes.front().x = 0.0;
        nodes.back().x = 1.0;
    }

    // Exponente aplicado al tiempo normalizado del segmento.
    static float curveToExponent (float curve) noexcept
    {
        return std::pow (maxCurveExponent, curve);
    }

    // Forma del segmento: t en [0, 1] -> [0, 1].
    static double shape (double t, float curve) noexcept
    {
        if (std::abs (curve) < 1.0e-4f)
            return t;

        return std::pow (t, (double) curveToExponent (curve));
    }

    // Inversa usada por el editor: dada la fracción deseada en el punto medio
    // del segmento, devuelve la curvatura que la produce.
    static float curveForMidpointFraction (double fraction) noexcept
    {
        fraction = std::clamp (fraction, 0.005, 0.995);
        const auto exponent = std::log (fraction) / std::log (0.5);
        return std::clamp ((float) (std::log (exponent) / std::log ((double) maxCurveExponent)), -1.0f, 1.0f);
    }

    static double valueInSegment (const Node& a, const Node& b, double x) noexcept
    {
        const auto width = b.x - a.x;

        if (width <= 0.0)
            return b.y;

        const auto t = std::clamp ((x - a.x) / width, 0.0, 1.0);
        return std::clamp (a.y + (b.y - a.y) * shape (t, a.curve), 0.0, 1.0);
    }

    // Índice del primer nodo del segmento que contiene x.
    // Si varios nodos comparten la misma X (salto vertical), gana el último.
    size_t segmentIndexFor (double x) const noexcept
    {
        auto it = std::upper_bound (nodes.begin(), nodes.end(), x,
                                    [] (double value, const Node& n) { return value < n.x; });

        auto index = (size_t) std::distance (nodes.begin(), it);
        index = index == 0 ? 0 : index - 1;
        return std::min (index, nodes.size() - 2);
    }

    double evaluate (double x) const noexcept
    {
        x = std::clamp (x, 0.0, 1.0);
        const auto i = segmentIndexFor (x);
        return valueInSegment (nodes[i], nodes[i + 1], x);
    }

    // Llena una tabla de numPoints + 1 valores (el último corresponde a X = 1)
    // para que el hilo de audio pueda leerla con interpolación lineal.
    void renderTable (float* table, int numPoints) const noexcept
    {
        for (int i = 0; i <= numPoints; ++i)
            table[i] = (float) evaluate ((double) i / (double) numPoints);
    }

private:
    std::vector<Node> nodes;
};

} // namespace beatshaper
