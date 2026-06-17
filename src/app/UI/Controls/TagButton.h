#ifndef TAGBUTTON_H
#define TAGBUTTON_H

#include <QPushButton>

class TagButton : public QPushButton {
    Q_OBJECT

public:
    explicit TagButton(QWidget *parent = nullptr);
    explicit TagButton(const QString &text, QWidget *parent = nullptr);
};

#endif // TAGBUTTON_H
