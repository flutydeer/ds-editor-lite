#ifndef NOTELYRICTOOLTIPCONTROLLER_H
#define NOTELYRICTOOLTIPCONTROLLER_H

#include <QRect>
#include <QString>

#include <memory>

class ToolTip;
class QWidget;

class NoteLyricToolTipController final {
public:
    explicit NoteLyricToolTipController(QWidget *parent);
    ~NoteLyricToolTipController();

    void showFor(int noteId, const QString &lyric, const QRect &screenAnchor);
    void hide();

private:
    std::unique_ptr<ToolTip> m_toolTip;
    int m_noteId = -1;
    QString m_lyric;
};

#endif // NOTELYRICTOOLTIPCONTROLLER_H
