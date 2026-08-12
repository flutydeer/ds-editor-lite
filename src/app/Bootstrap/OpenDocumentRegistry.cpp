#include "OpenDocumentRegistry.h"

#include "Controller/DocumentWorkflow/DocumentWorkflowPathUtils.h"

bool OpenDocumentRegistry::reserve(const void *owner, const QString &path) {
    if (!owner || path.isEmpty() || ownerForPath(path))
        return false;
    m_paths.insert(owner, DocumentWorkflowPathUtils::normalizedProjectPath(path));
    return true;
}

bool OpenDocumentRegistry::update(const void *owner, const QString &path) {
    if (!owner)
        return false;
    if (path.isEmpty()) {
        release(owner);
        return true;
    }

    const auto existingOwner = ownerForPath(path);
    if (existingOwner && existingOwner != owner)
        return false;
    m_paths.insert(owner, DocumentWorkflowPathUtils::normalizedProjectPath(path));
    return true;
}

void OpenDocumentRegistry::release(const void *owner) {
    m_paths.remove(owner);
}

const void *OpenDocumentRegistry::ownerForPath(const QString &path) const {
    if (path.isEmpty())
        return nullptr;
    const auto normalized = DocumentWorkflowPathUtils::normalizedProjectPath(path);
    for (auto it = m_paths.cbegin(); it != m_paths.cend(); ++it) {
        if (DocumentWorkflowPathUtils::projectPathsEqual(it.value(), normalized))
            return it.key();
    }
    return nullptr;
}

QString OpenDocumentRegistry::pathForOwner(const void *owner) const {
    return m_paths.value(owner);
}
