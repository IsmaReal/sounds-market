#pragma once

#include "Envelope.h"

#include <string>
#include <vector>

namespace beatshaper
{

struct Preset
{
    std::string name;
    std::vector<Node> nodes;
};

namespace detail
{
    // Gate cuadrado: `steps` pasos por patrón, abierto durante `openFraction` de cada paso.
    inline std::vector<Node> gate (int steps, double openFraction, float low = 0.0f)
    {
        std::vector<Node> n;
        const auto step = 1.0 / steps;

        for (int i = 0; i < steps; ++i)
        {
            const auto start = i * step;
            const auto close = start + step * openFraction;
            n.push_back ({ start, 1.0f, 0.0f });
            n.push_back ({ close, 1.0f, 0.0f });
            n.push_back ({ close, low, 0.0f });
            n.push_back ({ start + step, low, 0.0f });
        }

        return n;
    }

    // Efecto "sidechain": cae a `depth` al inicio de cada paso y vuelve con una curva.
    inline std::vector<Node> pump (int steps, float depth)
    {
        std::vector<Node> n;
        const auto step = 1.0 / steps;

        for (int i = 0; i < steps; ++i)
        {
            const auto start = i * step;
            n.push_back ({ start, depth, -0.35f });
            n.push_back ({ start + step, 1.0f, 0.0f });
        }

        return n;
    }
}

inline const std::vector<Preset>& getFactoryPresets()
{
    static const std::vector<Preset> presets = [] {
        std::vector<Preset> p;
        p.push_back ({ "Plano (sin efecto)", { { 0.0, 1.0f, 0.0f }, { 1.0, 1.0f, 0.0f } } });
        p.push_back ({ "Sidechain negras", detail::pump (4, 0.0f) });
        p.push_back ({ "Sidechain corcheas", detail::pump (8, 0.15f) });
        p.push_back ({ "Gate 1/4", detail::gate (4, 0.5) });
        p.push_back ({ "Gate 1/8", detail::gate (8, 0.5) });
        p.push_back ({ "Gate 1/16", detail::gate (16, 0.5) });
        p.push_back ({ "Trance gate", { { 0.0, 1.0f, 0.0f }, { 0.125, 1.0f, 0.0f }, { 0.125, 0.0f, 0.0f },
                                         { 0.1875, 0.0f, 0.0f }, { 0.1875, 1.0f, 0.0f }, { 0.25, 1.0f, 0.0f },
                                         { 0.25, 0.0f, 0.0f }, { 0.375, 0.0f, 0.0f }, { 0.375, 1.0f, 0.0f },
                                         { 0.4375, 1.0f, 0.0f }, { 0.4375, 0.0f, 0.0f }, { 0.5, 0.0f, 0.0f },
                                         { 0.5, 1.0f, 0.0f }, { 0.625, 1.0f, 0.0f }, { 0.625, 0.0f, 0.0f },
                                         { 0.6875, 0.0f, 0.0f }, { 0.6875, 1.0f, 0.0f }, { 0.8125, 1.0f, 0.0f },
                                         { 0.8125, 0.0f, 0.0f }, { 0.875, 0.0f, 0.0f }, { 0.875, 1.0f, 0.0f },
                                         { 0.9375, 1.0f, 0.0f }, { 0.9375, 0.0f, 0.0f }, { 1.0, 0.0f, 0.0f } } });
        p.push_back ({ "Fade in (1 compás)", { { 0.0, 0.0f, 0.5f }, { 1.0, 1.0f, 0.0f } } });
        p.push_back ({ "Fade out (1 compás)", { { 0.0, 1.0f, 0.5f }, { 1.0, 0.0f, 0.0f } } });
        p.push_back ({ "Corte último beat", { { 0.0, 1.0f, 0.0f }, { 0.75, 1.0f, 0.0f },
                                               { 0.75, 0.0f, 0.0f }, { 1.0, 0.0f, 0.0f } } });
        return p;
    }();

    return presets;
}

} // namespace beatshaper
