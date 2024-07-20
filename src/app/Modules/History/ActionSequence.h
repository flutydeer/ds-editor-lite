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
    ~ActionSequence() override;
    void execute();
    void undo();
    qsizetype count();
    QString name();

protected:
    void addAction(IAction *action);
    void setName(const QString &name);

private:
    QList<IAction *> m_actions;
    QString m_name;
};



#endif //ACTIONSEQUENCE_H
