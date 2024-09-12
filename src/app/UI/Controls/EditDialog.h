#ifndef EDITDIALOG_H
#define EDITDIALOG_H

#include <QDialog>
#include <QKeyEvent>

class EditDialog final : public QDialog {
public:
    explicit EditDialog(const QString &lyric, const QRectF &rect, const QFont &font,
                        QWidget *parent = nullptr);

    QString text;

protected:
    bool event(QEvent *event) override;
};


#endif // EDITDIALOG_H
