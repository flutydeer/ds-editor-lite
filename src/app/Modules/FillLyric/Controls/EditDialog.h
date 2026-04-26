#ifndef LYRIC_TAB_CONTROLS_EDIT_DIALOG_H
#define LYRIC_TAB_CONTROLS_EDIT_DIALOG_H

#include <QDialog>
#include <QKeyEvent>

class QLineEdit;

namespace FillLyric
{
    class EditDialog final : public QDialog {
    public:
        explicit EditDialog(const QString &lyric, const QRectF &rect, const QFont &font, QWidget *parent = nullptr);

        QString text() const;

    protected:
        bool event(QEvent *event) override;

    private:
        QLineEdit *m_lineEdit;
    };
} // namespace FillLyric

#endif // LYRIC_TAB_CONTROLS_EDIT_DIALOG_H
