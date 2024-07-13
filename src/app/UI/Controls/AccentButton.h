//
// Created by fluty on 2024/7/13.
//

#ifndef ACCENTBUTTON_H
#define ACCENTBUTTON_H

#include "Button.h"

class AccentButton : public Button {
    Q_OBJECT

public:
    explicit AccentButton(QWidget *parent = nullptr) : Button(parent) {
    }
    explicit AccentButton(const QString &text, QWidget *parent = nullptr) : Button(text, parent) {
    }
    AccentButton(const QIcon &icon, const QString &text, QWidget *parent = nullptr)
        : Button(icon, text, parent) {
    }
};


#endif // ACCENTBUTTON_H
