//
// Created by fluty on 2024/2/7.
//

#ifndef ACTIONSEQUENCE_H
#define ACTIONSEQUENCE_H

#include <QByteArray>
#include <QList>
#include <QObject>

#include "IAction.h"

class ActionSequence : public QObject, public IAction {
    Q_OBJECT

public:
    ~ActionSequence() override;
    void execute() override;
    void undo() override;
    qsizetype count() const;
    QString name() const;

protected:
    void addAction(IAction *action);
    void setName(const QString &name);
    void setTranslatableName(const char *context, const char *sourceText);

private:
    QList<IAction *> m_actions;
    QString m_name;
    QByteArray m_nameContext;
    QByteArray m_nameSourceText;
};



#endif // ACTIONSEQUENCE_H
