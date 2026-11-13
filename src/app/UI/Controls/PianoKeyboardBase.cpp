#include "PianoKeyboardBase.h"

#include "UI/Utils/AppColorPalette.h"

#include <lite/GUI/Controls/TouchClaimFilter.h>
#include <lite/GUI/Theme/ThemeManager.h>

#include <QMetaMethod>
#include <QPainter>
#include <QWheelEvent>

PianoKeyboardBase::PianoKeyboardBase(QWidget *parent) : QWidget(parent) {
    setMouseTracking(true);
    m_primaryColor = AppColorPalette::instance()->keyHighlight(m_trackColorIndex);

    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this] { refreshThemeColors(); });
}

QColor PianoKeyboardBase::whiteKeyColor() const {
    return m_whiteKeyColor;
}

void PianoKeyboardBase::setWhiteKeyColor(const QColor &whiteKeyColor) {
    m_whiteKeyColor = whiteKeyColor;
    update();
}

QColor PianoKeyboardBase::blackKeyColor() const {
    return m_blackKeyColor;
}

void PianoKeyboardBase::setBlackKeyColor(const QColor &blackKeyColor) {
    m_blackKeyColor = blackKeyColor;
    update();
}

QColor PianoKeyboardBase::dividerColor() const {
    return m_dividerColor;
}

void PianoKeyboardBase::setDividerColor(const QColor &dividerColor) {
    m_dividerColor = dividerColor;
    update();
}

void PianoKeyboardBase::setHoveredKeyIndex(const int keyIndex) {
    if (keyIndex != m_hoveredKeyIndex) {
        m_hoveredKeyIndex = keyIndex;
        update();
    }
}

void PianoKeyboardBase::setTrackColorIndex(const int index) {
    m_trackColorIndex = index;
    m_primaryColor = AppColorPalette::instance()->keyHighlight(index);
    update();
}

void PianoKeyboardBase::setHeldKey(const int key, const bool held) {
    if (key < 0)
        return;
    const bool changed = held ? !m_heldKeys.contains(key) : m_heldKeys.contains(key);
    if (!changed)
        return;
    if (held)
        m_heldKeys.insert(key);
    else
        m_heldKeys.remove(key);
    update();
}

void PianoKeyboardBase::clearHeldKeys() {
    if (m_heldKeys.isEmpty())
        return;
    m_heldKeys.clear();
    update();
}

bool PianoKeyboardBase::isKeyHeld(const int key) const {
    return m_heldKeys.contains(key);
}

const QSet<int> &PianoKeyboardBase::heldKeys() const {
    return m_heldKeys;
}

void PianoKeyboardBase::refreshThemeColors() {
    m_primaryColor = AppColorPalette::instance()->keyHighlight(m_trackColorIndex);
    update();
}

void PianoKeyboardBase::setKeyRange(const double top, const double bottom) {
    m_top = top;
    m_bottom = bottom;
    keyRangeChanged();
    update();
}

void PianoKeyboardBase::keyRangeChanged() {
}

int PianoKeyboardBase::hoveredKeyIndex() const {
    return m_hoveredKeyIndex;
}

int PianoKeyboardBase::pressedKeyIndex() const {
    return m_pressedKeyIndex;
}

double PianoKeyboardBase::keyRangeTop() const {
    return m_top;
}

double PianoKeyboardBase::keyRangeBottom() const {
    return m_bottom;
}

QColor PianoKeyboardBase::overlayColor(const int alpha) const {
    QColor color = m_primaryColor;
    color.setAlpha(alpha);
    return color;
}

void PianoKeyboardBase::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    paintKeyboard(painter);
}

void PianoKeyboardBase::wheelEvent(QWheelEvent *event) {
    // Forward only when someone listens: the piano roll scales its key range
    // with the wheel, while a keyboard embedded in a settings page has to let
    // the event reach the enclosing scroll area.
    if (isSignalConnected(QMetaMethod::fromSignal(&PianoKeyboardBase::wheelScroll))) {
        emit wheelScroll(event);
        event->accept();
        return;
    }
    QWidget::wheelEvent(event);
}

void PianoKeyboardBase::leaveEvent(QEvent *event) {
    QWidget::leaveEvent(event);
    releaseCurrentNote();
    if (m_hoveredKeyIndex != -1) {
        m_hoveredKeyIndex = -1;
        update();
    }
}

void PianoKeyboardBase::mousePressEvent(QMouseEvent *event) {
    QWidget::mousePressEvent(event);
    if (event->button() != Qt::LeftButton)
        return;
    // A touch-initiated press owns the gesture: pin the ancestor scrollers (a
    // running glide included) so the Qt-synthesized mouse drag plays a
    // glissando instead of scrolling the page. A real mouse never drives a
    // QScroller, so for it the call only stops a glide still in progress.
    TouchClaimFilter::stopAncestorScrollers(this);
    const int keyIndex = keyAtPosition(event->position());
    if (keyIndex >= 0) {
        m_pressedKeyIndex = keyIndex;
        playNoteOn(keyIndex);
        update();
    }
}

void PianoKeyboardBase::mouseReleaseEvent(QMouseEvent *event) {
    QWidget::mouseReleaseEvent(event);
    if (event->button() != Qt::LeftButton)
        return;
    releaseCurrentNote();
    update();
}

void PianoKeyboardBase::mouseMoveEvent(QMouseEvent *event) {
    QWidget::mouseMoveEvent(event);
    const int keyIndex = keyAtPosition(event->position());
    if (keyIndex != m_hoveredKeyIndex) {
        m_hoveredKeyIndex = keyIndex;
        update();
    }
    if (event->buttons() & Qt::LeftButton) {
        if (keyIndex != m_pressedKeyIndex) {
            releaseCurrentNote();
            if (keyIndex >= 0) {
                m_pressedKeyIndex = keyIndex;
                playNoteOn(keyIndex);
            }
            update();
        }
    }
}

void PianoKeyboardBase::hideEvent(QHideEvent *event) {
    releaseCurrentNote();
    QWidget::hideEvent(event);
}

void PianoKeyboardBase::releaseCurrentNote() {
    if (m_pressedKeyIndex >= 0) {
        playNoteOff(m_pressedKeyIndex);
        m_pressedKeyIndex = -1;
    }
}
