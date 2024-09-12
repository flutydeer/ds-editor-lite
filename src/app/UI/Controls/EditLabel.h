//
// Created by fluty on 2023/8/13.
//

#ifndef DATASET_TOOLS_EDITLABEL_H
#define DATASET_TOOLS_EDITLABEL_H

#include <QStackedWidget>

class QLabel;
class LineEdit;

class EditLabel : public QStackedWidget {
    Q_OBJECT

public:
    explicit EditLabel(QWidget *parent = nullptr);

    QLabel *label;

    QString text() const;
    void setText(const QString &text);

signals:
    void editCompleted(const QString &text);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event) override;
};

#endif // DATASET_TOOLS_EDITLABEL_H
