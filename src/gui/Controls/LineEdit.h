//
// Created by fluty on 24-2-20.
//

#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <QLineEdit>

class LineEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit LineEdit(QWidget *parent = nullptr);
    explicit LineEdit(const QString &text, QWidget *parent = nullptr);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
};



#endif // LINEEDIT_H
