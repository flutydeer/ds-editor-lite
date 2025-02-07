//
// Created by FlutyDeer on 25-2-7.
//

#ifndef TITLECONTROLGROUP_H
#define TITLECONTROLGROUP_H

#include <QWidget>

class TitleControlGroup : public QWidget {
    Q_OBJECT

public:
    explicit TitleControlGroup(QWidget *parent = nullptr);
};

inline TitleControlGroup::TitleControlGroup(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setContentsMargins({0,0,0,0});
}



#endif // TITLECONTROLGROUP_H
