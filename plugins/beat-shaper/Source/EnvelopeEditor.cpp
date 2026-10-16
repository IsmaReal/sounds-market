#include "EnvelopeEditor.h"

namespace
{
    namespace colours
    {
        const juce::Colour background { 0xff15151c };
        const juce::Colour beatEven { 0xff1c1c26 };
        const juce::Colour beatOdd { 0xff191921 };
        const juce::Colour gridMajor { 0xff3a3a4a };
        const juce::Colour gridMinor { 0xff26262f };
        const juce::Colour label { 0xff8a8a9c };
        const juce::Colour accent { 0xffff8a3d };
        const juce::Colour node { 0xfffff1e6 };
        const juce::Colour curveHandle { 0xff7fd1ff };
        const juce::Colour playhead { 0xff6dffb0 };
    }

    constexpr float nodeRadius = 5.0f;
    constexpr float hitRadius = 9.0f;
    constexpr float leftMargin = 40.0f;
    constexpr float topMargin = 22.0f;
    constexpr float padding = 8.0f;
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

void EnvelopeEditor::setSnapDivisions (int divisions)
{
    snapDivisions = juce::jmax (0, divisions);
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
    g.fillAll (colours::background);

    const auto area = getGridArea();
    drawGrid (g, area);
    drawEnvelope (g, area);
}

void EnvelopeEditor::drawGrid (juce::Graphics& g, juce::Rectangle<float> area)
{
    constexpr auto beats = beatshaper::Envelope::patternBeats;
    const auto beatWidth = area.getWidth() / beats;

    for (int b = 0; b < beats; ++b)
    {
        const auto column = area.withX (area.getX() + b * beatWidth).withWidth (beatWidth);
        g.setColour (b % 2 == 0 ? colours::beatEven : colours::beatOdd);
        g.fillRect (column);

        g.setColour (colours::label);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String (b + 1), column.withY (0).withHeight (topMargin).reduced (4, 0),
                    juce::Justification::centredLeft);
    }

    // Subdivisiones según el snap (o semicorcheas si el snap está apagado).
    const auto divisions = snapDivisions > 0 ? snapDivisions : 16;
    g.setColour (colours::gridMinor);

    for (int d = 1; d < divisions; ++d)
    {
        const auto x = area.getX() + area.getWidth() * (float) d / (float) divisions;
        g.drawVerticalLine (juce::roundToInt (x), area.getY(), area.getBottom());
    }

    g.setColour (colours::gridMajor);

    for (int b = 0; b <= beats; ++b)
        g.drawVerticalLine (juce::roundToInt (juce::jmin (area.getX() + b * beatWidth, area.getRight() - 1.0f)),
                            area.getY(), area.getBottom());

    // Líneas horizontales de volumen.
    g.setFont (juce::FontOptions (11.0f));

    for (int level = 0; level <= 4; ++level)
    {
        const auto y = area.getBottom() - area.getHeight() * (float) level / 4.0f;
        g.setColour (level == 0 || level == 4 ? colours::gridMajor : colours::gridMinor);
        g.drawHorizontalLine (juce::roundToInt (juce::jmin (y, area.getBottom() - 1.0f)), area.getX(), area.getRight());

        g.setColour (colours::label);
        g.drawText (juce::String (level * 25) + "%",
                    juce::Rectangle<float> (0.0f, y - 7.0f, leftMargin - 6.0f, 14.0f),
                    juce::Justification::centredRight);
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

    g.setGradientFill (juce::ColourGradient (colours::accent.withAlpha (0.45f), area.getTopLeft(),
                                             colours::accent.withAlpha (0.05f), area.getBottomLeft(), false));
    g.fillPath (fill);

    g.setColour (colours::accent);
    g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

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

        g.setColour (colours::curveHandle.withAlpha (active ? 1.0f : 0.7f));
        g.drawEllipse (p.x - r, p.y - r, r * 2, r * 2, 1.5f);
    }

    // Nodos.
    for (int i = 0; i < (int) nodes.size(); ++i)
    {
        const auto p = toScreen (nodes[(size_t) i].x, nodes[(size_t) i].y);
        const auto active = (hovered.type == TargetType::node && hovered.index == i)
                         || (dragging.type == TargetType::node && dragging.index == i);
        const auto r = active ? nodeRadius + 1.5f : nodeRadius;

        g.setColour (colours::background);
        g.fillEllipse (p.x - r - 1.5f, p.y - r - 1.5f, (r + 1.5f) * 2, (r + 1.5f) * 2);
        g.setColour (active ? colours::accent : colours::node);
        g.fillEllipse (p.x - r, p.y - r, r * 2, r * 2);
    }

    // Etiqueta con posición y volumen del nodo que se está arrastrando.
    if (dragging.type == TargetType::node)
    {
        const auto& n = nodes[(size_t) dragging.index];
        const auto beat = n.x * beatshaper::Envelope::patternBeats;
        const auto text = "Beat " + juce::String (1.0 + beat, 2) + "  |  " + juce::String (juce::roundToInt (n.y * 100.0f)) + "%";
        const auto p = toScreen (n.x, n.y);

        auto box = juce::Rectangle<float> (120.0f, 20.0f).withCentre (p.translated (0.0f, n.y > 0.8f ? 22.0f : -22.0f));
        box = box.constrainedWithin (area);

        g.setColour (juce::Colours::black.withAlpha (0.75f));
        g.fillRoundedRectangle (box, 4.0f);
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (text, box, juce::Justification::centred);
    }

    // Cursor de reproducción.
    if (playheadVisible)
    {
        const auto p = toScreen (playheadPhase, playheadGain);
        g.setColour (colours::playhead.withAlpha (0.8f));
        g.drawLine (p.x, area.getY(), p.x, area.getBottom(), 1.5f);
        g.fillEllipse (p.x - 4.0f, p.y - 4.0f, 8.0f, 8.0f);
    }
}
