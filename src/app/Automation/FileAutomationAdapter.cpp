#include "FileAutomationAdapter.h"

#include "Modules/ProjectFormats/IProjectFormatHandler.h"
#include "Modules/ProjectFormats/LibreSVIPFormatHandler.h"
#include "Modules/ProjectFormats/ProjectFormatRegistry.h"

#include <lite/ProjectConverters/MidiConverter.h>

namespace Automation {

    FileRuntimeServices createFileAutomationServices() {
        FileRuntimeServices services;
        services.listProjectFormats = [] {
            QList<ProjectFormatDto> result;
            for (const auto *handler : projectFormatRegistry->handlers()) {
                const auto descriptor = handler->descriptor();
                const auto *libreSvipHandler =
                    dynamic_cast<const LibreSVIPFormatHandler *>(handler);
                const auto available =
                    !libreSvipHandler || !LibreSVIPFormatHandler::executablePath().isEmpty();
                result.append({
                    .id = descriptor.id,
                    .displayName = descriptor.displayName,
                    .extensions = descriptor.extensions,
                    .canOpen = descriptor.canOpen,
                    .canImport = descriptor.canImport,
                    .canExport = descriptor.canExport,
                    .available = available,
                    .unavailableReason =
                        available ? QString()
                                  : QStringLiteral("libresvip-cli executable was not found"),
                });
            }
            return result;
        };
        services.exportMidi = [](AppModel *model, const QString &path, QString &errorMessage) {
            if (!model) {
                errorMessage = QStringLiteral("AppModel is unavailable");
                return false;
            }
            MidiConverter converter;
            return converter.save(path, model, errorMessage);
        };
        return services;
    }

} // namespace Automation
