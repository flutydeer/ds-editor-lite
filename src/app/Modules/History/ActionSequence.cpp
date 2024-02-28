//
// Created by fluty on 2024/2/7.
//

#include "ActionSequence.h"
void ActionSequence::execute() {
    for (const auto action : m_actionSequence)
        action->execute();
}
void ActionSequence::undo() {
    for (int i = m_actionSequence.count() - 1; i >= 0; i--)
        m_actionSequence[i]->undo();
}
int ActionSequence::count() {
    return m_actionSequence.count();
}
void ActionSequence::addAction(IAction *action) {
    m_actionSequence.append(action);
}