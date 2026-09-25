#include "EditorPenStroke.h"

#include <cmath>

EditorPenStroke::EditorPenStroke() = default;

EditorPenStroke::EditorPenStroke(const Config &config) : m_config(config) {
}

void EditorPenStroke::setConfig(const Config &config) {
    m_config = config;
}

const EditorPenStroke::Config &EditorPenStroke::config() const {
    return m_config;
}

bool EditorPenStroke::hasSideButton(const Qt::MouseButtons buttons) {
    return (buttons & ~Qt::LeftButton) != Qt::NoButton;
}

bool EditorPenStroke::needsTranslation(const Sample &sample) {
    if (sample.pointerType == QPointingDevice::PointerType::Eraser)
        return true;
    return hasSideButton(sample.buttons);
}

EditorPenStroke::Input EditorPenStroke::classify(const Sample &sample) {
    // The eraser is checked first: while the tip is in contact the platform
    // reports the barrel flag only for the pen tip, but an inverted pen that
    // also has the barrel down must still be read as an eraser.
    if (sample.pointerType == QPointingDevice::PointerType::Eraser)
        return Input::Eraser;
    if (hasSideButton(sample.buttons))
        return Input::SideButton;
    return Input::Tip;
}

EditorPenStroke::Phase EditorPenStroke::phase() const {
    return m_phase;
}

EditorPenStroke::Input EditorPenStroke::input() const {
    return m_input;
}

bool EditorPenStroke::erases() const {
    switch (m_input) {
        case Input::Eraser:
            return m_eraserSupported;
        case Input::SideButton:
            return m_sideButtonResolved && m_eraserSupported;
        case Input::Tip:
            return false;
    }
    return false;
}

QPointF EditorPenStroke::origin() const {
    return m_origin;
}

void EditorPenStroke::reset() {
    m_phase = Phase::Idle;
    m_sideButtonResolved = false;
}

EditorPenStroke::Intents EditorPenStroke::pressed(const Sample &sample, const bool eraserSupported) {
    m_phase = Phase::Stroke;
    m_input = classify(sample);
    m_origin = sample.position;
    m_lastPosition = sample.position;
    m_eraserSupported = eraserSupported;
    m_sideButtonResolved = false;

    Intents intents;
    switch (m_input) {
        case Input::Tip:
            intents.append({Intent::Type::Begin, sample.position, false});
            break;
        case Input::Eraser:
            // Nothing to erase under this tool: claim the stroke and let it
            // produce nothing at all.
            if (eraserSupported)
                intents.append({Intent::Type::Begin, sample.position, true});
            break;
        case Input::SideButton:
            // Deferred. A click opens a menu, a drag erases, and until the pen
            // travels there is no way to tell them apart.
            break;
    }
    return intents;
}

EditorPenStroke::Intents EditorPenStroke::moved(const Sample &sample) {
    Intents intents;
    if (m_phase != Phase::Stroke)
        return intents;

    m_lastPosition = sample.position;

    switch (m_input) {
        case Input::Tip:
            intents.append({Intent::Type::Move, sample.position, false});
            break;
        case Input::Eraser:
            if (m_eraserSupported)
                intents.append({Intent::Type::Move, sample.position, true});
            break;
        case Input::SideButton: {
            if (!m_sideButtonResolved) {
                const auto travel = sample.position - m_origin;
                if (std::hypot(travel.x(), travel.y()) <= m_config.sideButtonSlopPx)
                    break;
                m_sideButtonResolved = true;
                // Past the slop this is an erase stroke after all. Press where
                // the pen was put down, so erasing starts there rather than at
                // the point that crossed the threshold, and carry the sample
                // that crossed it in the same frame so the segment between the
                // two is not skipped.
                if (m_eraserSupported) {
                    intents.append({Intent::Type::Begin, m_origin, true});
                    intents.append({Intent::Type::Move, sample.position, true});
                }
                break;
            }
            if (m_eraserSupported)
                intents.append({Intent::Type::Move, sample.position, true});
            break;
        }
    }
    return intents;
}

EditorPenStroke::Intents EditorPenStroke::released(const Sample &sample) {
    Intents intents;
    if (m_phase != Phase::Stroke)
        return intents;

    const auto input = m_input;
    const auto supported = m_eraserSupported;
    const auto resolved = m_sideButtonResolved;
    const auto origin = m_origin;
    reset();

    switch (input) {
        case Input::Tip:
            intents.append({Intent::Type::End, sample.position, false});
            break;
        case Input::SideButton:
            if (!resolved) {
                // Never travelled, so this was a click on the barrel button and
                // not an erase request: hand it to the context menu path, the
                // same way it arrived before the pen layer existed.
                intents.append({Intent::Type::ContextMenu, origin, false});
            } else if (supported) {
                intents.append({Intent::Type::End, sample.position, true});
            }
            break;
        case Input::Eraser:
            if (supported)
                intents.append({Intent::Type::End, sample.position, true});
            break;
    }
    return intents;
}

EditorPenStroke::Intents EditorPenStroke::feed(const Report report, const Sample &sample) {
    if (m_phase != Phase::Stroke)
        return Intents();
    // Every report is read against the contact state, never against its own
    // type: that single rule is what makes the mid-stroke press/release noise
    // harmless, and it also covers the lift being announced as a move.
    if (sample.pressure <= 0.0)
        return released(sample);
    // Still on the glass, so nothing here is a boundary. Only a move carries
    // information the stroke does not already have; a press or release at this
    // point can only be the platform reacting to the barrel button changing
    // state, and says nothing about the tip (the stroke's input was locked when
    // it began, and a second press would restart it).
    if (report == Report::Move)
        return moved(sample);
    return Intents();
}

EditorPenStroke::Intents EditorPenStroke::cancelled() {
    Intents intents;
    if (m_phase != Phase::Stroke)
        return intents;

    const auto input = m_input;
    const auto supported = m_eraserSupported;
    const auto resolved = m_sideButtonResolved;
    const auto last = m_lastPosition;
    reset();

    // An aborted stroke never opens a menu, however short it was.
    switch (input) {
        case Input::Tip:
            intents.append({Intent::Type::End, last, false});
            break;
        case Input::Eraser:
            if (supported)
                intents.append({Intent::Type::End, last, true});
            break;
        case Input::SideButton:
            if (resolved && supported)
                intents.append({Intent::Type::End, last, true});
            break;
    }
    return intents;
}
