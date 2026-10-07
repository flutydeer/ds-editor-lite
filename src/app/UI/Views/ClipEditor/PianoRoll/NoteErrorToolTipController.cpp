#include "NoteErrorToolTipController.h"

#include <lite/GUI/Controls/ToolTip.h>

#include <QObject>
#include <QStyle>
#include <QTextDocument>

NoteErrorToolTipController::NoteErrorToolTipController(QWidget *parent)
    : m_toolTip(std::make_unique<ToolTip>(QString(), parent)) {
    m_hoverTimer.setSingleShot(true);
    QObject::connect(&m_hoverTimer, &QTimer::timeout, [this] { showPending(); });
}

NoteErrorToolTipController::~NoteErrorToolTipController() = default;

void NoteErrorToolTipController::showFor(const int noteId, const QString &title,
                                         const QString &message, const QRect &screenAnchor) {
    if (title.isEmpty() || message.isEmpty() || screenAnchor.isEmpty()) {
        hide();
        return;
    }
    m_hoverTimer.stop();
    m_pendingNoteId = -1;

    if (m_noteId == noteId && m_title == title && m_message == message && m_toolTip->isVisible())
        return;

    m_noteId = noteId;
    m_title = title;
    m_message = message;
    m_toolTip->setTitle(Qt::convertFromPlainText(title));
    m_toolTip->setMessage({Qt::convertFromPlainText(message)});
    m_toolTip->showAbove(screenAnchor);
}

void NoteErrorToolTipController::hoverFor(const int noteId, const QString &title,
                                          const QString &message, const QRect &screenAnchor) {
    if (title.isEmpty() || message.isEmpty() || screenAnchor.isEmpty()) {
        hide();
        return;
    }
    // A card already settled on this badge must not reposition or flicker
    if (m_toolTip->isVisible() && m_noteId == noteId && m_title == title && m_message == message)
        return;
    // Neither must a countdown that is already running for this badge restart
    if (m_hoverTimer.isActive() && m_pendingNoteId == noteId && m_pendingTitle == title &&
        m_pendingMessage == message)
        return;

    m_pendingNoteId = noteId;
    m_pendingTitle = title;
    m_pendingMessage = message;
    m_pendingAnchor = screenAnchor;
    // The same delays QToolTip itself honours: the wake-up delay raises a card
    // from nothing, the fall-asleep delay applies while a card is still on
    // screen, possibly mid fade-out on its way to another badge
    const auto *style = m_toolTip->style();
    m_hoverTimer.start(style->styleHint(m_toolTip->isVisible() ? QStyle::SH_ToolTip_FallAsleepDelay
                                                               : QStyle::SH_ToolTip_WakeUpDelay));
}

void NoteErrorToolTipController::hide() {
    m_hoverTimer.stop();
    m_pendingNoteId = -1;
    m_noteId = -1;
    m_title.clear();
    m_message.clear();
    if (m_toolTip->isVisible())
        m_toolTip->hideWithAnimation();
}

void NoteErrorToolTipController::showPending() {
    showFor(m_pendingNoteId, m_pendingTitle, m_pendingMessage, m_pendingAnchor);
}
