#pragma once

#include "Envelope.h"
#include "ModuleType.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

// Presets de módulo: un patrón para la envolvente y, opcionalmente, valores para los
// parámetros propios del módulo (por ejemplo, Size/Decay/Tone de la reverb).
//
// Orden de la lista: primero los básicos (iguales para todos los módulos) y después
// los creativos. Los patrones se generan según el largo del patrón en beats, así
// "Gate 1/8" son siempre corcheas, sea cual sea el Grid.
namespace flow
{

using beatshaper::Node;

struct ModulePreset
{
    std::string name;
    std::string category;
    std::function<std::vector<Node> (int patternBeats)> pattern;
    std::vector<std::pair<int, float>> parameters; // índice del parámetro -> valor [0, 1]
    bool followsGrid = true; // se regenera si cambia el largo del patrón
};

namespace presets
{
    // Figuras musicales, en beats (negras).
    struct Division
    {
        const char* name;
        double beats;
    };

    inline const std::vector<Division>& gateDivisions()
    {
        static const std::vector<Division> d {
            { "1/1", 4.0 }, { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 }, { "1/32", 0.125 },
            { "1/2T", 4.0 / 3.0 }, { "1/4T", 2.0 / 3.0 }, { "1/8T", 1.0 / 3.0 }, { "1/16T", 1.0 / 6.0 },
            { "1/4D", 1.5 }, { "1/8D", 0.75 },
        };
        return d;
    }

    inline const std::vector<Division>& sidechainDivisions()
    {
        static const std::vector<Division> d {
            { "1/1", 4.0 }, { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 },
            { "1/4T", 2.0 / 3.0 }, { "1/8T", 1.0 / 3.0 }, { "1/4D", 1.5 }, { "1/8D", 0.75 },
        };
        return d;
    }

    inline const std::vector<Division>& rampDivisions()
    {
        static const std::vector<Division> d {
            { "1/1", 4.0 }, { "1/2", 2.0 }, { "1/4", 1.0 }, { "1/8", 0.5 }, { "1/16", 0.25 },
        };
        return d;
    }

    // Recorre el patrón en pasos de `stepBeats` y llama a addStep (inicio, fin) en [0, 1].
    template <typename AddStep>
    std::vector<Node> steps (int patternBeats, double stepBeats, AddStep addStep)
    {
        std::vector<Node> nodes;
        const auto total = (double) patternBeats;

        for (int i = 0; i * stepBeats < total - 1.0e-9; ++i)
        {
            const auto start = i * stepBeats;
            const auto end = std::min (start + stepBeats, total);
            addStep (nodes, start / total, end / total, std::min (start + stepBeats * 0.5, total) / total);
        }

        return nodes;
    }

    inline std::vector<Node> gate (int patternBeats, double stepBeats)
    {
        return steps (patternBeats, stepBeats, [] (auto& n, double s, double e, double half) {
            n.push_back ({ s, 1.0f, 0.0f });
            n.push_back ({ half, 1.0f, 0.0f });
            n.push_back ({ half, 0.0f, 0.0f });
            n.push_back ({ e, 0.0f, 0.0f });
        });
    }

    inline std::vector<Node> sidechain (int patternBeats, double stepBeats)
    {
        return steps (patternBeats, stepBeats, [] (auto& n, double s, double e, double) {
            n.push_back ({ s, 0.0f, -0.35f });
            n.push_back ({ e, 1.0f, 0.0f });
        });
    }

    inline std::vector<Node> ramp (int patternBeats, double stepBeats, bool up)
    {
        return steps (patternBeats, stepBeats, [up] (auto& n, double s, double e, double) {
            n.push_back ({ s, up ? 0.0f : 1.0f, 0.0f });
            n.push_back ({ e, up ? 1.0f : 0.0f, 0.0f });
        });
    }

    // Crece con una curva hasta un pico cerca del final de cada paso y cae de golpe.
    inline std::vector<Node> swells (int patternBeats, double stepBeats, double peakAt, float peak)
    {
        auto n = steps (patternBeats, stepBeats, [peakAt, peak] (auto& nodes, double s, double e, double) {
            nodes.push_back ({ s, 0.0f, 0.5f });
            nodes.push_back ({ s + (e - s) * peakAt, peak, 0.0f });
        });
        n.push_back ({ 1.0, 0.0f, 0.0f });
        return n;
    }

    // Preset cuyo patrón se genera según el largo del patrón.
    inline ModulePreset generated (std::string name, std::string category,
                                   std::function<std::vector<Node> (int)> pattern)
    {
        return { std::move (name), std::move (category), std::move (pattern), {}, true };
    }

    inline ModulePreset fixed (std::string name, std::string category, std::vector<Node> nodes)
    {
        return { std::move (name), std::move (category),
                 [nodes] (int) { return nodes; }, {}, false };
    }

    // Básicos: iguales para todos los módulos.
    inline std::vector<ModulePreset> basic()
    {
        std::vector<ModulePreset> p;
        p.push_back (fixed ("Init", "Basic", { { 0.0, 1.0f, 0.0f }, { 1.0, 1.0f, 0.0f } }));

        for (const auto& d : gateDivisions())
            p.push_back (generated (std::string ("Gate ") + d.name, "Gate",
                                     [b = d.beats] (int beats) { return gate (beats, b); }));

        for (const auto& d : sidechainDivisions())
            p.push_back (generated (std::string ("Sidechain ") + d.name, "Sidechain",
                                     [b = d.beats] (int beats) { return sidechain (beats, b); }));

        for (const auto& d : rampDivisions())
            p.push_back (generated (std::string ("Ramp Up ") + d.name, "Ramp",
                                     [b = d.beats] (int beats) { return ramp (beats, b, true); }));

        for (const auto& d : rampDivisions())
            p.push_back (generated (std::string ("Ramp Down ") + d.name, "Ramp",
                                     [b = d.beats] (int beats) { return ramp (beats, b, false); }));

        return p;
    }

    // Creativos para todos los módulos.
    inline std::vector<ModulePreset> creative()
    {
        std::vector<ModulePreset> p;
        p.push_back (generated ("Swells", "Creative", [] (int beats) { return swells (beats, 1.0, 0.85, 0.9f); }));
        p.push_back (generated ("Swells 1/8", "Creative", [] (int beats) { return swells (beats, 0.5, 0.8, 0.85f); }));
        p.push_back (fixed ("Trance Gate", "Creative",
                            { { 0.0, 1.0f, 0.0f }, { 0.125, 1.0f, 0.0f }, { 0.125, 0.0f, 0.0f },
                              { 0.1875, 0.0f, 0.0f }, { 0.1875, 1.0f, 0.0f }, { 0.25, 1.0f, 0.0f },
                              { 0.25, 0.0f, 0.0f }, { 0.375, 0.0f, 0.0f }, { 0.375, 1.0f, 0.0f },
                              { 0.4375, 1.0f, 0.0f }, { 0.4375, 0.0f, 0.0f }, { 0.5, 0.0f, 0.0f },
                              { 0.5, 1.0f, 0.0f }, { 0.625, 1.0f, 0.0f }, { 0.625, 0.0f, 0.0f },
                              { 0.6875, 0.0f, 0.0f }, { 0.6875, 1.0f, 0.0f }, { 0.8125, 1.0f, 0.0f },
                              { 0.8125, 0.0f, 0.0f }, { 0.875, 0.0f, 0.0f }, { 0.875, 1.0f, 0.0f },
                              { 0.9375, 1.0f, 0.0f }, { 0.9375, 0.0f, 0.0f }, { 1.0, 0.0f, 0.0f } }));
        p.push_back (fixed ("Fade In", "Creative", { { 0.0, 0.0f, 0.5f }, { 1.0, 1.0f, 0.0f } }));
        p.push_back (fixed ("Fade Out", "Creative", { { 0.0, 1.0f, 0.5f }, { 1.0, 0.0f, 0.0f } }));
        p.push_back (fixed ("Last Beat Cut", "Creative", { { 0.0, 1.0f, 0.0f }, { 0.75, 1.0f, 0.0f },
                                                           { 0.75, 0.0f, 0.0f }, { 1.0, 0.0f, 0.0f } }));
        p.push_back (fixed ("Last Beat Only", "Creative", { { 0.0, 0.0f, 0.0f }, { 0.75, 0.0f, 0.0f },
                                                            { 0.75, 1.0f, 0.0f }, { 1.0, 1.0f, 0.0f } }));
        return p;
    }

    // Creativos propios de cada tipo de módulo (pueden fijar sus parámetros).
    // Ejemplo: { "Big Hall Swell", "Reverb", patrón, { { 0, 0.9f }, { 1, 0.85f } } }
    inline std::vector<ModulePreset> forType (ModuleType type)
    {
        switch (type)
        {
            case ModuleType::volume: return {};
            case ModuleType::reverb: return {};
        }

        return {};
    }
} // namespace presets

inline const std::vector<ModulePreset>& getModulePresets (ModuleType type)
{
    static const auto build = [] (ModuleType t) {
        auto list = presets::basic();
        for (auto& p : presets::creative())
            list.push_back (std::move (p));
        for (auto& p : presets::forType (t))
            list.push_back (std::move (p));
        return list;
    };

    static const std::vector<ModulePreset> volume = build (ModuleType::volume);
    static const std::vector<ModulePreset> reverb = build (ModuleType::reverb);
    return type == ModuleType::reverb ? reverb : volume;
}

// Índice del preset con ese nombre, o -1.
inline int findModulePreset (ModuleType type, const std::string& name)
{
    const auto& list = getModulePresets (type);

    for (size_t i = 0; i < list.size(); ++i)
        if (list[i].name == name)
            return (int) i;

    return -1;
}

} // namespace flow
