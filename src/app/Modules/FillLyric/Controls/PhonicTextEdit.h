#ifndef LYRIC_TAB_CONTROLS_PHONIC_TEXT_EDIT_H
#define LYRIC_TAB_CONTROLS_PHONIC_TEXT_EDIT_H

#include <QPlainTextEdit>

namespace FillLyric
{
    class PhonicTextEdit final : public QPlainTextEdit {
        Q_OBJECT
    public:
        explicit PhonicTextEdit(QWidget *parent = nullptr);

    Q_SIGNALS:
        void fontChanged();

    protected:
        void wheelEvent(QWheelEvent *event) override;
        void contextMenuEvent(QContextMenuEvent *event) override;
    };
} // namespace FillLyric

#endif // LYRIC_TAB_CONTROLS_PHONIC_TEXT_EDIT_H
