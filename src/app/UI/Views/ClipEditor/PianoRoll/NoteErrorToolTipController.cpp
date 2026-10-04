#include "NoteErrorToolTipController.h"

#include <lite/GUI/Controls/ToolTip.h>

#include <QTextDocument>

NoteErrorToolTipController::NoteErrorToolTipController(QWidget *parent)
    : m_toolTip(std::make_unique<ToolTip>(QString(), parent)) {
    m_toolTip->setAnimationEnabled(false);
}

NoteErrorToolTipController::~NoteErrorToolTipController() = default;

void NoteErrorToolTipController::showFor(const int noteId, const QString &title,
                                         const QString &message, const QRect &screenAnchor) {
    if (title.isEmpty() || message.isEmpty() || screenAnchor.isEmpty()) {
        hide();
        return;
    }
    if (m_noteId == noteId && m_title == title && m_message == message && m_toolTip->isVisible())
        return;

    m_noteId = noteId;
    m_title = title;
    m_message = message;
    m_toolTip->setTitle(Qt::convertFromPlainText(title));
    m_toolTip->setMessage({Qt::convertFromPlainText(message)});
    m_toolTip->showAbove(screenAnchor);
}

void NoteErrorToolTipController::hide() {
    m_noteId = -1;
    m_title.clear();
    m_message.clear();
    if (m_toolTip->isVisible())
        m_toolTip->hideWithAnimation();
}
