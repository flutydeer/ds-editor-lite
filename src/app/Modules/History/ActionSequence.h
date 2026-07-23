//
// Created by fluty on 2024/2/7.
//

#ifndef ACTIONSEQUENCE_H
#define ACTIONSEQUENCE_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <optional>

#include "IAction.h"
#include "HistoryFocus.h"

class HistoryManager;

class ActionSequence : public QObject, public IAction {
    Q_OBJECT

public:
    ~ActionSequence() override;
    void execute() override;
    void undo() override;
    qsizetype count() const;
    QString name() const;
    [[nodiscard]] quint64 historyId() const;
    [[nodiscard]] const std::optional<HistoryFocusTransition> &focusTransition() const;

protected:
    void addAction(IAction *action);
    void setName(const QString &name);
    void setTranslatableName(const char *context, const char *sourceText);
    void setFocusTransition(const HistoryFocusTransition &transition);

private:
    friend class HistoryManager;
    void setHistoryId(quint64 id);

    QList<IAction *> m_actions;
    QString m_name;
    QByteArray m_nameContext;
    QByteArray m_nameSourceText;
    quint64 m_historyId = 0;
    std::optional<HistoryFocusTransition> m_focusTransition;
};



#endif // ACTIONSEQUENCE_H
