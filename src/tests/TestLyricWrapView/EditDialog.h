#ifndef EDITDIALOG_H
#define EDITDIALOG_H

#include <QKeyEvent>
#include <QDialog>

namespace LyricWrap {
    class EditDialog final : public QDialog {
    public:
        explicit EditDialog(const QString &lyric, const QRectF &rect, QWidget *parent = nullptr);

        QString text;

    protected:
        bool event(QEvent *event) override;
    };
} // LyricWrap

#endif // EDITDIALOG_H
