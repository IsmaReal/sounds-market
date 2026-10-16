#include "EnvelopeEditor.h"

#include "FlowLookAndFeel.h"

namespace
{
    namespace colours
    {
        using namespace flow::ui::colours;
        const juce::Colour node { 0xff1a2620 };
        const juce::Colour curveHandle { 0xffbfeed0 };
        const juce::Colour playhead { 0xffe4e8ee };
    }

    constexpr float nodeRadius = 5.0f;
    constexpr float hitRadius = 9.0f;
    constexpr float leftMargin = 52.0f;
    constexpr float topMargin = 40.0f;
    constexpr float padding = 14.0f;
}

EnvelopeEditor::EnvelopeEditor()
{
    nodes = envelope.getNodes();
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
}

void EnvelopeEditor::setNodes (const std::vector<beatshaper::Node>& newNodes)
{
    // No pisamos la edición en curso.
    if (dragging.type != TargetType::none)
        return;

    envelope.setNodes (newNodes);
    nodes = envelope.getNodes();
    hovered = {};
    repaint();
}

void EnvelopeEditor::setGrid (int beats, int snapPerBeat)
{
    patternBeats = juce::jmax (1, beats);
    snapDivisions = juce::jmax (0, snapPerBeat) * patternBeats;
    repaint();
}

void EnvelopeEditor::setPlayhead (float phase, float gain, bool visible)
{
    if (juce::approximatelyEqual (phase, playheadPhase) && juce::approximatelyEqual (gain, playheadGain)
        && visible == playheadVisible)
        return;

    playheadPhase = phase;
    playheadGain = gain;
    playheadVisible = visible;
    repaint (getGridArea().toNearestInt().expanded (8));
}

void EnvelopeEditor::resized() {}

//==============================================================================
juce::Rectangle<float> EnvelopeEditor::getGridArea() const noexcept
{
    return getLocalBounds().toFloat()
        .withTrimmedLeft (leftMargin)
        .withTrimmedTop (topMargin)
        .withTrimmedRight (padding)
        .withTrimmedBottom (padding);
}

juce::Point<float> EnvelopeEditor::toScreen (double x, double y) const noexcept
{
    const auto area = getGridArea();
    return { area.getX() + (float) x * area.getWidth(),
             area.getBottom() - (float) y * area.getHeight() };
}

double EnvelopeEditor::screenToX (float px) const noexcept
{
    const auto area = getGridArea();
    return juce::jlimit (0.0, 1.0, (double) ((px - area.getX()) / area.getWidth()));
}

double EnvelopeEditor::screenToY (float py) const noexcept
{
    const auto area = getGridArea();
    return juce::jlimit (0.0, 1.0, (double) ((area.getBottom() - py) / area.getHeight()));
}

double EnvelopeEditor::snapX (double x, bool bypass) const noexcept
{
    if (bypass || snapDivisions <= 0)
        return x;

    return std::round (x * snapDivisions) / snapDivisions;
}

juce::Point<float> EnvelopeEditor::curveHandlePosition (int segmentIndex) const noexcept
{
    const auto& a = nodes[(size_t) segmentIndex];
    const auto& b = nodes[(size_t) segmentIndex + 1];
    const auto midX = (a.x + b.x) * 0.5;
    return toScreen (midX, beatshaper::Envelope::valueInSegment (a, b, midX));
}

EnvelopeEditor::Target EnvelopeEditor::findTarget (juce::Point<float> position) const noexcept
{
    // Los nodos tienen prioridad; si hay varios superpuestos tomamos el más cercano.
    auto best = Target {};
    auto bestDistance = hitRadius;

    for (int i = 0; i < (int) nodes.size(); ++i)
    {
        const auto d = toScreen (nodes[(size_t) i].x, nodes[(size_t) i].y).getDistanceFrom (position);

        if (d <= bestDistance)
        {
            best = { TargetType::node, i };
            bestDistance = d;
        }
    }

    if (best.type != TargetType::none)
        return best;

    for (int i = 0; i + 1 < (int) nodes.size(); ++i)
    {
        const auto& a = nodes[(size_t) i];
        const auto& b = nodes[(size_t) i + 1];
        const auto widthPx = toScreen (b.x, 0).x - toScreen (a.x, 0).x;

        if (widthPx < 16.0f || std::abs (a.y - b.y) < 0.02f)
            continue;

        if (curveHandlePosition (i).getDistanceFrom (position) <= hitRadius)
            return { TargetType::curve, i };
    }

    return {};
}

void EnvelopeEditor::refreshEnvelope()
{
    envelope.setNodes (nodes);
    nodes = envelope.getNodes();
}

void EnvelopeEditor::commit()
{
    refreshEnvelope();
    repaint();

    if (onChange)
        onChange (nodes);
}

//==============================================================================
void EnvelopeEditor::mouseMove (const juce::MouseEvent& e)
{
    const auto target = findTarget (e.position);

    if (target.type != hovered.type || target.index != hovered.index)
    {
        hovered = target;
        setMouseCursor (target.type == TargetType::none ? juce::MouseCursor::CrosshairCursor
                        : target.type == TargetType::curve ? juce::MouseCursor::UpDownResizeCursor
                                                           : juce::MouseCursor::DraggingHandCursor);
        repaint();
    }
}

void EnvelopeEditor::mouseExit (const juce::MouseEvent&)
{
    hovered = {};
    repaint();
}

void EnvelopeEditor::mouseDown (const juce::MouseEvent& e)
{
    // El segundo click de un doble click lo resuelve mouseDoubleClick.
    if (e.getNumberOfClicks() > 1)
        return;

    createdOnThisClick = false;
    const auto target = findTarget (e.position);
    const auto lastIndex = (int) nodes.size() - 1;

    if (e.mods.isPopupMenu())
    {
        if (target.type == TargetType::node && target.index > 0 && target.index < lastIndex)
        {
            nodes.erase (nodes.begin() + target.index);
            hovered = {};
            commit();
        }
        else if (target.type == TargetType::curve)
        {
            nodes[(size_t) target.index].curve = 0.0f;
            commit();
        }

        return;
    }

    if (target.type != TargetType::none)
    {
        dragging = target;
        return;
    }

    if (! getGridArea().expanded (hitRadius).contains (e.position))
        return;

    // Click en un espacio vacío: nuevo nodo, insertado después de los que tengan la misma X.
    const auto x = snapX (screenToX (e.position.x), e.mods.isShiftDown());
    const auto y = (float) screenToY (e.position.y);

    auto it = std::upper_bound (nodes.begin(), nodes.end(), x,
                                [] (double value, const beatshaper::Node& n) { return value < n.x; });

    // Nunca insertamos por fuera de los nodos extremos.
    if (it == nodes.begin())
        ++it;
    if (it == nodes.end())
        --it;

    const auto index = (int) std::distance (nodes.begin(), it);
    nodes.insert (it, { x, y, 0.0f });
    dragging = { TargetType::node, index };
    hovered = dragging;
    createdOnThisClick = true;
    commit();
}

void EnvelopeEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging.type == TargetType::none)
        return;

    const auto i = (size_t) dragging.index;

    if (dragging.type == TargetType::node)
    {
        auto& node = nodes[i];
        node.y = (float) screenToY (e.position.y);

        // Los extremos solo se mueven en vertical; el resto queda entre sus vecinos.
        if (i > 0 && i + 1 < nodes.size())
            node.x = juce::jlimit (nodes[i - 1].x, nodes[i + 1].x,
                                   snapX (screenToX (e.position.x), e.mods.isShiftDown()));
    }
    else
    {
        const auto& a = nodes[i];
        const auto& b = nodes[i + 1];
        const auto fraction = (screenToY (e.position.y) - a.y) / (double) (b.y - a.y);
        nodes[i].curve = beatshaper::Envelope::curveForMidpointFraction (fraction);
    }

    commit();
}

void EnvelopeEditor::mouseUp (const juce::MouseEvent&)
{
    dragging = {};
    repaint();
}

void EnvelopeEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (createdOnThisClick)
    {
        createdOnThisClick = false;
        return;
    }

    const auto target = findTarget (e.position);
    const auto lastIndex = (int) nodes.size() - 1;

    if (target.type == TargetType::node && target.index > 0 && target.index < lastIndex)
    {
        nodes.erase (nodes.begin() + target.index);
        hovered = {};
        dragging = {};
        commit();
    }
    else if (target.type == TargetType::curve)
    {
        nodes[(size_t) target.index].curve = 0.0f;
        commit();
    }
}

//==============================================================================
void EnvelopeEditor::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (colours::field);
    g.fillRoundedRectangle (bounds, 6.0f);
    g.setColour (colours::outline);
    g.drawRoundedRectangle (bounds, 6.0f, 1.0f);

    const auto area = getGridArea();
    drawGrid (g, area);
    drawEnvelope (g, area);
}

void EnvelopeEditor::drawGrid (juce::Graphics& g, juce::Rectangle<float> area)
{
    const auto beats = patternBeats;
    const auto beatWidth = area.getWidth() / (float) beats;

    // Subdivisiones según el snap (o semicorcheas si el snap está apagado).
    const auto divisions = snapDivisions > 0 ? snapDivisions : beats * 4;
    const juce::PathStrokeType thin (1.0f);
    const float dashes[] = { 2.0f, 4.0f };

    for (int d = 1; d < divisions; ++d)
    {
        const auto x = area.getX() + area.getWidth() * (float) d / (float) divisions;
        juce::Path line, dashed;
        line.startNewSubPath (x, area.getY());
        line.lineTo (x, area.getBottom());
        thin.createDashedStroke (dashed, line, dashes, 2);
        g.setColour (colours::gridMinor);
        g.fillPath (dashed);
    }

    // Líneas horizontales de valor.
    for (int level = 1; level < 8; ++level)
    {
        const auto y = area.getBottom() - area.getHeight() * (float) level / 8.0f;
        juce::Path line, dashed;
        line.startNewSubPath (area.getX(), y);
        line.lineTo (area.getRight(), y);
        thin.createDashedStroke (dashed, line, dashes, 2);
        g.setColour (level == 4 ? colours::gridMajor.withAlpha (0.8f) : colours::gridMinor);
        g.fillPath (dashed);
    }

    // Beats: línea completa desde arriba de la grilla y número.
    for (int b = 0; b < beats; ++b)
    {
        const auto x = area.getX() + (float) b * beatWidth;
        g.setColour (colours::gridMajor);
        g.drawLine (x, area.getY() - topMargin + 10.0f, x, area.getBottom(), 1.0f);

        g.setColour (colours::text.withAlpha (0.85f));
        g.setFont (juce::FontOptions (15.0f));
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (x + 8.0f, area.getY() - topMargin + 10.0f, 40.0f, 22.0f),
                    juce::Justification::centredLeft);
    }

    // Línea de referencia arriba de los números y base.
    g.setColour (colours::gridMajor);
    g.drawLine (area.getX(), area.getY() - 6.0f, area.getRight(), area.getY() - 6.0f, 1.0f);
    g.setColour (colours::gridMajor.withAlpha (0.8f));
    g.drawLine (area.getX(), area.getBottom(), area.getRight(), area.getBottom(), 1.0f);

    // Etiquetas del eje Y.
    g.setColour (colours::textMuted);
    g.setFont (juce::FontOptions (14.0f));

    for (auto [value, text] : { std::pair { 1.0f, "1.0" }, std::pair { 0.5f, "0.5" }, std::pair { 0.0f, "0.0" } })
    {
        const auto y = area.getBottom() - area.getHeight() * value;
        g.drawText (text, juce::Rectangle<float> (0.0f, y - 9.0f, leftMargin - 14.0f, 18.0f),
                    juce::Justification::centredRight);
        g.drawLine (area.getX() - 6.0f, y, area.getX(), y, 1.0f);
    }
}

void EnvelopeEditor::drawEnvelope (juce::Graphics& g, juce::Rectangle<float> area)
{
    // Trazo de la curva, segmento por segmento (así los saltos verticales quedan exactos).
    juce::Path line;
    line.startNewSubPath (toScreen (nodes.front().x, nodes.front().y));

    for (size_t i = 0; i + 1 < nodes.size(); ++i)
    {
        const auto& a = nodes[i];
        const auto& b = nodes[i + 1];
        const auto widthPx = (b.x - a.x) * area.getWidth();
        const auto steps = juce::jlimit (1, 400, (int) std::ceil (widthPx / 2.0));

        line.lineTo (toScreen (a.x, a.y));

        for (int s = 1; s <= steps; ++s)
        {
            const auto x = a.x + (b.x - a.x) * s / steps;
            line.lineTo (toScreen (x, beatshaper::Envelope::valueInSegment (a, b, x)));
        }
    }

    juce::Path fill (line);
    fill.lineTo (area.getBottomRight());
    fill.lineTo (area.getBottomLeft());
    fill.closeSubPath();

    g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.32f), area.getTopLeft(),
                                             colours::accent.withAlpha (0.04f), area.getBottomLeft(), false));
    g.fillPath (fill);

    g.setColour (colours::accent.withAlpha (0.25f));
    g.strokePath (line, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::accent);
    g.strokePath (line, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Puntos de curva.
    for (int i = 0; i + 1 < (int) nodes.size(); ++i)
    {
        const auto& a = nodes[(size_t) i];
        const auto& b = nodes[(size_t) i + 1];

        if ((b.x - a.x) * area.getWidth() < 16.0 || std::abs (a.y - b.y) < 0.02f)
            continue;

        const auto active = (hovered.type == TargetType::curve && hovered.index == i)
                         || (dragging.type == TargetType::curve && dragging.index == i);
        const auto p = curveHandlePosition (i);
        const auto r = active ? 4.5f : 3.0f;

        g.setColour (colours::curveHandle.withAlpha (active ? 1.0f : 0.45f));
        g.drawEllipse (p.x - r, p.y - r, r * 2, r * 2, 1.5f);
    }

    // Nodos: círculo oscuro con borde verde, como en la referencia.
    for (int i = 0; i < (int) nodes.size(); ++i)
    {
        const auto p = toScreen (nodes[(size_t) i].x, nodes[(size_t) i].y);
        const auto active = (hovered.type == TargetType::node && hovered.index == i)
                         || (dragging.type == TargetType::node && dragging.index == i);
        const auto r = active ? nodeRadius + 2.0f : nodeRadius + 0.5f;

        if (active)
        {
            g.setColour (colours::accent.withAlpha (0.25f));
            g.fillEllipse (p.x - r - 5.0f, p.y - r - 5.0f, (r + 5.0f) * 2, (r + 5.0f) * 2);
        }

        g.setColour (active ? colours::accent : colours::node);
        g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
        g.setColour (colours::accent);
        g.drawEllipse (p.x - r, p.y - r, r * 2, r * 2, 2.2f);
    }

    // Etiqueta con posición y volumen del nodo que se está arrastrando.
    if (dragging.type == TargetType::node)
    {
        const auto& n = nodes[(size_t) dragging.index];
        const auto beat = n.x * patternBeats;
        const auto text = "Beat " + juce::String (1.0 + beat, 2) + "  |  " + juce::String (n.y, 2);
        const auto p = toScreen (n.x, n.y);

        auto box = juce::Rectangle<float> (120.0f, 20.0f).withCentre (p.translated (0.0f, n.y > 0.8f ? 22.0f : -22.0f));
        box = box.constrainedWithin (area);

        g.setColour (colours::panel.withAlpha (0.95f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (colours::outline);
        g.drawRoundedRectangle (box, 4.0f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (text, box, juce::Justification::centred);
    }

    // Cursor de reproducción.
    if (playheadVisible)
    {
        const auto p = toScreen (playheadPhase, playheadGain);
        g.setColour (colours::playhead.withAlpha (0.35f));
        g.drawLine (p.x, area.getY(), p.x, area.getBottom(), 1.5f);
        g.setColour (colours::playhead);
        g.fillEllipse (p.x - 3.5f, p.y - 3.5f, 7.0f, 7.0f);
    }
}
