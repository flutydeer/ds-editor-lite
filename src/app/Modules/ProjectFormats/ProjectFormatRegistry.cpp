#include "ProjectFormatRegistry.h"

#include "DspxFormatHandler.h"
#include "IProjectFormatHandler.h"
#include "MidiFormatHandler.h"

#include <QFileInfo>

ProjectFormatRegistry::ProjectFormatRegistry(QObject *parent) : QObject(parent) {
    // Built-in formats are registered here; future formats can be added via
    // registerHandler() at startup.
    registerHandler(std::make_unique<MidiFormatHandler>());
    registerHandler(std::make_unique<DspxFormatHandler>());
}

ProjectFormatRegistry::~ProjectFormatRegistry() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(ProjectFormatRegistry)

void ProjectFormatRegistry::registerHandler(std::unique_ptr<IProjectFormatHandler> handler) {
    m_handlers.push_back(std::move(handler));
}

IProjectFormatHandler *ProjectFormatRegistry::resolveByPath(const QString &filePath) const {
    const auto suffix = QFileInfo(filePath).suffix().toLower();
    if (suffix.isEmpty())
        return nullptr;
    for (const auto &handler : m_handlers) {
        if (handler->descriptor().extensions.contains(suffix))
            return handler.get();
    }
    return nullptr;
}

QList<IProjectFormatHandler *> ProjectFormatRegistry::handlers() const {
    QList<IProjectFormatHandler *> result;
    result.reserve(static_cast<int>(m_handlers.size()));
    for (const auto &handler : m_handlers)
        result.append(handler.get());
    return result;
}
