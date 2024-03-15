//
// Created by fluty on 24-3-15.
//

#ifndef IOPTIONSCONTROLLER_H
#define IOPTIONSCONTROLLER_H

#include <QObject>

class IOptionsController : public QObject {
    Q_OBJECT

signals:
    void modified();

public:
    virtual void accept() = 0;
    virtual void cancel() = 0;
    virtual void update() = 0;
};
#endif // IOPTIONSCONTROLLER_H
