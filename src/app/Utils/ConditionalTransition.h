//
// Created by FlutyDeer on 2026/1/24.
//

#ifndef DS_EDITOR_LITE_CONDITIONALTRANSITION_H
#define DS_EDITOR_LITE_CONDITIONALTRANSITION_H

#include <QSignalTransition>

#include <functional>

class ConditionalTransition : public QSignalTransition {
public:
    explicit ConditionalTransition(QObject *sender, const char *signal);
    explicit ConditionalTransition(QObject *sender, const char *signal,
                                   std::function<bool()> condition);
    void setGuardCondition(std::function<bool()> condition);

protected:
    bool eventTest(QEvent *e) override;

private:
    std::function<bool()> m_guardCondition;
};

#endif // DS_EDITOR_LITE_CONDITIONALTRANSITION_H
