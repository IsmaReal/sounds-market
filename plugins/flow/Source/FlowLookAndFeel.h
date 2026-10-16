#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Estética de Flow: fondo grafito, paneles redondeados y acento verde.
namespace flow::ui
{

namespace colours
{
    const juce::Colour background { 0xff12161c };
    const juce::Colour panel { 0xff181d24 };
    const juce::Colour card { 0xff1c222a };
    const juce::Colour cardSelected { 0xff1d2a26 };
    const juce::Colour field { 0xff141920 };
    const juce::Colour outline { 0xff2a313b };
    const juce::Colour gridMajor { 0xff36404c };
    const juce::Colour gridMinor { 0xff222932 };
    const juce::Colour text { 0xffe4e8ee };
    const juce::Colour textMuted { 0xff8b94a1 };
    const juce::Colour accent { 0xff6ee59a };
    const juce::Colour accentDim { 0xff2f6b47 };
    const juce::Colour track { 0xff0f1318 };
    const juce::Colour thumb { 0xffe8ebf0 };
}

// Fuente con espaciado entre letras, para los títulos en mayúsculas.
inline juce::Font spacedFont (float height, bool bold = false, float kerning = 0.18f)
{
    return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain))
        .withExtraKerningFactor (kerning);
}

class FlowLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    FlowLookAndFeel();

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
};

// Botón con ícono dibujado (encendido, guardar, menú, flechas).
class IconButton final : public juce::Button
{
public:
    enum class Icon { power, save, menu, previous, next, close };

    IconButton (Icon iconToUse, const juce::String& name = {});

    void setActive (bool shouldBeActive);

private:
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    Icon icon;
    bool active = true;
};

// Logo "FLOW" con la onda y el subtítulo.
void drawLogo (juce::Graphics&, juce::Rectangle<float> area);

} // namespace flow::ui
