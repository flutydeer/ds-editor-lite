//
// Created by fluty on 24-2-18.
//

#ifndef WORKSPACEEDITOR_H
#define WORKSPACEEDITOR_H

#include <QJsonObject>

class WorkspaceEditor {
public:
    explicit WorkspaceEditor(QJsonObject &globalWorkspace, const QString &key);
    QJsonObject *privateWorkspace();
    void commit() const;

private:
    QJsonObject *m_globalWorkspace;
    QJsonObject m_privateWorkspace;
    QString m_key;
};

#endif // WORKSPACEEDITOR_H
