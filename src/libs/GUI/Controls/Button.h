#ifndef BUTTON_H
#define BUTTON_H

#include <lite/GUI/Controls/WheelEventPolicy.h>

#include <QPushButton>

class Button : public QPushButton, public WheelEventPolicySupport {
    Q_OBJECT

public:
    explicit Button(QWidget *parent = nullptr);
    explicit Button(const QString &text, QWidget *parent = nullptr);
    Button(const QIcon &icon, const QString &text, QWidget *parent = nullptr);

private:
    void initUi();
    void mousePressEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
};

#endif // BUTTON_H
