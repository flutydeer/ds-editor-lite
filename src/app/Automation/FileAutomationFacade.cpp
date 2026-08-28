#include "FileAutomationFacade.h"
#include "OperationIds.h"

#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QDir>
#include <QFileInfo>
#include <QSet>

namespace Automation {
    namespace {
        AutomationError unavailable() {
            AutomationError error;
            error.code = AutomationErrorCode::ModuleNotReady;
            error.message = QStringLiteral("File services are unavailable");
            return error;
        }

        AutomationResult<QString> validateMidiPath(const QString &path, const bool allowOverwrite) {
            if (path.trimmed().isEmpty()) {
                AutomationError error;
                error.code = AutomationErrorCode::PathRequired;
                error.fieldPath = QStringLiteral("path");
                error.message = QStringLiteral("MIDI export path is required");
                return error;
            }

            const QFileInfo fileInfo(path);
            if (!fileInfo.isAbsolute()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("path"), QStringLiteral("MIDI export path must be absolute"));
            }
            const auto suffix = fileInfo.suffix().toLower();
            if (suffix != QStringLiteral("mid") && suffix != QStringLiteral("midi")) {
                AutomationError error;
                error.code = AutomationErrorCode::FormatUnsupported;
                error.fieldPath = QStringLiteral("path");
                error.message = QStringLiteral("MIDI export path must use .mid or .midi");
                return error;
            }
            if (!fileInfo.dir().exists()) {
                AutomationError error;
                error.code = AutomationErrorCode::FileNotFound;
                error.fieldPath = QStringLiteral("path");
                error.message = QStringLiteral("MIDI export directory does not exist");
                return error;
            }
            if (fileInfo.exists() && !fileInfo.isFile()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("path"), QStringLiteral("MIDI export target is not a file"));
            }
            if (fileInfo.exists() && !allowOverwrite) {
                AutomationError error;
                error.code = AutomationErrorCode::OverwriteDenied;
                error.fieldPath = QStringLiteral("allow_overwrite");
                error.message = QStringLiteral("MIDI export target already exists");
                return error;
            }
            return QDir::cleanPath(fileInfo.absoluteFilePath());
        }

        DocumentDraftDto snapshotDocument(const AppModel &model,
                                          const MidiExportOptionsDto &options) {
            DocumentDraftDto result;
            result.timeline = model.timeline();
            result.masterControl = model.masterControl();
            result.tracks.reserve(model.tracks().size());
            QSet<int> selectedTracks;
            for (const auto id : options.trackIds)
                selectedTracks.insert(id.value());
            QSet<int> selectedClips;
            for (const auto id : options.clipIds)
                selectedClips.insert(id.value());
            const auto selectAll = selectedTracks.isEmpty() && selectedClips.isEmpty();
            for (const auto *track : model.tracks()) {
                if (!track)
                    continue;
                if (selectAll || selectedTracks.contains(track->id())) {
                    result.tracks.append(trackDraftDto(*track));
                    continue;
                }
                TrackDraftDto draft = trackDraftDto(*track);
                draft.clips.clear();
                for (const auto *clip : track->clips()) {
                    if (clip && selectedClips.contains(clip->id()))
                        draft.clips.append(clipDraftDto(*clip));
                }
                if (!draft.clips.isEmpty())
                    result.tracks.append(std::move(draft));
            }
            return result;
        }
    }

    FileAutomationFacade::FileAutomationFacade(AutomationDispatcher &dispatcher,
                                               FileRuntimeServices services)
        : m_dispatcher(dispatcher), m_services(std::move(services)) {
    }

    AutomationResult<QList<ProjectFormatDto>> FileAutomationFacade::listFormats() {
        return m_dispatcher.dispatchApplicationQuery<QList<ProjectFormatDto>>(
            OperationIds::formats::list, [this] {
                if (!m_services.listProjectFormats)
                    return AutomationResult<QList<ProjectFormatDto>>(unavailable());
                return AutomationResult<QList<ProjectFormatDto>>(m_services.listProjectFormats());
            });
    }

    AutomationResult<QByteArray> FileAutomationFacade::convertLibreSvipToDspx(const QString &path) {
        if (!m_services.convertLibreSvipToDspx)
            return unavailable();
        return m_services.convertLibreSvipToDspx(path);
    }

    AutomationResult<FileWriteResultDto>
        FileAutomationFacade::exportMidi(const CommandContext &context, const QString &path,
                                         const bool allowOverwrite) {
        return exportMidi(context, path, allowOverwrite, {});
    }

    AutomationResult<FileWriteResultDto>
        FileAutomationFacade::exportMidi(const CommandContext &context, const QString &path,
                                         const bool allowOverwrite, MidiExportOptionsDto options) {
        return m_dispatcher.dispatchDocumentCommandResult<FileWriteResultDto>(
            OperationIds::exports::midi::start, context,
            [this, path, allowOverwrite, options = std::move(options)](DocumentSession &session,
                                                                       const bool validateOnly) {
                const auto validatedPath = validateMidiPath(path, allowOverwrite);
                if (!validatedPath)
                    return AutomationResult<FileWriteResultDto>(validatedPath.getError());
                if (!m_services.exportMidi)
                    return AutomationResult<FileWriteResultDto>(unavailable());
                if (validateOnly) {
                    return AutomationResult<FileWriteResultDto>({
                        .path = validatedPath.get(),
                        .validatedOnly = true,
                    });
                }

                QString errorMessage;
                if (!m_services.exportMidi(session.model(), validatedPath.get(), options,
                                           errorMessage)) {
                    AutomationError error;
                    error.code = AutomationErrorCode::IoError;
                    error.message = errorMessage.isEmpty() ? QStringLiteral("MIDI export failed")
                                                           : std::move(errorMessage);
                    return AutomationResult<FileWriteResultDto>(std::move(error));
                }
                return AutomationResult<FileWriteResultDto>({
                    .path = validatedPath.get(),
                    .wroteFile = true,
                });
            });
    }

    AutomationResult<PreparedMidiExportDto>
        FileAutomationFacade::prepareMidiExport(const CommandContext &context, const QString &path,
                                                const bool allowOverwrite,
                                                MidiExportOptionsDto options) {
        return m_dispatcher.dispatchDocumentCommandResult<PreparedMidiExportDto>(
            OperationIds::exports::midi::start, context,
            [this, path, allowOverwrite, options = std::move(options)](DocumentSession &session,
                                                                       const bool validateOnly) {
                const auto validatedPath = validateMidiPath(path, allowOverwrite);
                if (!validatedPath)
                    return AutomationResult<PreparedMidiExportDto>(validatedPath.getError());
                if (!m_services.exportMidi)
                    return AutomationResult<PreparedMidiExportDto>(unavailable());
                return AutomationResult<PreparedMidiExportDto>({
                    .document = session.version(),
                    .path = validatedPath.get(),
                    .allowOverwrite = allowOverwrite,
                    .options = options,
                    .modelSnapshot = validateOnly ? DocumentDraftDto{}
                                                  : snapshotDocument(*session.model(), options),
                    .validatedOnly = validateOnly,
                });
            });
    }

    AutomationResult<PreparedMidiExportDto>
        FileAutomationFacade::previewMidiExport(const DocumentId &documentId, const QString &path,
                                                MidiExportOptionsDto options) {
        return m_dispatcher.dispatchDocumentQuery<PreparedMidiExportDto>(
            OperationIds::exports::midi::preview, documentId,
            [this, path, options = std::move(options)](DocumentSession &session) {
                const auto validatedPath = validateMidiPath(path, true);
                if (!validatedPath)
                    return AutomationResult<PreparedMidiExportDto>(validatedPath.getError());
                if (!m_services.exportMidi)
                    return AutomationResult<PreparedMidiExportDto>(unavailable());
                return AutomationResult<PreparedMidiExportDto>({
                    .document = session.version(),
                    .path = validatedPath.get(),
                    .allowOverwrite = true,
                    .options = options,
                    .validatedOnly = true,
                });
            });
    }

    AutomationResult<FileWriteResultDto>
        FileAutomationFacade::writePreparedMidiExport(const PreparedMidiExportDto &prepared) const {
        if (prepared.validatedOnly) {
            return FileWriteResultDto{
                .path = prepared.path,
                .validatedOnly = true,
            };
        }
        const auto validatedPath = validateMidiPath(prepared.path, prepared.allowOverwrite);
        if (!validatedPath)
            return validatedPath.getError();
        if (!m_services.exportMidi)
            return unavailable();

        AppModel snapshot;
        snapshot.replaceProject(buildProjectModelData(prepared.modelSnapshot));
        QString errorMessage;
        if (!m_services.exportMidi(&snapshot, validatedPath.get(), prepared.options,
                                   errorMessage)) {
            AutomationError error;
            error.code = AutomationErrorCode::IoError;
            error.message = errorMessage.isEmpty() ? QStringLiteral("MIDI export failed")
                                                   : std::move(errorMessage);
            return error;
        }
        return FileWriteResultDto{
            .path = validatedPath.get(),
            .wroteFile = true,
        };
    }

} // namespace Automation
