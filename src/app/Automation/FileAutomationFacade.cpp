#include "FileAutomationFacade.h"
#include "OperationIds.h"

#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QDataStream>
#include <QDir>
#include <QFileInfo>

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

        QByteArray exportFingerprint(const QString &path, const bool allowOverwrite,
                                     const MidiExportOptionsDto &options) {
            QByteArray result;
            QDataStream stream(&result, QIODevice::WriteOnly);
            stream << path << allowOverwrite << options.includeTempo
                   << options.includeTimeSignatures;
            return result;
        }

        DocumentDraftDto snapshotDocument(const AppModel &model) {
            DocumentDraftDto result;
            result.timeline = model.timeline();
            result.masterControl = model.masterControl();
            result.tracks.reserve(model.tracks().size());
            for (const auto *track : model.tracks()) {
                if (track)
                    result.tracks.append(trackDraftDto(*track));
            }
            return result;
        }
    }

    FileAutomationFacade::FileAutomationFacade(OperationCatalog &catalog,
                                               AutomationDispatcher &dispatcher,
                                               FileRuntimeServices services)
        : m_catalog(catalog), m_dispatcher(dispatcher), m_services(std::move(services)) {
        registerOperations();
    }

    AutomationResult<QList<ProjectFormatDto>> FileAutomationFacade::listFormats() {
        return m_dispatcher.dispatchApplicationQuery<QList<ProjectFormatDto>>(
            OperationIds::formats::list, [this] {
                if (!m_services.listProjectFormats)
                    return AutomationResult<QList<ProjectFormatDto>>(unavailable());
                return AutomationResult<QList<ProjectFormatDto>>(m_services.listProjectFormats());
            });
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
            exportFingerprint(path, allowOverwrite, options),
            [this, path, allowOverwrite,
             options = std::move(options)](DocumentSession &session, const bool validateOnly) {
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
                    error.message = errorMessage.isEmpty()
                                        ? QStringLiteral("MIDI export failed")
                                        : std::move(errorMessage);
                    return AutomationResult<FileWriteResultDto>(std::move(error));
                }
                return AutomationResult<FileWriteResultDto>({
                    .path = validatedPath.get(),
                    .wroteFile = true,
                });
            });
    }

    AutomationResult<PreparedMidiExportDto> FileAutomationFacade::prepareMidiExport(
        const CommandContext &context, const QString &path, const bool allowOverwrite,
        MidiExportOptionsDto options) {
        return m_dispatcher.dispatchDocumentCommandResult<PreparedMidiExportDto>(
            OperationIds::exports::midi::start, context,
            exportFingerprint(path, allowOverwrite, options),
            [this, path, allowOverwrite,
             options = std::move(options)](DocumentSession &session, const bool validateOnly) {
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
                                                   : snapshotDocument(*session.model()),
                    .validatedOnly = validateOnly,
                });
            });
    }

    AutomationResult<FileWriteResultDto> FileAutomationFacade::writePreparedMidiExport(
        const PreparedMidiExportDto &prepared) const {
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
        if (!m_services.exportMidi(&snapshot, validatedPath.get(), prepared.options, errorMessage)) {
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

    void FileAutomationFacade::registerOperations() {
        auto result = m_catalog.add({
            .id = OperationIds::formats::list,
            .category = QStringLiteral("files"),
            .kind = OperationKind::Query,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::None,
            .revisionPolicy = RevisionPolicy::None,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::None,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::ReadOnly,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::Unsupported,
        });
        Q_ASSERT(result);
        result = m_catalog.add({
            .id = OperationIds::exports::midi::start,
            .category = QStringLiteral("exports"),
            .kind = OperationKind::Command,
            .syncMode = SyncMode::Synchronous,
            .documentPolicy = DocumentPolicy::Read,
            .revisionPolicy = RevisionPolicy::Check,
            .historyPolicy = HistoryPolicy::None,
            .fileAccess = FileAccessPolicy::Write,
            .hostAvailability = HostAvailability::Core,
            .safety = SafetyClass::FileSystem,
            .exposure = ExposurePolicy::InternalOnly,
            .idempotency = IdempotencyPolicy::DocumentGeneration,
        });
        Q_ASSERT(result);
    }

} // namespace Automation
