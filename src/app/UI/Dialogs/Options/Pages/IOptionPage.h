//
// Created by fluty on 24-3-18.
//

#ifndef OPTIONPAGE_H
#define OPTIONPAGE_H

#include <QWidget>

class IOptionPage : public QWidget {
    Q_OBJECT

public:
    explicit IOptionPage(QWidget *parent = nullptr) : QWidget(parent) {
    }

protected:
    virtual void modifyOption() = 0;
};
#endif // OPTIONPAGE_H
