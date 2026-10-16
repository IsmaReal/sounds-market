#include "FlowLookAndFeel.h"

namespace flow::ui
{

FlowLookAndFeel::FlowLookAndFeel()
{
    setColourScheme (getMidnightColourScheme());

    setColour (juce::ResizableWindow::backgroundColourId, colours::background);
    setColour (juce::Slider::textBoxTextColourId, colours::text);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::text);
    setColour (juce::ComboBox::textColourId, colours::text);
    setColour (juce::ComboBox::backgroundColourId, colours::field);
    setColour (juce::ComboBox::outlineColourId, colours::outline);
    setColour (juce::ComboBox::arrowColourId, colours::text);
    setColour (juce::PopupMenu::backgroundColourId, colours::panel);
    setColour (juce::PopupMenu::textColourId, colours::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::accentDim);
    setColour (juce::PopupMenu::highlightedTextColourId, colours::text);
    setColour (juce::ScrollBar::thumbColourId, colours::outline);
}

void FlowLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float, float, juce::Slider::SliderStyle,
                                        juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto trackHeight = 6.0f;
    const auto track = bounds.withSizeKeepingCentre (bounds.getWidth(), trackHeight);
    const auto enabled = slider.isEnabled();

    g.setColour (colours::track);
    g.fillRoundedRectangle (track, trackHeight * 0.5f);

    const auto fill = track.withRight (sliderPos);
    g.setColour (enabled ? colours::accent : colours::textMuted.withAlpha (0.5f));
    g.fillRoundedRectangle (fill, trackHeight * 0.5f);

    const auto thumbSize = 18.0f;
    const auto thumb = juce::Rectangle<float> (thumbSize, thumbSize).withCentre ({ sliderPos, track.getCentreY() });
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillEllipse (thumb.translated (0.0f, 1.5f));
    g.setColour (enabled ? colours::thumb : colours::textMuted);
    g.fillEllipse (thumb);
}

juce::Label* FlowLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (15.0f));
    label->setJustificationType (juce::Justification::centredRight);
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    return label;
}

void FlowLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

    g.setColour (box.findColour (juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.isMouseOver (true) ? colours::gridMajor.brighter (0.2f) : colours::outline);
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    // Triángulo
    const auto arrow = juce::Rectangle<float> ((float) width - 30.0f, 0.0f, 20.0f, (float) height)
                           .withSizeKeepingCentre (10.0f, 6.0f);
    juce::Path p;
    p.addTriangle (arrow.getTopLeft(), arrow.getTopRight(), { arrow.getCentreX(), arrow.getBottom() });
    g.setColour (colours::text);
    g.fillPath (p);
}

void FlowLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (12, 1, box.getWidth() - 44, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font FlowLookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return juce::FontOptions (juce::jmin (17.0f, (float) box.getHeight() * 0.48f));
}

juce::Font FlowLookAndFeel::getPopupMenuFont()
{
    return juce::FontOptions (15.0f);
}

void FlowLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (colours::panel);
    g.setColour (colours::outline);
    g.drawRect (0, 0, width, height);
}

//==============================================================================
IconButton::IconButton (Icon iconToUse, const juce::String& name)
    : juce::Button (name), icon (iconToUse)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::setActive (bool shouldBeActive)
{
    active = shouldBeActive;
    repaint();
}

void IconButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto area = getLocalBounds().toFloat().reduced (2.0f);
    const auto size = juce::jmin (area.getWidth(), area.getHeight());
    area = area.withSizeKeepingCentre (size, size);

    auto colour = icon == Icon::power ? (active ? colours::accent : colours::textMuted)
                                      : colours::text.withAlpha (0.85f);

    if (highlighted)
        colour = colour.brighter (0.25f);
    if (down)
        colour = colour.darker (0.2f);

    g.setColour (colour);
    const auto stroke = juce::jmax (1.6f, size * 0.09f);
    const juce::PathStrokeType strokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path p;

    switch (icon)
    {
        case Icon::power:
        {
            const auto r = area.reduced (size * 0.12f);
            p.addCentredArc (r.getCentreX(), r.getCentreY(), r.getWidth() * 0.5f, r.getHeight() * 0.5f,
                             0.0f, juce::degreesToRadians (35.0f), juce::degreesToRadians (325.0f), true);
            p.startNewSubPath (r.getCentreX(), r.getY() - stroke * 0.3f);
            p.lineTo (r.getCentreX(), r.getCentreY() - r.getHeight() * 0.05f);
            g.strokePath (p, strokeType);
            break;
        }

        case Icon::save:
        {
            const auto r = area.reduced (size * 0.16f);
            p.addRoundedRectangle (r, size * 0.06f);
            g.strokePath (p, strokeType);

            juce::Path details;
            const auto top = r.withHeight (r.getHeight() * 0.32f).reduced (r.getWidth() * 0.22f, 0.0f);
            details.addRectangle (top.withTrimmedBottom (0.0f));
            details.addRectangle (r.withTrimmedTop (r.getHeight() * 0.58f).reduced (r.getWidth() * 0.2f, 0.0f)
                                      .withTrimmedBottom (r.getHeight() * 0.0f));
            g.strokePath (details, juce::PathStrokeType (stroke * 0.75f));
            break;
        }

        case Icon::menu:
        {
            const auto r = area.reduced (size * 0.12f, size * 0.22f);

            for (int i = 0; i < 3; ++i)
            {
                const auto y = r.getY() + r.getHeight() * (float) i / 2.0f;
                p.startNewSubPath (r.getX(), y);
                p.lineTo (r.getRight(), y);
            }

            g.strokePath (p, juce::PathStrokeType (stroke * 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }

        case Icon::previous:
        case Icon::next:
        {
            const auto r = area.reduced (size * 0.32f);
            const auto left = icon == Icon::previous;
            p.startNewSubPath (left ? r.getRight() - r.getWidth() * 0.25f : r.getX() + r.getWidth() * 0.25f, r.getY());
            p.lineTo (left ? r.getX() + r.getWidth() * 0.25f : r.getRight() - r.getWidth() * 0.25f, r.getCentreY());
            p.lineTo (left ? r.getRight() - r.getWidth() * 0.25f : r.getX() + r.getWidth() * 0.25f, r.getBottom());
            g.strokePath (p, juce::PathStrokeType (stroke * 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }

        case Icon::close:
        {
            const auto r = area.reduced (size * 0.3f);
            p.startNewSubPath (r.getTopLeft());
            p.lineTo (r.getBottomRight());
            p.startNewSubPath (r.getTopRight());
            p.lineTo (r.getBottomLeft());
            g.setColour (colours::textMuted.withAlpha (highlighted ? 1.0f : 0.6f));
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            break;
        }
    }
}

//==============================================================================
void drawLogo (juce::Graphics& g, juce::Rectangle<float> area)
{
    // "FLOW" en trazo fino y espaciado
    const auto wordWidth = area.getHeight() * 4.6f;
    auto word = area.removeFromLeft (wordWidth);

    g.setColour (colours::text);
    g.setFont (juce::Font (juce::FontOptions (area.getHeight() * 0.95f, juce::Font::plain))
                   .withExtraKerningFactor (0.12f)
                   .withHorizontalScale (1.25f));
    g.drawFittedText ("FLOW", word.toNearestInt(), juce::Justification::centredLeft, 1, 0.5f);

    // Onda
    area.removeFromLeft (area.getHeight() * 0.3f);
    const auto wave = area.removeFromLeft (area.getHeight() * 1.3f).reduced (0.0f, area.getHeight() * 0.18f);
    juce::Path p;

    for (int i = 0; i <= 60; ++i)
    {
        const auto t = (float) i / 60.0f;
        const auto envelope = std::sin (t * juce::MathConstants<float>::pi);
        const auto y = wave.getCentreY() - std::sin (t * juce::MathConstants<float>::twoPi * 1.5f) * envelope * wave.getHeight() * 0.5f;
        const auto x = wave.getX() + t * wave.getWidth();

        if (i == 0)
            p.startNewSubPath (x, y);
        else
            p.lineTo (x, y);
    }

    g.setColour (colours::accent);
    g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Subtítulo en tres líneas
    area.removeFromLeft (area.getHeight() * 0.4f);
    auto subtitle = area.withSizeKeepingCentre (area.getWidth(), area.getHeight() * 0.9f);
    g.setColour (colours::text.withAlpha (0.8f));
    g.setFont (spacedFont (subtitle.getHeight() * 0.25f, false, 0.35f));
    const auto lineHeight = subtitle.getHeight() / 3.0f;

    for (auto* line : { "RHYTHMIC", "SHAPING", "TOOL" })
        g.drawText (line, subtitle.removeFromTop (lineHeight), juce::Justification::centredLeft, false);
}

} // namespace flow::ui
