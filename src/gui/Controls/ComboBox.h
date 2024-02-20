//
// Created by fluty on 24-2-20.
//

#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <QComboBox>

class ComboBox : public QComboBox {
Q_OBJECT
public:
    explicit ComboBox(QWidget *parent = nullptr);
    explicit ComboBox(bool scrollWheelChangeSelection, QWidget *parent = nullptr);

private:
    void wheelEvent(QWheelEvent *event) override;
    bool m_scrollWheelChangeSelection = false;

    void initUi();
};



#endif //COMBOBOX_H
