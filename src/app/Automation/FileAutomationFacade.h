#ifndef FILEAUTOMATIONFACADE_H
#define FILEAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "ProjectAutomationDtos.h"

#include <QByteArray>

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
        QJsonObject optionSchema;

        friend bool operator==(const ProjectFormatDto &, const ProjectFormatDto &) = default;
    };

    struct FileWriteResultDto {
        QString path;
        bool wroteFile = false;
        bool validatedOnly = false;

        friend bool operator==(const FileWriteResultDto &, const FileWriteResultDto &) = default;
    };

    struct MidiExportOptionsDto {
        bool includeTempo = true;
        bool includeTimeSignatures = true;
        bool includeLyrics = true;
        QList<TrackId> trackIds;
        QList<ClipId> clipIds;

        friend bool operator==(const MidiExportOptionsDto &,
                               const MidiExportOptionsDto &) = default;
    };

    struct PreparedMidiExportDto {
        DocumentVersion document;
        QString path;
        bool allowOverwrite = false;
        MidiExportOptionsDto options;
        DocumentDraftDto modelSnapshot;
        bool validatedOnly = false;
    };

    struct FileRuntimeServices {
        std::function<QList<ProjectFormatDto>()> listProjectFormats;
        std::function<AutomationResult<QByteArray>(const QString &)> convertLibreSvipToDspx;
        std::function<bool(AppModel *, const QString &, const MidiExportOptionsDto &, QString &)>
            exportMidi;
    };

    class FileAutomationFacade final {
    public:
        FileAutomationFacade(OperationCatalog &catalog, AutomationDispatcher &dispatcher,
                             FileRuntimeServices services = {});

        AutomationResult<QList<ProjectFormatDto>> listFormats();
        AutomationResult<QByteArray> convertLibreSvipToDspx(const QString &path);
        AutomationResult<FileWriteResultDto> exportMidi(const CommandContext &context,
                                                        const QString &path, bool allowOverwrite);
        AutomationResult<FileWriteResultDto> exportMidi(const CommandContext &context,
                                                        const QString &path, bool allowOverwrite,
                                                        MidiExportOptionsDto options);
        AutomationResult<PreparedMidiExportDto>
            previewMidiExport(const DocumentId &documentId, const QString &path,
                              MidiExportOptionsDto options = {});
        AutomationResult<PreparedMidiExportDto>
            prepareMidiExport(const CommandContext &context, const QString &path,
                              bool allowOverwrite, MidiExportOptionsDto options = {});
        AutomationResult<FileWriteResultDto>
            writePreparedMidiExport(const PreparedMidiExportDto &prepared) const;

    private:
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        FileRuntimeServices m_services;
    };

} // namespace Automation

#endif // FILEAUTOMATIONFACADE_H
