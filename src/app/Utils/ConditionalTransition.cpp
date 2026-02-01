//
// Created by FlutyDeer on 2026/1/24.
//

#include "ConditionalTransition.h"

ConditionalTransition::ConditionalTransition(QObject *sender, const char *signal)
    : QSignalTransition(sender, signal) {
}

ConditionalTransition::ConditionalTransition(QObject *sender, const char *signal,
                                             std::function<bool()> condition)
    : QSignalTransition(sender, signal), m_guardCondition(std::move(condition)) {
}

void ConditionalTransition::setGuardCondition(std::function<bool()> condition) {
    m_guardCondition = std::move(condition);
}

bool ConditionalTransition::eventTest(QEvent *e) {
    if (!QSignalTransition::eventTest(e))
        return false;

    if (m_guardCondition)
        return m_guardCondition();

    return true;
}