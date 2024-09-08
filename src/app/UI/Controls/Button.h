//
// Created by fluty on 24-2-20.
//

#ifndef BUTTON_H
#define BUTTON_H

#include "UI/Utils/DropShadow.h"

#include <QGraphicsDropShadowEffect>
#include <QPushButton>

class Button : public QPushButton {
    Q_OBJECT
    DROP_SHADOW_EFFECT

public:
    explicit Button(QWidget *parent = nullptr);
    explicit Button(const QString &text, QWidget *parent = nullptr);
    Button(const QIcon &icon, const QString &text, QWidget *parent = nullptr);

private:
    void initUi();
    void mousePressEvent(QMouseEvent *event) override;
};

#endif // BUTTON_H
