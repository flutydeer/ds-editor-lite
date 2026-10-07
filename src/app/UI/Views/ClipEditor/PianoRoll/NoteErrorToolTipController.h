#ifndef NOTEERRORTOOLTIPCONTROLLER_H
#define NOTEERRORTOOLTIPCONTROLLER_H

#include <QRect>
#include <QString>
#include <QTimer>

#include <memory>

class ToolTip;
class QWidget;

// Tooltip explaining why a note is excluded from inference. The title carries
// the error category, the body the specific one-line explanation. Anchored
// above the error badge the pointer rests on and deduplicated per note. Hover
// requests go through the style's tooltip wake-up delay and the fade in and
// out of the shared ToolTip, while a tap on the badge shows immediately
class NoteErrorToolTipController final {
public:
    explicit NoteErrorToolTipController(QWidget *parent);
    ~NoteErrorToolTipController();

    // Show without delay: an explicit tap on the badge is intent, not hover
    void showFor(int noteId, const QString &title, const QString &message,
                 const QRect &screenAnchor);
    // Schedule a show after the style's tooltip wake-up delay, or the shorter
    // fall-asleep delay while a card is already on screen. Repeated calls for
    // the same badge must not restart the countdown, or a resting pointer
    // would never raise the card
    void hoverFor(int noteId, const QString &title, const QString &message,
                  const QRect &screenAnchor);
    void hide();

private:
    void showPending();

    std::unique_ptr<ToolTip> m_toolTip;
    QTimer m_hoverTimer;
    int m_noteId = -1;
    QString m_title;
    QString m_message;
    int m_pendingNoteId = -1;
    QString m_pendingTitle;
    QString m_pendingMessage;
    QRect m_pendingAnchor;
};

#endif // NOTEERRORTOOLTIPCONTROLLER_H
