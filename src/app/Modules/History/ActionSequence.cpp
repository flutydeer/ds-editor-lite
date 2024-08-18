//
// Created by fluty on 2024/2/7.
//

#include "ActionSequence.h"

ActionSequence::~ActionSequence() {
    for (auto action : m_actions)
        delete action;
}

void ActionSequence::execute() {
    for (const auto action : m_actions)
        action->execute();
}

void ActionSequence::undo() {
    for (qsizetype i = m_actions.count() - 1; i >= 0; i--)
        m_actions[i]->undo();
}

qsizetype ActionSequence::count() {
    return m_actions.count();
}

QString ActionSequence::name() {
    return m_name;
}

void ActionSequence::addAction(IAction *action) {
    m_actions.append(action);
}

void ActionSequence::setName(const QString &name) {
    m_name = name;
}