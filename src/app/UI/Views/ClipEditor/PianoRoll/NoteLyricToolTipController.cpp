#include "NoteLyricToolTipController.h"

#include <lite/GUI/Controls/ToolTip.h>

#include <QTextDocument>

NoteLyricToolTipController::NoteLyricToolTipController(QWidget *parent)
    : m_toolTip(std::make_unique<ToolTip>(QString(), parent)) {
    m_toolTip->setAnimationEnabled(false);
}

NoteLyricToolTipController::~NoteLyricToolTipController() = default;

void NoteLyricToolTipController::showFor(
    const int noteId, const QString &lyric, const QRect &screenAnchor) {
    if (lyric.isEmpty() || screenAnchor.isEmpty()) {
        hide();
        return;
    }
    if (m_noteId == noteId && m_lyric == lyric && m_toolTip->isVisible())
        return;

    m_noteId = noteId;
    m_lyric = lyric;
    m_toolTip->setTitle(Qt::convertFromPlainText(lyric));
    m_toolTip->showAbove(screenAnchor);
}

void NoteLyricToolTipController::hide() {
    m_noteId = -1;
    m_lyric.clear();
    if (m_toolTip->isVisible())
        m_toolTip->hideWithAnimation();
}
