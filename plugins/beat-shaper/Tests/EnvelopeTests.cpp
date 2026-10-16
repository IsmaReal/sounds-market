// Tests mínimos del modelo de la envolvente (no dependen de JUCE).
#include "Envelope.h"
#include "Presets.h"

#include <cstdio>
#include <cstdlib>

namespace
{
    int failures = 0;

    void check (bool condition, const char* what)
    {
        if (! condition)
        {
            std::printf ("FALLO: %s\n", what);
            ++failures;
        }
    }

    bool near (double a, double b, double tolerance = 1.0e-4) { return std::abs (a - b) <= tolerance; }
}

int main()
{
    using namespace beatshaper;

    {
        Envelope env;
        check (env.getNodes().size() == 2, "envolvente por defecto con dos nodos");
        check (near (env.evaluate (0.37), 1.0), "envolvente plana vale 1");
    }

    {
        Envelope env;
        env.setNodes ({ { 0.0, 0.0f, 0.0f }, { 1.0, 1.0f, 0.0f } });
        check (near (env.evaluate (0.5), 0.5), "rampa lineal en el medio");
        check (near (env.evaluate (0.25), 0.25), "rampa lineal a un cuarto");
    }

    {
        // Salto vertical: dos nodos en la misma X.
        Envelope env;
        env.setNodes ({ { 0.0, 1.0f, 0.0f }, { 0.5, 1.0f, 0.0f }, { 0.5, 0.0f, 0.0f }, { 1.0, 0.0f, 0.0f } });
        check (near (env.evaluate (0.49), 1.0), "antes del salto");
        check (near (env.evaluate (0.5), 0.0), "en el salto gana el último nodo");
        check (near (env.evaluate (0.75), 0.0), "después del salto");
    }

    {
        // Sanitise: orden, extremos y rangos.
        Envelope env;
        env.setNodes ({ { 0.7, 2.0f, 5.0f }, { 0.2, -1.0f, 0.0f } });
        const auto& n = env.getNodes();
        check (n.front().x == 0.0 && n.back().x == 1.0, "extremos fijados en 0 y 1");
        check (n.front().y == 0.0f && n.back().y == 1.0f, "valores recortados a [0, 1]");
        check (n.back().curve == 1.0f, "curva recortada a [-1, 1]");

        env.setNodes ({});
        check (env.getNodes().size() == 2, "lista vacía vuelve a plano");
    }

    {
        // La curva resultante pasa por el punto medio pedido.
        for (double fraction : { 0.1, 0.3, 0.5, 0.8, 0.95 })
        {
            const auto curve = Envelope::curveForMidpointFraction (fraction);
            check (near (Envelope::shape (0.5, curve), fraction, 1.0e-3), "inversa de la curva");
        }
    }

    {
        // Tabla para el hilo de audio.
        Envelope env;
        env.setNodes ({ { 0.0, 0.0f, 0.0f }, { 1.0, 1.0f, 0.0f } });
        float table[17] {};
        env.renderTable (table, 16);
        check (near (table[0], 0.0) && near (table[8], 0.5) && near (table[16], 1.0), "tabla renderizada");
    }

    for (const auto& preset : getFactoryPresets())
    {
        Envelope env;
        env.setNodes (preset.nodes);
        check (env.getNodes().size() == preset.nodes.size(), preset.name.c_str());

        for (int i = 0; i <= 100; ++i)
        {
            const auto v = env.evaluate (i / 100.0);
            check (v >= 0.0 && v <= 1.0, "preset dentro de rango");
        }
    }

    if (failures == 0)
        std::printf ("Todos los tests pasaron.\n");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
