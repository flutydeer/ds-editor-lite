//
// Created by fluty on 24-2-18.
//

#include "WorkspaceEditor.h"

WorkspaceEditor::WorkspaceEditor(QJsonObject &globalWorkspace, const QString &key) {
    m_globalWorkspace = &globalWorkspace;
    m_key = key;
    if (m_globalWorkspace->contains(m_key))
        m_privateWorkspace = m_globalWorkspace->value(m_key).toObject();
}

QJsonObject *WorkspaceEditor::privateWorkspace() {
    return &m_privateWorkspace;
}

void WorkspaceEditor::commit() const {
    m_globalWorkspace->remove(m_key);
    m_globalWorkspace->insert(m_key, m_privateWorkspace);
}