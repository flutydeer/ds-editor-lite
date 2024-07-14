//
// Created by fluty on 2024/2/7.
//

#include "ActionSequence.h"
void ActionSequence::execute() {
    for (const auto action : m_actionSequence)
        action->execute();
}
void ActionSequence::undo() {
    for (qsizetype i = m_actionSequence.count() - 1; i >= 0; i--)
        m_actionSequence[i]->undo();
}
qsizetype ActionSequence::count() {
    return m_actionSequence.count();
}
QString ActionSequence::name() {
    return m_name;
}
void ActionSequence::addAction(IAction *action) {
    m_actionSequence.append(action);
}
void ActionSequence::setName(const QString &name) {
    m_name = name;
}