//
// Created by fluty on 2024/2/7.
//

#ifndef ACTIONSEQUENCE_H
#define ACTIONSEQUENCE_H

#include <QList>

#include "IAction.h"

class ActionSequence {
public:
    void execute();
    void undo();
    int count();

protected:
    QList<IAction *> m_actionSequence;
    void addAction(IAction *action);
};



#endif //ACTIONSEQUENCE_H
