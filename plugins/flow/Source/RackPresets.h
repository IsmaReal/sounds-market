#pragma once

#include <juce_core/juce_core.h>

#include <vector>

// Presets generales de Flow: guardan el rack completo (módulos, orden, envolventes,
// parámetros y ajustes de la grilla).
namespace flow
{

struct RackPreset
{
    juce::String name;
    juce::String xml; // estado en XML (como lo genera FlowProcessor::createStateTree). Vacío = rack inicial.
};

// Presets de fábrica. Para sumar uno: armalo en el plugin, guardalo con el ícono de
// guardar y pegá acá el contenido del archivo .flowpreset.
inline const std::vector<RackPreset>& getFactoryRackPresets()
{
    static const std::vector<RackPreset> presets {
        { "Init", {} },
    };

    return presets;
}

// Carpeta de presets del usuario: Documentos/Flow/Presets.
inline juce::File getUserPresetFolder()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Flow")
        .getChildFile ("Presets");
}

inline juce::Array<juce::File> getUserRackPresets()
{
    auto files = getUserPresetFolder().findChildFiles (juce::File::findFiles, false, "*.flowpreset");
    files.sort();
    return files;
}

constexpr const char* presetFileExtension = ".flowpreset";

} // namespace flow
