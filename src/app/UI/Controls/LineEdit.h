//
// Created by fluty on 24-2-20.
//

#ifndef LINEEDIT_H
#define LINEEDIT_H

#include <QLineEdit>

class Menu;

class LineEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit LineEdit(QWidget *parent = nullptr);
    explicit LineEdit(const QString &text, QWidget *parent = nullptr);

    [[nodiscard]] Menu *createContextMenu(QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
};



#endif // LINEEDIT_H
