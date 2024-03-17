//
// Created by fluty on 24-2-20.
//

#ifndef BUTTON_H
#define BUTTON_H

#include <QPushButton>

#include "Global/AppGlobal.h"

class Button : public QPushButton {
    Q_OBJECT
public:
    explicit Button(QWidget *parent = nullptr);
    explicit Button(const QString &text, QWidget *parent = nullptr);
    Button(const QIcon &icon, const QString &text, QWidget *parent = nullptr);

private:
    void initUi();
};

#endif // BUTTON_H
