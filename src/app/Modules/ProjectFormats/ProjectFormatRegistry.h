#ifndef DS_EDITOR_LITE_PROJECTFORMATREGISTRY_H
#define DS_EDITOR_LITE_PROJECTFORMATREGISTRY_H

#define projectFormatRegistry ProjectFormatRegistry::instance()

#include <lite/Core/Singleton.h>

#include <QObject>
#include <QString>

#include <memory>
#include <vector>

class IProjectFormatHandler;

// Owns the registered project format handlers and resolves a file path to the
// handler that claims it. Built-in formats are registered in the constructor;
// additional formats can register at startup via registerHandler().
class ProjectFormatRegistry final : public QObject {
    Q_OBJECT

private:
    explicit ProjectFormatRegistry(QObject *parent = nullptr);
    ~ProjectFormatRegistry() override;

public:
    LITE_SINGLETON_DECLARE_INSTANCE(ProjectFormatRegistry)
    Q_DISABLE_COPY_MOVE(ProjectFormatRegistry)

    void registerHandler(std::unique_ptr<IProjectFormatHandler> handler);

    // Extension-based lookup (lower-cased suffix, no leading dot). Returns the
    // first registered handler whose descriptor lists the extension, or
    // nullptr when no handler claims the file.
    [[nodiscard]] IProjectFormatHandler *resolveByPath(const QString &filePath) const;
    [[nodiscard]] QList<IProjectFormatHandler *> handlers() const;

private:
    std::vector<std::unique_ptr<IProjectFormatHandler>> m_handlers;
};

#endif // DS_EDITOR_LITE_PROJECTFORMATREGISTRY_H
