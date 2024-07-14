//
// Created by fluty on 2024/2/7.
//

#ifndef ACTIONSEQUENCE_H
#define ACTIONSEQUENCE_H

#include <QObject>
#include <QList>

#include "IAction.h"

class ActionSequence : public QObject {
    Q_OBJECT

public:
    void execute();
    void undo();
    qsizetype count();
    QString name();

protected:
    QList<IAction *> m_actionSequence;
    void addAction(IAction *action);
    void setName(const QString &name);

private:
    QString m_name;
};



#endif //ACTIONSEQUENCE_H
