#ifndef OPENDOCUMENTREGISTRY_H
#define OPENDOCUMENTREGISTRY_H

#include <QHash>
#include <QString>

class OpenDocumentRegistry final {
public:
    bool reserve(const void *owner, const QString &path);
    bool update(const void *owner, const QString &path);
    void release(const void *owner);

    [[nodiscard]] const void *ownerForPath(const QString &path) const;
    [[nodiscard]] QString pathForOwner(const void *owner) const;

private:
    QHash<const void *, QString> m_paths;
};

#endif // OPENDOCUMENTREGISTRY_H
