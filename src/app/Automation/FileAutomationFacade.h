#ifndef FILEAUTOMATIONFACADE_H
#define FILEAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"

#include <functional>

class AppModel;

namespace Automation {

    struct ProjectFormatDto {
        QString id;
        QString displayName;
        QStringList extensions;
        bool canOpen = false;
        bool canImport = false;
        bool canExport = false;
        bool available = true;
        QString unavailableReason;

        friend bool operator==(const ProjectFormatDto &, const ProjectFormatDto &) = default;
    };

    struct FileWriteResultDto {
        QString path;
        bool wroteFile = false;
        bool validatedOnly = false;

        friend bool operator==(const FileWriteResultDto &, const FileWriteResultDto &) = default;
    };

    struct FileRuntimeServices {
        std::function<QList<ProjectFormatDto>()> listProjectFormats;
        std::function<bool(AppModel *, const QString &, QString &)> exportMidi;
    };

    class FileAutomationFacade final {
    public:
        FileAutomationFacade(OperationCatalog &catalog,
                             AutomationDispatcher &dispatcher,
                             FileRuntimeServices services = {});

        AutomationResult<QList<ProjectFormatDto>> listFormats();
        AutomationResult<FileWriteResultDto> exportMidi(const CommandContext &context,
                                                        const QString &path,
                                                        bool allowOverwrite);

    private:
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        FileRuntimeServices m_services;
    };

} // namespace Automation

#endif // FILEAUTOMATIONFACADE_H
