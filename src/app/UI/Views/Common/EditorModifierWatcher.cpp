#include "EditorModifierWatcher.h"

#include <QApplication>
#include <QEvent>
#include <QKeyEvent>

#include <utility>

namespace {

    bool isModifierKey(const int key) {
        return key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt ||
               key == Qt::Key_Meta;
    }

    // The event's own modifier state already accounts for the key that changed,
    // but spelling the change out keeps synthetic key events (tests, IME paths)
    // consistent with what a physical keyboard produces.
    Qt::KeyboardModifiers applyModifier(const Qt::KeyboardModifiers modifiers, const int key,
                                        const bool pressed) {
        switch (key) {
            case Qt::Key_Control:
                return pressed ? modifiers | Qt::ControlModifier : modifiers & ~Qt::ControlModifier;
            case Qt::Key_Shift:
                return pressed ? modifiers | Qt::ShiftModifier : modifiers & ~Qt::ShiftModifier;
            case Qt::Key_Alt:
                return pressed ? modifiers | Qt::AltModifier : modifiers & ~Qt::AltModifier;
            case Qt::Key_Meta:
                return pressed ? modifiers | Qt::MetaModifier : modifiers & ~Qt::MetaModifier;
            default:
                return modifiers;
        }
    }

} // namespace

EditorModifierWatcher::EditorModifierWatcher(QObject *owner,
                                             std::function<void(Qt::KeyboardModifiers)> onChanged)
    : QObject(owner), m_onChanged(std::move(onChanged)) {
    qApp->installEventFilter(this);
}

void EditorModifierWatcher::setEnabled(const bool enabled) {
    m_enabled = enabled;
}

bool EditorModifierWatcher::isEnabled() const {
    return m_enabled;
}

bool EditorModifierWatcher::eventFilter(QObject *watched, QEvent *event) {
    Q_UNUSED(watched)
    if (!m_enabled || !m_onChanged)
        return false;
    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease)
        return false;
    const auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (!isModifierKey(keyEvent->key()))
        return false;
    m_onChanged(
        applyModifier(keyEvent->modifiers(), keyEvent->key(), event->type() == QEvent::KeyPress));
    return false;
}
