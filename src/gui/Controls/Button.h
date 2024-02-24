//
// Created by fluty on 24-2-20.
//

#ifndef BUTTON_H
#define BUTTON_H

#include <QPushButton>

#include "Utils/AppGlobal.h"

class Button : public QPushButton {
    Q_OBJECT
public:
    explicit Button(QWidget *parent = nullptr, bool isPrimary = false);
    explicit Button(const QString &text, QWidget *parent = nullptr, bool isPrimary = false);
    Button(const QIcon &icon, const QString &text, QWidget *parent = nullptr, bool isPrimary = false);

    bool isPrimary() const;
    void setPrimary(bool on);

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool m_isPrimary;
    QColor m_backgroundColor = AppGlobal::controlBackgroundColorNormal;
    QColor m_foregroundColor = AppGlobal::controlForegroundColorNormal;
    // QPropertyAnimation m_backgroundColorAnimation;
    // QPropertyAnimation m_foregroundColorAnimation;
    bool m_hover = false;
    bool m_pressed = false;

    void initUi();
};

#endif // BUTTON_H
