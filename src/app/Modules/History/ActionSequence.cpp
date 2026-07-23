//
// Created by fluty on 2024/2/7.
//

#include "ActionSequence.h"

#include <QCoreApplication>

ActionSequence::~ActionSequence() {
    for (const auto action : m_actions)
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

qsizetype ActionSequence::count() const {
    return m_actions.count();
}

QString ActionSequence::name() const {
    if (!m_nameSourceText.isEmpty())
        return QCoreApplication::translate(m_nameContext.constData(), m_nameSourceText.constData());
    return m_name;
}

quint64 ActionSequence::historyId() const {
    return m_historyId;
}

const std::optional<HistoryFocusTransition> &ActionSequence::focusTransition() const {
    return m_focusTransition;
}

void ActionSequence::addAction(IAction *action) {
    m_actions.append(action);
}

void ActionSequence::setName(const QString &name) {
    m_name = name;
    m_nameContext.clear();
    m_nameSourceText.clear();
}

void ActionSequence::setTranslatableName(const char *context, const char *sourceText) {
    m_name.clear();
    m_nameContext = context;
    m_nameSourceText = sourceText;
}

void ActionSequence::setFocusTransition(const HistoryFocusTransition &transition) {
    m_focusTransition = transition;
}

void ActionSequence::setHistoryId(const quint64 id) {
    m_historyId = id;
}
