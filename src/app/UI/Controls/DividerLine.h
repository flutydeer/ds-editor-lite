//
// Created by fluty on 24-3-18.
//

#ifndef DIVIDERLINE_H
#define DIVIDERLINE_H

#include <QFrame>

class DividerLine : public QFrame {
    Q_OBJECT

public:
    explicit DividerLine(QWidget *parent = nullptr);
    explicit DividerLine(Qt::Orientation orientation, QWidget *parent = nullptr);

    void setOrientation(Qt::Orientation orientation);

private:
    Qt::Orientation m_orientation = Qt::Horizontal;
    void initUi();
};



#endif // DIVIDERLINE_H
