#ifndef NOTEERRORTOOLTIPCONTROLLER_H
#define NOTEERRORTOOLTIPCONTROLLER_H

#include <QRect>
#include <QString>

#include <memory>

class ToolTip;
class QWidget;

// Tooltip explaining why a note is excluded from inference. Anchored above the
// note like the lyric tooltip and deduplicated per note
class NoteErrorToolTipController final {
public:
    explicit NoteErrorToolTipController(QWidget *parent);
    ~NoteErrorToolTipController();

    void showFor(int noteId, const QString &lyric, const QString &message,
                 const QRect &screenAnchor);
    void hide();

private:
    std::unique_ptr<ToolTip> m_toolTip;
    int m_noteId = -1;
    QString m_lyric;
    QString m_message;
};

#endif // NOTEERRORTOOLTIPCONTROLLER_H
