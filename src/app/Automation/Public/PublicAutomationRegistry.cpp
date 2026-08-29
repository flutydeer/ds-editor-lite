#include "PublicAutomationRegistry.h"
#include "PublicAutomationCodecs.h"
#include "PublicCollectionPagination.h"

#include "../CoreRuntime.h"
#include "../OperationIds.h"
#include "Modules/FillLyric/Utils/SplitLyric.h"

#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/CanonicalJson.h>
#include <lite/AutomationWire/PublicConstants.h>
#include <lite/AutomationWire/PublicValueDomains.h>

#include <lite/ProjectConverters/DspxProjectParser.h>
#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/ProjectConverters/MidiTextCodecConverter.h>
#include <lite/ProjectModel/AppModel/SingerIdentifier.h>
#include <lite/ProjectModel/AppModel/Note.h>

#include <opendspx/clip.h>
#include <opendspx/singingclip.h>

#include <QFileInfo>
#include <QFile>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QJsonDocument>
#include <QSet>
#include <QThreadPool>
#include <QTimer>
#include <QVersionNumber>

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace Automation {
    namespace ToolNames = AutomationWire::PublicToolNames;

    namespace {
        using PublicRegistryDetail::paginateJson;

        AutomationError error(const AutomationErrorCode code, QString message,
                              QString fieldPath = {}) {
            AutomationError result;
            result.code = code;
            result.message = std::move(message);
            result.fieldPath = std::move(fieldPath);
            return result;
        }

        AutomationError unavailable(const QString &message) {
            return error(AutomationErrorCode::HostCapabilityUnavailable, message);
        }

        bool supportsPurpose(const ProjectFormatDto &format, const QString &purpose) {
            return (purpose == QStringLiteral("open") && format.canOpen) ||
                   (purpose == QStringLiteral("import") && format.canImport);
        }

        bool acceptsPath(const ProjectFormatDto &format, const QString &path) {
            const auto suffix = QFileInfo(path).suffix();
            return std::any_of(format.extensions.cbegin(), format.extensions.cend(),
                               [&](QString extension) {
                                   extension.remove(u'*');
                                   extension.remove(u'.');
                                   return extension.compare(suffix, Qt::CaseInsensitive) == 0;
                               });
        }

        AutomationResult<ProjectFormatDto> resolveFormat(const QList<ProjectFormatDto> &formats,
                                                         const QString &path,
                                                         const QString &purpose,
                                                         const QString &requestedId = {}) {
            const auto found =
                std::find_if(formats.cbegin(), formats.cend(), [&](const ProjectFormatDto &format) {
                    return supportsPurpose(format, purpose) && acceptsPath(format, path);
                });
            if (found == formats.cend()) {
                return error(AutomationErrorCode::FormatUnsupported,
                             QStringLiteral("No project format accepts the path for this purpose"),
                             QStringLiteral("path"));
            }
            if (!requestedId.isEmpty() && requestedId != found->id) {
                return AutomationError::invalidArgument(
                    QStringLiteral("format_id"),
                    QStringLiteral("The requested format does not match the file path"));
            }
            if (!found->available) {
                auto failure = unavailable(found->unavailableReason.isEmpty()
                                               ? QStringLiteral("The project format is unavailable")
                                               : found->unavailableReason);
                failure.fieldPath = QStringLiteral("format_id");
                return failure;
            }
            return *found;
        }

        AutomationResult<QString> normalizeMidiEncoding(const QString &encoding,
                                                        const QString &fieldPath) {
            if (encoding.isEmpty())
                return QString();
            for (const auto &codec : MidiTextCodecConverter::availableCodecs()) {
                if (QString::fromLatin1(codec.identifier).compare(encoding, Qt::CaseInsensitive) ==
                    0) {
                    return QString::fromLatin1(codec.identifier);
                }
            }
            return AutomationError::invalidArgument(
                fieldPath, QStringLiteral("The requested MIDI text encoding is unavailable"));
        }

        AutomationResult<QString>
            validateFormatOptions(const ProjectFormatDto &format, const QString &purpose,
                                  const QJsonObject &options,
                                  const QString &fieldPrefix = QStringLiteral("options")) {
            const auto encoding = options.value(QStringLiteral("encoding")).toString();
            if (!encoding.isEmpty() && format.id != QStringLiteral("midi")) {
                return AutomationError::invalidArgument(
                    fieldPrefix + QStringLiteral(".encoding"),
                    QStringLiteral("Text encoding is supported only by the MIDI importer"));
            }
            if (purpose == QStringLiteral("open") && format.id != QStringLiteral("midi") &&
                (options.contains(QStringLiteral("import_tempo")) ||
                 options.contains(QStringLiteral("import_time_signatures")))) {
                return AutomationError::invalidArgument(
                    fieldPrefix,
                    QStringLiteral("Timeline import options do not apply to this open format"));
            }
            return normalizeMidiEncoding(encoding, fieldPrefix + QStringLiteral(".encoding"));
        }

        AutomationResult<QJsonObject> inspectFormatPath(CoreRuntime &runtime,
                                                        const QList<ProjectFormatDto> &formats,
                                                        const QString &path, const QString &purpose,
                                                        const QString &requestedId = {}) {
            auto selected = resolveFormat(formats, path, purpose, requestedId);
            if (!selected)
                return selected.getError();

            QFile file(path);
            if (!file.open(QIODevice::ReadOnly)) {
                return error(AutomationErrorCode::IoError,
                             QStringLiteral("The project file could not be read"),
                             QStringLiteral("path"));
            }
            const auto bytes = file.readAll();
            if (file.error() != QFileDevice::NoError) {
                return error(AutomationErrorCode::IoError,
                             QStringLiteral("The project file could not be read completely"),
                             QStringLiteral("path"));
            }

            QJsonArray sources;
            QJsonArray lyricsPreview;
            QString encoding;
            if (purpose != QStringLiteral("export")) {
                if (selected.get().id == QStringLiteral("midi")) {
                    const auto parsed = MidiFileParser::parse(path);
                    if (!parsed.valid) {
                        return error(AutomationErrorCode::FormatUnsupported,
                                     parsed.errorMessage.isEmpty()
                                         ? QStringLiteral("The MIDI file could not be parsed")
                                         : parsed.errorMessage,
                                     QStringLiteral("path"));
                    }
                    QByteArray lyricBytes;
                    for (const auto &track : parsed.trackInfos) {
                        for (const auto &lyric : track.lyrics)
                            lyricBytes.append(lyric);
                    }
                    auto codec = MidiTextCodecConverter::detectEncoding(lyricBytes);
                    if (codec.isEmpty())
                        codec = MidiTextCodecConverter::defaultCodec();
                    encoding = QString::fromLatin1(codec);
                    for (qsizetype index = 0; index < parsed.trackInfos.size(); ++index) {
                        const auto &track = parsed.trackInfos.at(index);
                        sources.append(QJsonObject{
                            {QStringLiteral("index"), static_cast<qint64>(index)},
                            {QStringLiteral("name"),
                             MidiTextCodecConverter::decode(track.name, codec)},
                            {QStringLiteral("kind"), QStringLiteral("channel")},
                        });
                        for (const auto &lyric : track.lyrics) {
                            if (lyricsPreview.size() >= 32)
                                break;
                            const auto decoded = MidiTextCodecConverter::decode(lyric, codec);
                            if (!decoded.isEmpty())
                                lyricsPreview.append(decoded);
                        }
                    }
                } else {
                    auto projectBytes = bytes;
                    if (selected.get().id != QStringLiteral("dspx")) {
                        auto converted = runtime.files().convertLibreSvipToDspx(path);
                        if (!converted)
                            return converted.getError();
                        projectBytes = converted.get();
                    }
                    const auto parsed = DspxProjectParser::parse(projectBytes);
                    if (!parsed.success()) {
                        return error(AutomationErrorCode::FormatUnsupported,
                                     parsed.errorMessage.isEmpty()
                                         ? QStringLiteral("The project file could not be parsed")
                                         : parsed.errorMessage,
                                     QStringLiteral("path"));
                    }
                    encoding = QStringLiteral("UTF-8");
                    for (qsizetype index = 0;
                         index < static_cast<qsizetype>(parsed.model->content.tracks.size());
                         ++index) {
                        const auto &track = parsed.model->content.tracks.at(index);
                        sources.append(QJsonObject{
                            {QStringLiteral("index"), static_cast<qint64>(index)        },
                            {QStringLiteral("name"),  QString::fromStdString(track.name)},
                            {QStringLiteral("kind"),  QStringLiteral("track")           },
                        });
                        for (const auto &clip : track.clips) {
                            if (!clip || clip->type != opendspx::Clip::Type::Singing)
                                continue;
                            const auto singing =
                                std::static_pointer_cast<opendspx::SingingClip>(clip);
                            for (const auto &note : singing->notes) {
                                if (lyricsPreview.size() >= 32)
                                    break;
                                const auto lyric = QString::fromStdString(note.lyric);
                                if (!lyric.isEmpty())
                                    lyricsPreview.append(lyric);
                            }
                        }
                    }
                }
            }

            QJsonObject result{
                {QStringLiteral("path"),               path                       },
                {QStringLiteral("purpose"),            purpose                    },
                {QStringLiteral("format_id"),          selected.get().id          },
                {QStringLiteral("sources"),            sources                    },
                {QStringLiteral("encoding"),           encoding                   },
                {QStringLiteral("lyrics_preview"),     lyricsPreview              },
                {QStringLiteral("timeline_supported"), true                       },
                {QStringLiteral("option_schema"),      selected.get().optionSchema},
            };
            auto digestInput = result;
            digestInput.insert(
                QStringLiteral("file_sha256"),
                QString::fromLatin1(
                    QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()));
            result.insert(QStringLiteral("plan_digest"), AutomationWire::sha256Digest(digestInput));
            return result;
        }

        std::function<AutomationResult<AutomationUnit>()> planRevalidator(
            CoreRuntime &runtime, AutomationFileGuard &fileGuard, QList<ProjectFormatDto> formats,
            QString path, QString purpose, QString formatId, QString expectedDigest,
            QString pathField, QString digestField) {
            if (expectedDigest.isEmpty())
                return {};
            return [&runtime, &fileGuard, formats = std::move(formats), path = std::move(path),
                    purpose = std::move(purpose), formatId = std::move(formatId),
                    expectedDigest = std::move(expectedDigest), pathField = std::move(pathField),
                    digestField = std::move(digestField)]() -> AutomationResult<AutomationUnit> {
                auto authorized =
                    fileGuard.reauthorize({path, FileAccessPurpose::Read});
                if (!authorized) {
                    auto failure = authorized.getError();
                    failure.fieldPath = pathField;
                    return failure;
                }
                auto plan = inspectFormatPath(runtime, formats, path, purpose, formatId);
                if (!plan) {
                    auto failure = plan.getError();
                    failure.fieldPath = pathField;
                    return failure;
                }
                if (plan.get().value(QStringLiteral("plan_digest")).toString() != expectedDigest) {
                    return AutomationError::invalidArgument(
                        digestField,
                        QStringLiteral("The %1 plan digest is stale or invalid").arg(purpose));
                }
                return AutomationUnit{};
            };
        }

        AutomationResult<MidiExportOptionsDto>
            decodeMidiExportOptions(const ProjectSnapshotDto &project, const QJsonObject &encoded) {
            MidiExportOptionsDto result;
            result.includeTempo = encoded.value(QStringLiteral("include_tempo")).toBool(true);
            result.includeTimeSignatures =
                encoded.value(QStringLiteral("include_time_signatures")).toBool(true);
            result.includeLyrics = encoded.value(QStringLiteral("include_lyrics")).toBool(true);

            QSet<int> availableTracks;
            QSet<int> availableSingingClips;
            QSet<int> availableAudioClips;
            for (const auto &track : project.tracks) {
                availableTracks.insert(track.id.value());
                for (const auto &clip : track.clips) {
                    if (clip.data.type == ClipDraftDto::Type::Singing)
                        availableSingingClips.insert(clip.id.value());
                    else
                        availableAudioClips.insert(clip.id.value());
                }
            }

            QSet<int> selectedTracks;
            for (const auto &value : encoded.value(QStringLiteral("track_ids")).toArray()) {
                const auto id = value.toInt();
                if (selectedTracks.contains(id)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("options.track_ids"),
                        QStringLiteral("MIDI export track IDs must be unique"));
                }
                if (!availableTracks.contains(id)) {
                    return AutomationError::notFound(
                        {ObjectKind::Track, id},
                        QStringLiteral("A selected MIDI export track was not found"));
                }
                selectedTracks.insert(id);
                result.trackIds.append(TrackId(id));
            }
            QSet<int> selectedClips;
            for (const auto &value : encoded.value(QStringLiteral("clip_ids")).toArray()) {
                const auto id = value.toInt();
                if (selectedClips.contains(id)) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("options.clip_ids"),
                        QStringLiteral("MIDI export clip IDs must be unique"));
                }
                if (availableAudioClips.contains(id)) {
                    return AutomationError::wrongObjectType(
                        {ObjectKind::Clip, id},
                        QStringLiteral("MIDI export accepts only singing clips"));
                }
                if (!availableSingingClips.contains(id)) {
                    return AutomationError::notFound(
                        {ObjectKind::Clip, id},
                        QStringLiteral("A selected MIDI export clip was not found"));
                }
                selectedClips.insert(id);
                result.clipIds.append(ClipId(id));
            }
            return result;
        }

        QPair<QJsonArray, QJsonArray> effectiveMidiSelection(const ProjectSnapshotDto &project,
                                                             const MidiExportOptionsDto &options) {
            QSet<int> selectedTracks;
            for (const auto id : options.trackIds)
                selectedTracks.insert(id.value());
            QSet<int> selectedClips;
            for (const auto id : options.clipIds)
                selectedClips.insert(id.value());
            const auto selectAll = selectedTracks.isEmpty() && selectedClips.isEmpty();

            QJsonArray tracks;
            QJsonArray clips;
            for (const auto &track : project.tracks) {
                const auto wholeTrack = selectAll || selectedTracks.contains(track.id.value());
                bool includedTrack = wholeTrack;
                for (const auto &clip : track.clips) {
                    if (clip.data.type != ClipDraftDto::Type::Singing)
                        continue;
                    if (wholeTrack || selectedClips.contains(clip.id.value())) {
                        clips.append(clip.id.value());
                        includedTrack = true;
                    }
                }
                if (includedTrack)
                    tracks.append(track.id.value());
            }
            return {tracks, clips};
        }

        QJsonObject encodeDocumentVersion(const DocumentVersion &version) {
            return {
                {QStringLiteral("document_id"), version.documentId.toString()        },
                {QStringLiteral("revision"),    static_cast<qint64>(version.revision)},
            };
        }

        QJsonValue encodeNullableDocumentVersion(const DocumentVersion &version) {
            return version.documentId.isNull() ? QJsonValue(QJsonValue::Null)
                                               : QJsonValue(encodeDocumentVersion(version));
        }

        QJsonObject encodeObjectRef(const ObjectRef &ref) {
            return {
                {QStringLiteral("kind"), encodePublicObjectKind(ref.kind)},
                {QStringLiteral("id"),   ref.value                       },
            };
        }

        QJsonObject encodeMutation(const MutationResult &mutation) {
            QJsonArray affected;
            for (const auto &ref : mutation.affectedObjects)
                affected.append(encodeObjectRef(ref));
            QJsonArray created;
            for (const auto &ref : mutation.createdObjects) {
                created.append(QJsonObject{
                    {QStringLiteral("client_ref"), ref.clientRef              },
                    {QStringLiteral("object"),     encodeObjectRef(ref.object)},
                });
            }
            QJsonArray resolvedValues;
            for (const auto &resolved : mutation.resolvedValues) {
                resolvedValues.append(QJsonObject{
                    {QStringLiteral("field_path"), resolved.fieldPath},
                    {QStringLiteral("value"),      resolved.value    },
                });
            }
            QJsonArray presentationEffects;
            for (const auto &effect : mutation.presentationEffects)
                presentationEffects.append(effect);
            QJsonArray warnings;
            for (const auto &warning : mutation.warnings)
                warnings.append(warning);
            return {
                {QStringLiteral("previous"),             encodeNullableDocumentVersion(mutation.previous)},
                {QStringLiteral("current"),              encodeNullableDocumentVersion(mutation.current) },
                {QStringLiteral("changed"),              mutation.changed                                },
                {QStringLiteral("validated_only"),       mutation.validatedOnly                          },
                {QStringLiteral("affected_objects"),     affected                                        },
                {QStringLiteral("created_objects"),      created                                         },
                {QStringLiteral("resolved_values"),      resolvedValues                                  },
                {QStringLiteral("presentation_effects"), presentationEffects                             },
                {QStringLiteral("warnings"),             warnings                                        },
            };
        }

        QJsonObject encodeDocumentLifecycleResult(const MutationResult &mutation,
                                                  const QString &path = {},
                                                  const bool dirty = false) {
            QJsonArray warnings;
            for (const auto &warning : mutation.warnings)
                warnings.append(warning);
            QJsonArray presentationEffects;
            for (const auto &effect : mutation.presentationEffects)
                presentationEffects.append(effect);
            return {
                {QStringLiteral("previous"),             encodeNullableDocumentVersion(mutation.previous)},
                {QStringLiteral("current"),              encodeNullableDocumentVersion(mutation.current) },
                {QStringLiteral("path"),                 path                                            },
                {QStringLiteral("dirty"),                dirty                                           },
                {QStringLiteral("changed"),              mutation.changed                                },
                {QStringLiteral("validated_only"),       mutation.validatedOnly                          },
                {QStringLiteral("presentation_effects"), presentationEffects                             },
                {QStringLiteral("warnings"),             warnings                                        },
            };
        }

        QJsonObject encodeTaskAccepted(const TaskAcceptedResult &accepted) {
            QJsonObject result{
                {QStringLiteral("scope"),          accepted.scope == AutomationTaskScope::Application
                                              ? QStringLiteral("application")
                                              : QStringLiteral("document")},
                {QStringLiteral("document"),
                 accepted.document.documentId.isNull()
                     ? QJsonValue(QJsonValue::Null)
                     : QJsonValue(encodeDocumentVersion(accepted.document))                                        },
                {QStringLiteral("validated_only"), accepted.validatedOnly                                          },
            };
            if (!accepted.validatedOnly)
                result.insert(QStringLiteral("task_id"), accepted.taskId.toString());
            return result;
        }

        QString taskStateName(const AutomationTaskState state) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::TaskState);
            const auto index = static_cast<qsizetype>(state);
            return index >= 0 && index < values.size() ? values.at(index) : QString();
        }

        bool isPublicTaskKind(const QString &operationId) {
            return AutomationWire::publicStringValueDomainValues(
                       AutomationWire::PublicValueDomain::TaskKind)
                .contains(operationId);
        }

        QJsonObject encodeAutomationError(const AutomationError &value) {
            QJsonObject result{
                {QStringLiteral("code"),    errorCodeName(value.code)},
                {QStringLiteral("message"), value.message            },
            };
            if (!value.fieldPath.isEmpty())
                result.insert(QStringLiteral("field_path"), value.fieldPath);
            return result;
        }

        QJsonObject encodeTaskSnapshot(const AutomationTaskSnapshot &snapshot) {
            QJsonObject progress{
                {QStringLiteral("minimum"),       snapshot.progress.minimum      },
                {QStringLiteral("maximum"),       snapshot.progress.maximum      },
                {QStringLiteral("value"),         snapshot.progress.value        },
                {QStringLiteral("indeterminate"), snapshot.progress.indeterminate},
            };
            QJsonObject result{
                {QStringLiteral("task_id"),      snapshot.taskId.toString()                                        },
                {QStringLiteral("operation_id"), snapshot.operationId                                              },
                {QStringLiteral("scope"),        snapshot.scope == AutomationTaskScope::Application
                                              ? QStringLiteral("application")
                                              : QStringLiteral("document")},
                {QStringLiteral("document"),     encodeNullableDocumentVersion(snapshot.baseDocument)              },
                {QStringLiteral("state"),        taskStateName(snapshot.state)                                     },
                {QStringLiteral("progress"),     progress                                                          },
            };
            if (snapshot.mutation)
                result.insert(QStringLiteral("result"), encodeMutation(*snapshot.mutation));
            if (snapshot.applicationResult) {
                result.insert(QStringLiteral("application_result"), *snapshot.applicationResult);
            }
            if (snapshot.error)
                result.insert(QStringLiteral("error"), encodeAutomationError(*snapshot.error));
            if (!snapshot.createdByClientId.isEmpty()) {
                result.insert(QStringLiteral("created_by_client_id"), snapshot.createdByClientId);
            }
            return result;
        }

        DocumentId documentId(const QJsonObject &arguments) {
            return DocumentId::fromString(
                arguments.value(QStringLiteral("document_id")).toString());
        }

        CommandContext commandContext(const QJsonObject &arguments,
                                      const PublicInvocationContext &invocation) {
            return {
                .expected =
                    {
                               documentId(arguments),
                               static_cast<Revision>(
                            arguments.value(QStringLiteral("expected_revision")).toInteger()),
                               },
                .validateOnly = arguments.value(QStringLiteral("validate_only")).toBool(false),
                .idempotencyKey = arguments.value(QStringLiteral("idempotency_key")).toString(),
                .source = InvocationSource::PublicMcp,
                .clientId = invocation.clientId,
            };
        }

        ApplicationCommandContext
            applicationCommandContext(const QJsonObject &arguments,
                                      const PublicInvocationContext &invocation) {
            return {
                .validateOnly = arguments.value(QStringLiteral("validate_only")).toBool(false),
                .source = InvocationSource::PublicMcp,
                .clientId = invocation.clientId,
            };
        }

        CommandContext replacementCommandContext(CoreRuntime &runtime, const QJsonObject &arguments,
                                                 const PublicInvocationContext &invocation) {
            auto context = commandContext(arguments, invocation);
            context.expected = runtime.documentVersion();
            if (arguments.contains(QStringLiteral("current_document_id"))) {
                context.expected.documentId = DocumentId::fromString(
                    arguments.value(QStringLiteral("current_document_id")).toString());
            }
            if (arguments.contains(QStringLiteral("expected_revision"))) {
                context.expected.revision = static_cast<Revision>(
                    arguments.value(QStringLiteral("expected_revision")).toInteger());
            }
            return context;
        }

        AutomationResult<CommandContext>
            documentQueryCommandContext(CoreRuntime &runtime, const QJsonObject &arguments,
                                        const PublicInvocationContext &invocation) {
            auto document = runtime.documents().getDocument(documentId(arguments));
            if (!document)
                return document.getError();
            auto context = commandContext(arguments, invocation);
            context.expected = document.get().document;
            return context;
        }

        CommandContext playbackCommandContext(CoreRuntime &runtime, const QJsonObject &arguments,
                                              const PublicInvocationContext &invocation) {
            auto context = commandContext(arguments, invocation);
            context.expected.revision = runtime.documentVersion().revision;
            return context;
        }

        AutomationResult<AutomationUnit> validateInferenceScope(CoreRuntime &runtime,
                                                                const DocumentId &documentId,
                                                                const QJsonObject &scope) {
            auto project = runtime.project().getProject(documentId);
            if (!project)
                return project.getError();
            const auto kind = scope.value(QStringLiteral("kind")).toString();
            if (kind == QStringLiteral("track")) {
                QSet<int> missing;
                for (const auto &value : scope.value(QStringLiteral("track_ids")).toArray())
                    missing.insert(value.toInt());
                for (const auto &track : project.get().tracks)
                    missing.remove(track.id.value());
                if (!missing.isEmpty()) {
                    return AutomationError::notFound(
                        {ObjectKind::Track, *missing.cbegin()},
                        QStringLiteral("Inference scope track was not found"));
                }
            } else if (kind == QStringLiteral("clip")) {
                QSet<int> missing;
                for (const auto &value : scope.value(QStringLiteral("clip_ids")).toArray())
                    missing.insert(value.toInt());
                for (const auto &track : project.get().tracks) {
                    for (const auto &clip : track.clips) {
                        if (!missing.contains(clip.id.value()))
                            continue;
                        if (clip.data.type != ClipDraftDto::Type::Singing) {
                            return AutomationError::wrongObjectType(
                                {ObjectKind::Clip, clip.id.value()},
                                QStringLiteral("Inference requires a singing clip"));
                        }
                        missing.remove(clip.id.value());
                    }
                }
                if (!missing.isEmpty()) {
                    return AutomationError::notFound(
                        {ObjectKind::Clip, *missing.cbegin()},
                        QStringLiteral("Inference scope clip was not found"));
                }
            }
            return AutomationUnit{};
        }

        template <typename Id>
        QList<Id> objectIds(const QJsonArray &values) {
            QList<Id> result;
            result.reserve(values.size());
            for (const auto &value : values)
                result.append(Id(value.toInt()));
            return result;
        }

        AutomationResult<int> playbackLoopTick(const QJsonObject &arguments, const QString &field) {
            const auto value = arguments.value(field);
            const auto number = value.toDouble(std::numeric_limits<double>::quiet_NaN());
            if (!value.isDouble() || !std::isfinite(number) || number < 0.0 ||
                number > std::numeric_limits<int>::max() || std::floor(number) != number) {
                return AutomationError::invalidArgument(
                    field, QStringLiteral("Playback loop positions must be integer ticks"));
            }
            return static_cast<int>(number);
        }

        ParamInfo::Name parameterName(const QString &value) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::ParameterName);
            const auto index = values.indexOf(value);
            return index < 0 || index >= static_cast<qsizetype>(ParamInfo::Unknown)
                       ? ParamInfo::Unknown
                       : static_cast<ParamInfo::Name>(index);
        }

        QString parameterName(const ParamInfo::Name value) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::ParameterName);
            const auto index = static_cast<qsizetype>(value);
            return index >= 0 && index < values.size() ? values.at(index) : QString();
        }

        Param::Type parameterType(const QString &value) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::ParameterLayer);
            const auto index = values.indexOf(value);
            return index < 0 || index >= static_cast<qsizetype>(Param::Unknown)
                       ? Param::Unknown
                       : static_cast<Param::Type>(index);
        }

        QString parameterType(const Param::Type value) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::ParameterLayer);
            const auto index = static_cast<qsizetype>(value);
            return index >= 0 && index < values.size() ? values.at(index) : QString();
        }

        AnchorNode::InterpMode interpolation(const QString &value) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::Interpolation);
            if (value == values.value(1))
                return AnchorNode::Linear;
            if (value == values.value(2))
                return AnchorNode::None;
            return AnchorNode::Hermite;
        }

        QString interpolation(const AnchorNode::InterpMode value) {
            const auto values = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::Interpolation);
            switch (value) {
                case AnchorNode::Linear:
                    return values.value(1);
                case AnchorNode::Hermite:
                case AnchorNode::Cubic:
                    return values.value(0);
                case AnchorNode::None:
                    return values.value(2);
            }
            return {};
        }

        CurveDraftDto decodeCurve(const QJsonObject &object) {
            CurveDraftDto result;
            if (object.contains(QStringLiteral("curve_id")))
                result.id = CurveId(object.value(QStringLiteral("curve_id")).toInt());
            if (object.value(QStringLiteral("type")).toString() == QStringLiteral("anchor")) {
                result.type = CurveDraftDto::Type::Anchor;
                for (const auto &value : object.value(QStringLiteral("nodes")).toArray()) {
                    const auto node = value.toObject();
                    AnchorNodeDraftDto draft{
                        node.value(QStringLiteral("position")).toInt(),
                        node.value(QStringLiteral("value")).toInt(),
                        interpolation(node.value(QStringLiteral("interpolation")).toString()),
                    };
                    if (node.contains(QStringLiteral("anchor_id")))
                        draft.id = AnchorId(node.value(QStringLiteral("anchor_id")).toInt());
                    result.nodes.append(std::move(draft));
                }
            } else {
                result.type = CurveDraftDto::Type::Draw;
                result.localStart = object.value(QStringLiteral("local_start")).toInt();
                result.step = object.value(QStringLiteral("step")).toInt(5);
                for (const auto &value : object.value(QStringLiteral("values")).toArray())
                    result.values.append(value.toInt());
            }
            return result;
        }

        QJsonObject encodeCurve(const CurveDraftDto &curve) {
            if (curve.type == CurveDraftDto::Type::Anchor) {
                QJsonArray nodes;
                for (const auto &node : curve.nodes) {
                    QJsonObject encoded{
                        {QStringLiteral("anchor_id"),     node.id.value()                  },
                        {QStringLiteral("position"),      node.position                    },
                        {QStringLiteral("value"),         node.value                       },
                        {QStringLiteral("interpolation"), interpolation(node.interpolation)},
                    };
                    nodes.append(encoded);
                }
                QJsonObject result{
                    {QStringLiteral("type"),     QStringLiteral("anchor")},
                    {QStringLiteral("curve_id"), curve.id.value()        },
                    {QStringLiteral("nodes"),    nodes                   },
                };
                return result;
            }
            QJsonArray values;
            for (const auto value : curve.values)
                values.append(value);
            QJsonObject result{
                {QStringLiteral("type"),        QStringLiteral("draw")},
                {QStringLiteral("curve_id"),    curve.id.value()      },
                {QStringLiteral("local_start"), curve.localStart      },
                {QStringLiteral("step"),        curve.step            },
                {QStringLiteral("values"),      values                },
            };
            return result;
        }

        Pronunciation decodePronunciation(const QJsonValue &value) {
            Pronunciation result;
            if (value.isString()) {
                result.edited = value.toString();
            } else {
                const auto object = value.toObject();
                const auto text = object.value(QStringLiteral("value")).toString();
                if (object.value(QStringLiteral("source")).toString() ==
                    QStringLiteral("original")) {
                    result.original = text;
                } else {
                    result.edited = text;
                }
            }
            return result;
        }

        Phonemes decodePhonemes(const QJsonArray &array) {
            Phonemes result;
            bool allOffsetsPresent = !array.isEmpty();
            for (const auto &value : array) {
                const auto object = value.toObject();
                PhonemeName name;
                name.language = object.value(QStringLiteral("language")).toString();
                name.name = object.value(QStringLiteral("symbol")).toString();
                result.nameSeq.edited.append(std::move(name));
                if (object.contains(QStringLiteral("offset"))) {
                    result.offsetSeq.edited.append(object.value(QStringLiteral("offset")).toInt());
                } else {
                    allOffsetsPresent = false;
                }
            }
            if (!allOffsetsPresent)
                result.offsetSeq.edited.clear();
            return result;
        }

        QJsonArray encodePhonemes(const Phonemes &phonemes) {
            QJsonArray result;
            const auto &names = phonemes.nameSeq.result();
            const auto &offsets = phonemes.offsetSeq.result();
            for (qsizetype index = 0; index < names.size(); ++index) {
                const auto &name = names.at(index);
                QJsonObject encoded{
                    {QStringLiteral("symbol"),   name.name    },
                    {QStringLiteral("language"), name.language},
                };
                if (index < offsets.size())
                    encoded.insert(QStringLiteral("offset"), offsets.at(index));
                result.append(encoded);
            }
            return result;
        }

        QJsonObject encodePronunciation(const Pronunciation &pronunciation) {
            const bool edited = !pronunciation.edited.isEmpty();
            return {
                {QStringLiteral("value"),  edited ? pronunciation.edited : pronunciation.original},
                {QStringLiteral("source"),
                 edited ? QStringLiteral("edited") : QStringLiteral("original")                  },
            };
        }

        QString decodeLanguageSelection(const QJsonValue &value, const QString &fallback) {
            if (value.isString())
                return value.toString();
            const auto selection = value.toObject();
            const auto mode = selection.value(QStringLiteral("mode")).toString();
            if (mode == QStringLiteral("explicit"))
                return selection.value(QStringLiteral("language_id")).toString();
            if (mode == QStringLiteral("unknown"))
                return QStringLiteral("unknown");
            return fallback;
        }

        QJsonObject encodeLanguageSelection(const QString &language,
                                            const QString &singerDefault = {}) {
            if (language.isEmpty() || (!singerDefault.isEmpty() && language == singerDefault))
                return {
                    {QStringLiteral("mode"), QStringLiteral("follow_singer")}
                };
            if (language == QStringLiteral("unknown"))
                return {
                    {QStringLiteral("mode"), QStringLiteral("unknown")}
                };
            return {
                {QStringLiteral("mode"),        QStringLiteral("explicit")},
                {QStringLiteral("language_id"), language                  },
            };
        }

        NoteDraftDto decodeNote(const QJsonObject &object, const QString &defaultLanguage = {},
                                const QString &defaultLyric = QStringLiteral("la")) {
            NoteDraftDto result;
            result.clientRef = object.value(QStringLiteral("client_ref")).toString();
            result.localStart = object.value(QStringLiteral("local_start")).toInt();
            result.length = object.value(QStringLiteral("length")).toInt();
            result.keyIndex = object.value(QStringLiteral("key_index")).toInt(60);
            result.centShift = object.value(QStringLiteral("cent_shift")).toInt();
            result.lyric = object.contains(QStringLiteral("lyric"))
                               ? object.value(QStringLiteral("lyric")).toString()
                               : defaultLyric;
            result.language = object.contains(QStringLiteral("language"))
                                  ? decodeLanguageSelection(
                                        object.value(QStringLiteral("language")), defaultLanguage)
                                  : defaultLanguage;
            result.pronunciation =
                decodePronunciation(object.value(QStringLiteral("pronunciation")));
            for (const auto &candidate :
                 object.value(QStringLiteral("pronunciation_candidates")).toArray()) {
                result.pronunciationCandidates.append(candidate.toString());
            }
            result.phonemes = decodePhonemes(object.value(QStringLiteral("phonemes")).toArray());
            result.lineFeed = object.value(QStringLiteral("line_feed")).toBool();
            return result;
        }

        QJsonObject encodeNoteDraft(const NoteDraftDto &note) {
            QJsonArray pronunciationCandidates;
            for (const auto &candidate : note.pronunciationCandidates)
                pronunciationCandidates.append(candidate);
            return {
                {QStringLiteral("client_ref"),               note.clientRef                         },
                {QStringLiteral("local_start"),              note.localStart                        },
                {QStringLiteral("length"),                   note.length                            },
                {QStringLiteral("key_index"),                note.keyIndex                          },
                {QStringLiteral("cent_shift"),               note.centShift                         },
                {QStringLiteral("lyric"),                    note.lyric                             },
                {QStringLiteral("language"),                 note.language                          },
                {QStringLiteral("pronunciation"),            encodePronunciation(note.pronunciation)},
                {QStringLiteral("pronunciation_candidates"), pronunciationCandidates                },
                {QStringLiteral("phonemes"),                 encodePhonemes(note.phonemes)          },
                {QStringLiteral("line_feed"),                note.lineFeed                          },
            };
        }

        ClipPropertiesDto decodeClipProperties(const QJsonObject &object) {
            ClipPropertiesDto result;
            result.id = ClipId(object.value(QStringLiteral("clip_id")).toInt(-1));
            result.name = object.value(QStringLiteral("name")).toString();
            result.start = object.value(QStringLiteral("start")).toInt();
            result.length = object.value(QStringLiteral("length")).toInt(1);
            result.clipStart = object.value(QStringLiteral("clip_start")).toInt();
            result.clipLen = object.value(QStringLiteral("clip_length")).toInt(result.length);
            result.gain = object.value(QStringLiteral("gain")).toDouble();
            result.mute = object.value(QStringLiteral("mute")).toBool();
            return result;
        }

        PublicAudioClipProperties decodeAudioImportProperties(const QJsonObject &object) {
            PublicAudioClipProperties result;
            if (object.contains(QStringLiteral("name")))
                result.name = object.value(QStringLiteral("name")).toString();
            if (object.contains(QStringLiteral("start")))
                result.start = object.value(QStringLiteral("start")).toInt();
            if (object.contains(QStringLiteral("length")))
                result.length = object.value(QStringLiteral("length")).toInt();
            if (object.contains(QStringLiteral("clip_start")))
                result.clipStart = object.value(QStringLiteral("clip_start")).toInt();
            if (object.contains(QStringLiteral("clip_length")))
                result.clipLength = object.value(QStringLiteral("clip_length")).toInt();
            if (object.contains(QStringLiteral("gain")))
                result.gain = object.value(QStringLiteral("gain")).toDouble();
            if (object.contains(QStringLiteral("mute")))
                result.mute = object.value(QStringLiteral("mute")).toBool();
            return result;
        }

        TrackPropertiesDto decodeTrackProperties(const QJsonObject &object) {
            TrackPropertiesDto result;
            result.id = TrackId(object.value(QStringLiteral("track_id")).toInt(-1));
            result.name = object.value(QStringLiteral("name")).toString();
            result.gain = object.value(QStringLiteral("gain")).toDouble();
            result.pan = object.value(QStringLiteral("pan")).toDouble();
            result.mute = object.value(QStringLiteral("mute")).toBool();
            result.solo = object.value(QStringLiteral("solo")).toBool();
            return result;
        }

        SingerInfo decodeSinger(const QJsonObject &object) {
            SingerIdentifier identifier{
                object.value(QStringLiteral("singer_id")).toString(),
                object.value(QStringLiteral("package_id")).toString(),
                QVersionNumber::fromString(
                    object.value(QStringLiteral("package_version")).toString()),
            };
            return SingerInfo(identifier);
        }

        AutomationResult<QVersionNumber> parsePackageVersion(const QJsonObject &object,
                                                             const QString &fieldPath) {
            const auto text = object.value(QStringLiteral("package_version")).toString().trimmed();
            qsizetype suffixIndex = 0;
            const auto version = QVersionNumber::fromString(text, &suffixIndex);
            if (version.isNull() || suffixIndex != text.size()) {
                return AutomationError::invalidArgument(
                    fieldPath, QStringLiteral("Package version is invalid"));
            }
            return version;
        }

        SpeakerInfo decodeSpeaker(const QJsonObject &object) {
            return SpeakerInfo(object.value(QStringLiteral("speaker_id")).toString());
        }

        QJsonObject encodeSingerRef(const SingerInfo &singer) {
            return {
                {QStringLiteral("package_id"),      singer.packageId()                },
                {QStringLiteral("package_version"), singer.packageVersion().toString()},
                {QStringLiteral("singer_id"),       singer.singerId()                 },
            };
        }

        QJsonObject encodeVoiceSelection(const SingerInfo &singer, const SpeakerInfo &speaker) {
            return {
                {QStringLiteral("singer"),  encodeSingerRef(singer)                         },
                {QStringLiteral("speaker"),
                 speaker.isEmpty()
                     ? QJsonValue(QJsonValue::Null)
                     : QJsonValue(QJsonObject{{QStringLiteral("speaker_id"), speaker.id()}})},
            };
        }

        AutomationResult<QPair<SingerInfo, SpeakerInfo>>
            resolveVoiceSelection(CoreRuntime &runtime, const QJsonObject &selection,
                                  const QString &fieldPath) {
            const auto singerRef = selection.value(QStringLiteral("singer")).toObject();
            const auto speakerRef = selection.value(QStringLiteral("speaker")).toObject();
            const auto packageId = singerRef.value(QStringLiteral("package_id")).toString();
            const auto singerId = singerRef.value(QStringLiteral("singer_id")).toString();
            const auto speakerId = speakerRef.value(QStringLiteral("speaker_id")).toString();
            auto packageVersion = parsePackageVersion(
                singerRef, fieldPath + QStringLiteral(".singer.package_version"));
            if (!packageVersion)
                return packageVersion.getError();
            auto packages = runtime.packages().getInstalledPackages();
            if (!packages)
                return packages.getError();
            for (const auto &package : packages.get()) {
                for (const auto &singer : package.singers) {
                    if (singer.packageId != packageId ||
                        singer.packageVersion != packageVersion.get() ||
                        singer.singerId != singerId) {
                        continue;
                    }
                    const auto speakers = singer.info.speakers();
                    if (speakerId.isEmpty()) {
                        if (speakers.isEmpty())
                            return QPair<SingerInfo, SpeakerInfo>{singer.info, {}};
                        if (speakers.size() == 1)
                            return QPair<SingerInfo, SpeakerInfo>{singer.info,
                                                                  speakers.constFirst()};
                        return AutomationError::invalidArgument(
                            fieldPath + QStringLiteral(".speaker"),
                            QStringLiteral("A speaker must be selected when the singer has "
                                           "multiple speakers"));
                    }
                    for (const auto &speaker : speakers) {
                        if (speaker.id() == speakerId)
                            return QPair<SingerInfo, SpeakerInfo>{singer.info, speaker};
                    }
                    return AutomationError::invalidArgument(
                        fieldPath + QStringLiteral(".speaker"),
                        QStringLiteral("The selected speaker does not belong to the singer"));
                }
            }
            return AutomationError::invalidArgument(
                fieldPath + QStringLiteral(".singer"),
                QStringLiteral("The selected singer is not installed"));
        }

        AutomationResult<QPair<SingerInfo, SpeakerMixModel::SpeakerMixData>>
            resolveSpeakerMix(CoreRuntime &runtime, const QJsonObject &object,
                              const QString &fieldPath) {
            const auto singerRef = object.value(QStringLiteral("singer")).toObject();
            const auto packageId = singerRef.value(QStringLiteral("package_id")).toString();
            const auto singerId = singerRef.value(QStringLiteral("singer_id")).toString();
            auto packageVersion = parsePackageVersion(
                singerRef, fieldPath + QStringLiteral(".singer.package_version"));
            if (!packageVersion)
                return packageVersion.getError();
            auto packages = runtime.packages().getInstalledPackages();
            if (!packages)
                return packages.getError();
            for (const auto &package : packages.get()) {
                for (const auto &singer : package.singers) {
                    if (singer.packageId != packageId ||
                        singer.packageVersion != packageVersion.get() ||
                        singer.singerId != singerId) {
                        continue;
                    }
                    SpeakerMixModel::SpeakerMixData mix;
                    QVector<double> fullWeights;
                    QSet<QString> sourceIds;
                    const auto &capability = singer.info.capability();
                    for (const auto &value : object.value(QStringLiteral("sources")).toArray()) {
                        const auto source = value.toObject();
                        const auto speakerId = source.value(QStringLiteral("speaker"))
                                                   .toObject()
                                                   .value(QStringLiteral("speaker_id"))
                                                   .toString();
                        if (sourceIds.contains(speakerId)) {
                            return AutomationError::invalidArgument(
                                fieldPath + QStringLiteral(".sources"),
                                QStringLiteral("Speaker mix sources must be unique"));
                        }
                        sourceIds.insert(speakerId);
                        const auto speakers = singer.info.speakers();
                        const auto found = std::find_if(
                            speakers.cbegin(), speakers.cend(),
                            [&](const SpeakerInfo &speaker) { return speaker.id() == speakerId; });
                        if (found == speakers.cend()) {
                            return AutomationError::invalidArgument(
                                fieldPath + QStringLiteral(".sources"),
                                QStringLiteral("A speaker does not belong to the singer"));
                        }
                        if (capability && !capability->mixableSpeakers.isEmpty() &&
                            !capability->mixableSpeakers.contains(speakerId)) {
                            return AutomationError::invalidArgument(
                                fieldPath + QStringLiteral(".sources"),
                                QStringLiteral("A speaker is not available for mixing"));
                        }
                        mix.sources.append({*found});
                        fullWeights.append(source.value(QStringLiteral("weight")).toDouble());
                    }
                    if (mix.sources.size() == 1) {
                        mix.mode = SpeakerMixModel::SingerSourceMode::Single;
                    } else {
                        mix.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
                        mix.fixedWeights =
                            SpeakerMixModel::explicitWeightsFromFullWeights(fullWeights);
                    }
                    return QPair<SingerInfo, SpeakerMixModel::SpeakerMixData>{
                        singer.info, SpeakerMixModel::normalizeSpeakerMixData(mix)};
                }
            }
            return AutomationError::invalidArgument(
                fieldPath + QStringLiteral(".singer"),
                QStringLiteral("The selected singer is not installed"));
        }

        SpeakerMixModel::SpeakerMixData decodeSpeakerMix(const QJsonObject &object) {
            SpeakerMixModel::SpeakerMixData result;
            if (object.contains(QStringLiteral("sources"))) {
                QVector<double> fullWeights;
                for (const auto &value : object.value(QStringLiteral("sources")).toArray()) {
                    const auto source = value.toObject();
                    result.sources.append(
                        {decodeSpeaker(source.value(QStringLiteral("speaker")).toObject())});
                    fullWeights.append(source.value(QStringLiteral("weight")).toDouble());
                }
                if (result.sources.size() > 1) {
                    result.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
                    result.fixedWeights =
                        SpeakerMixModel::explicitWeightsFromFullWeights(fullWeights);
                }
                return SpeakerMixModel::normalizeSpeakerMixData(result);
            }
            const auto mode = object.value(QStringLiteral("mode")).toString();
            result.mode =
                mode == QStringLiteral("dynamic") ? SpeakerMixModel::SingerSourceMode::DynamicMix
                : mode == QStringLiteral("fixed") ? SpeakerMixModel::SingerSourceMode::FixedMix
                                                  : SpeakerMixModel::SingerSourceMode::Single;
            for (const auto &value : object.value(QStringLiteral("speakers")).toArray())
                result.sources.append({decodeSpeaker(value.toObject())});
            for (const auto &value : object.value(QStringLiteral("fixed_weights")).toArray())
                result.fixedWeights.append(value.toDouble());
            for (const auto &value : object.value(QStringLiteral("dynamic_keyframes")).toArray()) {
                const auto keyframe = value.toObject();
                SpeakerMixModel::SpeakerMixKeyframe decoded;
                decoded.tick = keyframe.value(QStringLiteral("position")).toInt();
                for (const auto &weight : keyframe.value(QStringLiteral("weights")).toArray())
                    decoded.weights.append(weight.toDouble());
                result.dynamicKeyframes.append(std::move(decoded));
            }
            return result;
        }

        TrackDraftDto decodeTrack(const QJsonObject &object) {
            TrackDraftDto result;
            result.clientRef = object.value(QStringLiteral("client_ref")).toString();
            result.name = object.value(QStringLiteral("name")).toString();
            result.colorIndex = object.value(QStringLiteral("color_index")).toInt();
            result.gain = object.value(QStringLiteral("gain")).toDouble();
            result.pan = object.value(QStringLiteral("pan")).toDouble();
            result.mute = object.value(QStringLiteral("mute")).toBool();
            result.solo = object.value(QStringLiteral("solo")).toBool();
            result.defaultLanguage = object.value(QStringLiteral("default_language")).toString();
            if (object.contains(QStringLiteral("voice")))
                result.singerInfo = decodeSinger(object.value(QStringLiteral("voice")).toObject());
            return result;
        }

        ClipDraftDto decodeClip(const QJsonObject &object) {
            ClipDraftDto result;
            result.clientRef = object.value(QStringLiteral("client_ref")).toString();
            result.type = ClipDraftDto::Type::Singing;
            if (object.contains(QStringLiteral("properties"))) {
                result.properties =
                    decodeClipProperties(object.value(QStringLiteral("properties")).toObject());
            } else {
                result.properties.start = object.value(QStringLiteral("start")).toInt();
                result.properties.length = object.value(QStringLiteral("length")).toInt(1);
                result.properties.clipLen = result.properties.length;
                result.properties.name = object.value(QStringLiteral("name")).toString();
            }
            result.defaultLanguage = object.value(QStringLiteral("default_language")).toString();
            for (const auto &value : object.value(QStringLiteral("notes")).toArray())
                result.notes.append(decodeNote(value.toObject()));
            for (const auto &value : object.value(QStringLiteral("parameters")).toArray()) {
                const auto parameter = value.toObject();
                ParamCurvesDraftDto draft;
                draft.name = parameterName(parameter.value(QStringLiteral("name")).toString());
                draft.type = parameterType(parameter.value(QStringLiteral("type")).toString());
                for (const auto &curve : parameter.value(QStringLiteral("curves")).toArray())
                    draft.curves.append(decodeCurve(curve.toObject()));
                result.params.append(std::move(draft));
            }
            if (object.contains(QStringLiteral("speaker_mix"))) {
                const auto mix = object.value(QStringLiteral("speaker_mix")).toObject();
                result.usesTrackVoiceContext = false;
                result.ownSingerInfo = decodeSinger(mix.value(QStringLiteral("singer")).toObject());
                result.ownSpeakerMixData = decodeSpeakerMix(mix);
            }
            return result;
        }

        TrackControl decodeTrackControl(const QJsonObject &object) {
            TrackControl result;
            result.setGain(object.value(QStringLiteral("gain")).toDouble());
            result.setPan(object.value(QStringLiteral("pan")).toDouble());
            result.setMute(object.value(QStringLiteral("mute")).toBool());
            result.setSolo(object.value(QStringLiteral("solo")).toBool());
            return result;
        }

        AudioExportConfigDto decodeAudioExportConfig(const QJsonObject &object,
                                                     const QString &path = {}) {
            AudioExportConfigDto result;
            const QFileInfo fileInfo(path);
            result.fileName = fileInfo.fileName();
            result.fileDirectory = fileInfo.absolutePath();
            const auto format = object.value(QStringLiteral("format")).toString();
            if (format == QStringLiteral("wav"))
                result.fileType = 0;
            else if (format == QStringLiteral("flac"))
                result.fileType = 1;
            else if (format == QStringLiteral("ogg"))
                result.fileType = 2;
            else if (format == QStringLiteral("mp3"))
                result.fileType = 3;
            result.mono =
                object.value(QStringLiteral("channel_mode")).toString() == QStringLiteral("mono");
            result.sampleRate = object.value(QStringLiteral("sample_rate")).toDouble(44100.0);
            const auto mixing = object.value(QStringLiteral("mixing_mode")).toString();
            if (mixing == QStringLiteral("separated"))
                result.mixingOption = 1;
            else if (mixing == QStringLiteral("separated_through_master"))
                result.mixingOption = 2;
            const auto source = object.value(QStringLiteral("source")).toString();
            if (source == QStringLiteral("selected"))
                result.sourceOption = 1;
            else if (source == QStringLiteral("custom"))
                result.sourceOption = 2;
            result.muteSoloEnabled = object.value(QStringLiteral("mute_solo_enabled")).toBool(true);
            for (const auto &value : object.value(QStringLiteral("source_ids")).toArray())
                result.sources.append(value.toInt());
            return result;
        }

        AutomationResult<AudioExportConfigDto>
            resolveAudioExportSources(CoreRuntime &runtime, const DocumentId &documentId,
                                      AudioExportConfigDto config) {
            if (config.sourceOption != 2)
                return config;
            auto project = runtime.project().getProject(documentId);
            if (!project)
                return project.getError();
            QHash<int, int> trackIndexes;
            for (qsizetype index = 0; index < project.get().tracks.size(); ++index)
                trackIndexes.insert(project.get().tracks.at(index).id.value(), index);
            for (auto &source : config.sources) {
                const auto found = trackIndexes.constFind(source);
                if (found == trackIndexes.cend()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("options.source_ids"),
                        QStringLiteral("Audio export source TrackId does not exist"));
                }
                source = *found;
            }
            return config;
        }

        AutomationResult<QJsonObject>
            encodeAudioExportPreview(const AudioExportPreviewDto &preview) {
            constexpr quint32 knownFlags = AudioExportNoFile | AudioExportDuplicatedFile |
                                           AudioExportWillOverwrite |
                                           AudioExportUnrecognizedTemplate | AudioExportLossyFormat;
            if ((preview.warningFlags & ~knownFlags) != 0) {
                return error(AutomationErrorCode::InternalError,
                             QStringLiteral("Audio export preview returned an unknown diagnostic"));
            }
            QJsonArray targets;
            for (const auto &path : preview.filePaths)
                targets.append(QJsonObject{
                    {QStringLiteral("path"), path}
                });
            QJsonArray diagnostics;
            const auto append = [&](const quint32 flag, const QString &code,
                                    const QString &message) {
                if ((preview.warningFlags & flag) == 0)
                    return;
                diagnostics.append(QJsonObject{
                    {QStringLiteral("code"),     code                     },
                    {QStringLiteral("severity"), QStringLiteral("warning")},
                    {QStringLiteral("message"),  message                  },
                    {QStringLiteral("blocking"), true                     },
                });
            };
            append(AudioExportNoFile, QStringLiteral("no_files"),
                   QStringLiteral("The export plan contains no output files"));
            append(AudioExportDuplicatedFile, QStringLiteral("duplicate_paths"),
                   QStringLiteral("The export plan contains duplicate target paths"));
            append(AudioExportWillOverwrite, QStringLiteral("will_overwrite"),
                   QStringLiteral("The export plan would overwrite existing files"));
            append(AudioExportUnrecognizedTemplate, QStringLiteral("unrecognized_template"),
                   QStringLiteral("The export file-name template is not recognized"));
            append(AudioExportLossyFormat, QStringLiteral("lossy_format"),
                   QStringLiteral("The selected audio format is lossy"));
            return QJsonObject{
                {QStringLiteral("plan"),
                 QJsonObject{
                     {QStringLiteral("base_directory"), preview.baseDirectory},
                     {QStringLiteral("targets"), targets},
                 }                                         },
                {QStringLiteral("diagnostics"), diagnostics},
            };
        }

        QJsonObject queryResult(const DocumentVersion &document, const QString &name,
                                const QJsonValue &value) {
            return {
                {QStringLiteral("document"), encodeDocumentVersion(document)},
                {name,                       value                          },
            };
        }

        AutomationResult<QJsonObject> mutationResult(AutomationResult<MutationResult> result) {
            if (!result)
                return result.getError();
            return encodeMutation(result.get());
        }

        AutomationResult<QJsonObject>
            taskAcceptedResult(AutomationResult<TaskAcceptedResult> result) {
            if (!result)
                return result.getError();
            return encodeTaskAccepted(result.get());
        }

        QJsonObject encodeProjectStatistics(const ProjectSnapshotDto &project) {
            qint64 lengthTicks = 0;
            qint64 emptyTracks = 0;
            qint64 singingOnlyTracks = 0;
            qint64 audioOnlyTracks = 0;
            qint64 mixedTracks = 0;
            qint64 singingClips = 0;
            qint64 audioClips = 0;
            for (const auto &track : project.tracks) {
                bool hasSinging = false;
                bool hasAudio = false;
                for (const auto &clip : track.clips) {
                    const auto &properties = clip.data.properties;
                    lengthTicks =
                        std::max(lengthTicks, static_cast<qint64>(properties.start) +
                                                  properties.clipStart + properties.clipLen);
                    if (clip.data.type == ClipDraftDto::Type::Singing) {
                        hasSinging = true;
                        ++singingClips;
                    } else {
                        hasAudio = true;
                        ++audioClips;
                    }
                }
                if (hasSinging && hasAudio)
                    ++mixedTracks;
                else if (hasSinging)
                    ++singingOnlyTracks;
                else if (hasAudio)
                    ++audioOnlyTracks;
                else
                    ++emptyTracks;
            }
            return {
                {QStringLiteral("length_ticks"),             lengthTicks              },
                {QStringLiteral("track_count"),              project.tracks.size()    },
                {QStringLiteral("empty_track_count"),        emptyTracks              },
                {QStringLiteral("singing_only_track_count"), singingOnlyTracks        },
                {QStringLiteral("audio_only_track_count"),   audioOnlyTracks          },
                {QStringLiteral("mixed_track_count"),        mixedTracks              },
                {QStringLiteral("clip_count"),               singingClips + audioClips},
                {QStringLiteral("singing_clip_count"),       singingClips             },
                {QStringLiteral("audio_clip_count"),         audioClips               },
            };
        }

        QJsonObject encodeDocument(const DocumentSnapshotDto &snapshot,
                                   const ProjectSnapshotDto &project) {
            return {
                {QStringLiteral("path"),          snapshot.path                                    },
                {QStringLiteral("project_name"),  snapshot.projectName                             },
                {QStringLiteral("busy"),          snapshot.busy                                    },
                {QStringLiteral("saved"),         snapshot.saved                                   },
                {QStringLiteral("dirty"),         !snapshot.saved                                  },
                {QStringLiteral("on_save_point"), snapshot.saved                                   },
                {QStringLiteral("lifecycle"),     encodePublicDocumentLifecycle(snapshot.lifecycle)},
                {QStringLiteral("statistics"),    encodeProjectStatistics(project)                 },
            };
        }

        QJsonObject encodeTrackSnapshot(const TrackSnapshotDto &track, const qsizetype index) {
            return {
                {QStringLiteral("track_id"),            track.id.value()          },
                {QStringLiteral("index"),               static_cast<qint64>(index)},
                {QStringLiteral("name"),                track.data.name           },
                {QStringLiteral("color_index"),         track.data.colorIndex     },
                {QStringLiteral("gain"),                track.data.gain           },
                {QStringLiteral("pan"),                 track.data.pan            },
                {QStringLiteral("mute"),                track.data.mute           },
                {QStringLiteral("solo"),                track.data.solo           },
                {QStringLiteral("default_language_id"), track.data.defaultLanguage},
                {QStringLiteral("clip_count"),          track.clips.size()        },
            };
        }

        QJsonObject encodeClipSnapshot(const ClipSnapshotDto &clip) {
            return {
                {QStringLiteral("clip_id"),             clip.id.value()                                       },
                {QStringLiteral("track_id"),            clip.trackId.value()                                  },
                {QStringLiteral("type"),                clip.data.type == ClipDraftDto::Type::Singing
                                             ? QStringLiteral("singing")
                                             : QStringLiteral("audio")},
                {QStringLiteral("name"),                clip.data.properties.name                             },
                {QStringLiteral("start"),               clip.data.properties.start                            },
                {QStringLiteral("length"),              clip.data.properties.length                           },
                {QStringLiteral("gain"),                clip.data.properties.gain                             },
                {QStringLiteral("mute"),                clip.data.properties.mute                             },
                {QStringLiteral("default_language_id"), clip.data.defaultLanguage                             },
            };
        }

        QJsonObject encodeVoiceContext(const SingerInfo &ownSinger, const SpeakerInfo &ownSpeaker,
                                       const SingerInfo &effectiveSinger,
                                       const SpeakerInfo &effectiveSpeaker,
                                       const bool inheritsTrack, const QString &defaultLanguage) {
            const bool ownAvailable = !ownSinger.isEmpty();
            const bool effectiveAvailable = !effectiveSinger.isEmpty();
            return {
                {QStringLiteral("own_voice"),
                 ownAvailable ? QJsonValue(encodeVoiceSelection(ownSinger, ownSpeaker))
                              : QJsonValue(QJsonValue::Null)},
                {QStringLiteral("effective_voice"),
                 effectiveAvailable
                     ? QJsonValue(encodeVoiceSelection(effectiveSinger, effectiveSpeaker))
                     : QJsonValue(QJsonValue::Null)},
                {QStringLiteral("inherits_track"), inheritsTrack},
                {QStringLiteral("default_language"),
                 defaultLanguage.isEmpty()
                     ? QJsonValue(QJsonValue::Null)
                     : QJsonValue(encodeLanguageSelection(defaultLanguage,
                 effectiveSinger.defaultLanguage()))},
                {QStringLiteral("available"), effectiveAvailable},
                {QStringLiteral("unavailable_reason"),
                 effectiveAvailable ? QString() : QStringLiteral("No voice is assigned")},
            };
        }

        QJsonObject encodeAudioClipSnapshot(const ClipSnapshotDto &clip,
                                            const QString &documentPath) {
            const auto &audio = clip.data.audioInfo;
            QJsonArray candidates;
            QSet<QString> candidateSet;
            const auto appendCandidate = [&](const QString &path) {
                if (path.isEmpty())
                    return;
                const auto cleanPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
                if (!candidateSet.contains(cleanPath)) {
                    candidateSet.insert(cleanPath);
                    candidates.append(cleanPath);
                }
            };
            QString status = QStringLiteral("resolved");
            if (clip.data.audioPathStatus == AudioClip::PathStatus::Missing)
                status = QStringLiteral("missing");
            else if (clip.data.audioPathStatus == AudioClip::PathStatus::Unconfirmed)
                status = QStringLiteral("candidate");
            if (clip.data.audioPathStatus == AudioClip::PathStatus::Unconfirmed)
                appendCandidate(clip.data.audioPath);
            const auto fileName = QFileInfo(clip.data.audioPath).fileName();
            if (!documentPath.isEmpty() && !fileName.isEmpty()) {
                const auto projectDir = QFileInfo(documentPath).absoluteDir();
                if (!clip.data.audioPathInfo.relativeDir.isEmpty()) {
                    appendCandidate(
                        projectDir.filePath(clip.data.audioPathInfo.relativeDir + u'/' + fileName));
                }
                appendCandidate(projectDir.filePath(fileName));
            }
            const auto duration =
                audio.sampleRate > 0
                    ? QJsonValue(static_cast<double>(audio.frames) / audio.sampleRate)
                    : QJsonValue(QJsonValue::Null);
            return {
                {QStringLiteral("clip_id"),          clip.id.value()                                                             },
                {QStringLiteral("path"),             clip.data.audioPath                                                         },
                {QStringLiteral("path_status"),      status                                                                      },
                {QStringLiteral("candidate_paths"),  candidates                                                                  },
                {QStringLiteral("hash_exists"),      !clip.data.audioPathInfo.sha512.isEmpty()                                   },
                {QStringLiteral("duration_seconds"), duration                                                                    },
                {QStringLiteral("sample_rate"),      audio.sampleRate > 0
                                                    ? QJsonValue(audio.sampleRate)
                                                    : QJsonValue(QJsonValue::Null)},
                {QStringLiteral("channels"),
                 audio.channels > 0 ? QJsonValue(audio.channels) : QJsonValue(QJsonValue::Null)                                  },
            };
        }

        QJsonObject encodeSpeakerMixTarget(const SpeakerMixTargetDto &target) {
            return {
                {QStringLiteral("type"), target.kind == SpeakerMixTargetKind::Track
                                             ? QStringLiteral("track")
                                             : QStringLiteral("clip")},
                {QStringLiteral("id"),   target.id                                                           },
            };
        }

        SpeakerMixTargetDto decodeSpeakerMixTarget(const QJsonObject &target) {
            return {
                target.value(QStringLiteral("type")).toString() == QStringLiteral("clip")
                    ? SpeakerMixTargetKind::Clip
                    : SpeakerMixTargetKind::Track,
                target.value(QStringLiteral("id")).toInt(-1),
            };
        }

        QJsonObject encodeSpeakerMix(const SpeakerMixSnapshotDto &snapshot) {
            const auto mix = SpeakerMixModel::normalizeSpeakerMixData(snapshot.mix);
            QList<SpeakerInfo> speakers;
            if (mix.sources.isEmpty()) {
                if (!snapshot.speaker.isEmpty())
                    speakers.append(snapshot.speaker);
            } else {
                for (const auto &source : mix.sources)
                    speakers.append(source.speaker);
            }
            QVector<double> weights;
            if (speakers.size() == 1) {
                weights.append(1.0);
            } else if (!mix.fixedWeights.isEmpty()) {
                weights = SpeakerMixModel::fullWeightsFromExplicitWeights(mix.fixedWeights);
            } else if (!mix.dynamicKeyframes.isEmpty()) {
                weights = SpeakerMixModel::fullWeightsFromExplicitWeights(
                    mix.dynamicKeyframes.constFirst().weights);
            }
            QJsonArray sources;
            for (qsizetype index = 0; index < speakers.size(); ++index) {
                sources.append(QJsonObject{
                    {QStringLiteral("speaker"),
                     QJsonObject{{QStringLiteral("speaker_id"), speakers.at(index).id()}}},
                    {QStringLiteral("weight"), weights.value(index, index == 0 ? 1.0 : 0.0)},
                });
            }
            QJsonArray keyframes;
            for (const auto &keyframe : mix.dynamicKeyframes) {
                QJsonArray encodedWeights;
                for (const auto weight :
                     SpeakerMixModel::fullWeightsFromExplicitWeights(keyframe.weights)) {
                    encodedWeights.append(weight);
                }
                keyframes.append(QJsonObject{
                    {QStringLiteral("keyframe_id"), keyframe.id   },
                    {QStringLiteral("position"),    keyframe.tick },
                    {QStringLiteral("weights"),     encodedWeights},
                });
            }
            QJsonValue sourcePreset = QJsonValue(QJsonValue::Null);
            if (!mix.sourcePresetId.isEmpty()) {
                sourcePreset = QJsonObject{
                    {QStringLiteral("preset_id"), mix.sourcePresetId   },
                    {QStringLiteral("name"),      mix.sourcePresetName },
                    {QStringLiteral("dirty"),     mix.sourcePresetDirty},
                };
            }
            return {
                {QStringLiteral("target"),          encodeSpeakerMixTarget(snapshot.target)},
                {QStringLiteral("mix"),
                 QJsonObject{{QStringLiteral("singer"), encodeSingerRef(snapshot.singer)},
                             {QStringLiteral("sources"), sources}}                         },
                {QStringLiteral("dynamic_enabled"),
                 mix.mode == SpeakerMixModel::SingerSourceMode::DynamicMix                 },
                {QStringLiteral("bypassed"),        mix.dynamicBypassed                    },
                {QStringLiteral("keyframes"),       keyframes                              },
                {QStringLiteral("source_preset"),   sourcePreset                           },
            };
        }

        QJsonObject encodeSpeakerMixPreset(const SpeakerMixPresetDto &preset) {
            const auto fullWeights =
                SpeakerMixModel::fullWeightsFromExplicitWeights(preset.fixedWeights);
            QJsonArray sources;
            for (qsizetype index = 0; index < preset.sources.size(); ++index) {
                const auto &source = preset.sources.at(index);
                sources.append(QJsonObject{
                    {QStringLiteral("speaker"),
                     QJsonObject{{QStringLiteral("speaker_id"), source.speakerId}}},
                    {QStringLiteral("speaker_name"), source.speakerName           },
                    {QStringLiteral("weight"),       fullWeights.value(index)     },
                });
            }
            return {
                {QStringLiteral("preset_id"),  preset.id                    },
                {QStringLiteral("name"),       preset.name                  },
                {QStringLiteral("singer"),
                 QJsonObject{{QStringLiteral("package_id"), preset.packageId},
                             {QStringLiteral("package_version"), preset.packageVersion.toString()},
                             {QStringLiteral("singer_id"), preset.singerId}}},
                {QStringLiteral("sources"),    sources                      },
                {QStringLiteral("created_at"),
                 preset.createdAt.isValid() ? preset.createdAt.toUTC().toString(Qt::ISODateWithMs)
                                            : QString()                     },
                {QStringLiteral("updated_at"),
                 preset.updatedAt.isValid() ? preset.updatedAt.toUTC().toString(Qt::ISODateWithMs)
                                            : QString()                     },
            };
        }

        AutomationResult<SpeakerMixPresetDto> decodeSpeakerMixPreset(CoreRuntime &runtime,
                                                                     const QJsonObject &object) {
            auto resolved = resolveSpeakerMix(runtime, object, QStringLiteral("preset"));
            if (!resolved)
                return resolved.getError();
            const auto &singer = resolved.get().first;
            const auto &mix = resolved.get().second;
            SpeakerMixPresetDto preset;
            preset.id = object.value(QStringLiteral("preset_id")).toString();
            preset.name = object.value(QStringLiteral("name")).toString();
            preset.packageId = singer.packageId();
            preset.singerId = singer.singerId();
            preset.packageVersion = singer.packageVersion();
            for (const auto &source : mix.sources) {
                preset.sources.append({source.speaker.id(), source.speaker.name()});
            }
            preset.fixedWeights = mix.fixedWeights;
            return preset;
        }

        AutomationResult<
            QPair<SpeakerMixPresetDto, QPair<SingerInfo, SpeakerMixModel::SpeakerMixData>>>
            resolveSpeakerMixPreset(CoreRuntime &runtime, const QString &presetId) {
            auto presets = runtime.presets().getSpeakerMixPresets();
            if (!presets)
                return presets.getError();
            const auto found = std::find_if(
                presets.get().cbegin(), presets.get().cend(),
                [&presetId](const SpeakerMixPresetDto &preset) { return preset.id == presetId; });
            if (found == presets.get().cend()) {
                return error(AutomationErrorCode::NotFound,
                             QStringLiteral("Speaker mix preset was not found"),
                             QStringLiteral("preset_id"));
            }
            QJsonArray sources;
            const auto fullWeights =
                SpeakerMixModel::fullWeightsFromExplicitWeights(found->fixedWeights);
            for (qsizetype index = 0; index < found->sources.size(); ++index) {
                sources.append(QJsonObject{
                    {QStringLiteral("speaker"), QJsonObject{{QStringLiteral("speaker_id"),
                                                             found->sources.at(index).speakerId}}},
                    {QStringLiteral("weight"),  fullWeights.value(index)                                                                        },
                });
            }
            auto resolved =
                resolveSpeakerMix(runtime,
                                  QJsonObject{
                                      {QStringLiteral("singer"),
                                       QJsonObject{{QStringLiteral("package_id"), found->packageId},
                                                   {QStringLiteral("package_version"),
                                                    found->packageVersion.toString()},
                                                   {QStringLiteral("singer_id"), found->singerId}}},
                                      {QStringLiteral("sources"), sources                         },
            },
                                  QStringLiteral("preset_id"));
            if (!resolved)
                return resolved.getError();
            resolved.get().second.sourcePresetId = found->id;
            resolved.get().second.sourcePresetName = found->name;
            resolved.get().second.sourcePresetDirty = false;
            return QPair<SpeakerMixPresetDto, QPair<SingerInfo, SpeakerMixModel::SpeakerMixData>>{
                *found, resolved.get()};
        }

        QString effectiveDefaultLanguage(const ProjectSnapshotDto &project, const ClipId clipId) {
            for (const auto &track : project.tracks) {
                for (const auto &clip : track.clips) {
                    if (clip.id != clipId)
                        continue;
                    if (!clip.data.defaultLanguage.isEmpty())
                        return clip.data.defaultLanguage;
                    const auto singer = clip.data.usesTrackVoiceContext ? track.data.singerInfo
                                                                        : clip.data.ownSingerInfo;
                    if (clip.data.usesTrackVoiceContext && !track.data.defaultLanguage.isEmpty()) {
                        return track.data.defaultLanguage;
                    }
                    return singer.defaultLanguage();
                }
            }
            return {};
        }

        AutomationResult<QJsonObject> encodeParameter(const ParameterSnapshotDto &snapshot,
                                                      const QJsonObject &arguments) {
            std::optional<QPair<int, int>> range;
            if (arguments.contains(QStringLiteral("range"))) {
                const auto encodedRange = arguments.value(QStringLiteral("range")).toObject();
                const auto start = encodedRange.value(QStringLiteral("start")).toInt();
                const auto end = encodedRange.value(QStringLiteral("end")).toInt();
                if (start < 0 || end <= start) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("range"),
                        QStringLiteral("Parameter query range must be non-empty and ordered"));
                }
                range = QPair<int, int>{start, end};
            }

            QList<CurveDraftDto> projected;
            qint64 sourcePointCount = 0;
            qint64 anchorPointCount = 0;
            for (const auto &curve : snapshot.curves) {
                auto result = curve;
                if (curve.type == CurveDraftDto::Type::Anchor) {
                    if (range) {
                        result.nodes.removeIf([&range](const AnchorNodeDraftDto &node) {
                            return node.position < range->first || node.position >= range->second;
                        });
                    }
                    if (result.nodes.isEmpty())
                        continue;
                    sourcePointCount += result.nodes.size();
                    anchorPointCount += result.nodes.size();
                } else {
                    QList<int> values;
                    int localStart = curve.localStart;
                    for (qsizetype index = 0; index < curve.values.size(); ++index) {
                        const auto position = static_cast<qint64>(curve.localStart) +
                                              static_cast<qint64>(curve.step) * index;
                        if (range && (position < range->first || position >= range->second)) {
                            continue;
                        }
                        if (values.isEmpty())
                            localStart = static_cast<int>(position);
                        values.append(curve.values.at(index));
                    }
                    if (values.isEmpty())
                        continue;
                    result.localStart = localStart;
                    result.values = std::move(values);
                    sourcePointCount += result.values.size();
                }
                projected.append(std::move(result));
            }

            const auto maximumPoints = arguments.value(QStringLiteral("max_points"))
                                           .toInt(AutomationWire::MaximumCurveSampleItems);
            if (anchorPointCount > maximumPoints) {
                return AutomationError::invalidArgument(
                    QStringLiteral("max_points"),
                    QStringLiteral("The bound is smaller than the exact anchor result; narrow the "
                                   "range or increase max_points"));
            }

            QList<qsizetype> drawIndexes;
            qint64 drawPointCount = 0;
            for (qsizetype index = 0; index < projected.size(); ++index) {
                if (projected.at(index).type != CurveDraftDto::Type::Draw)
                    continue;
                drawIndexes.append(index);
                drawPointCount += projected.at(index).values.size();
            }
            auto drawBudget = static_cast<qint64>(maximumPoints) - anchorPointCount;
            if (drawPointCount > drawBudget && drawBudget < drawIndexes.size()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("max_points"),
                    QStringLiteral("The bound cannot represent every matching draw curve; narrow "
                                   "the range or increase max_points"));
            }
            qint64 remainingBudget = drawBudget;
            qint64 remainingSource = drawPointCount;
            qsizetype remainingCurves = drawIndexes.size();
            for (const auto index : drawIndexes) {
                auto &curve = projected[index];
                const auto sourceSize = static_cast<qint64>(curve.values.size());
                auto quota = sourceSize;
                if (drawPointCount > drawBudget) {
                    const auto reservedForOthers = static_cast<qint64>(remainingCurves - 1);
                    const auto proportional =
                        remainingSource > 0 ? (remainingBudget * sourceSize) / remainingSource : 0;
                    quota =
                        std::clamp<qint64>(proportional, 1, remainingBudget - reservedForOthers);
                    quota = std::min(quota, sourceSize);
                }
                if (sourceSize > quota) {
                    const auto stride = (sourceSize + quota - 1) / quota;
                    const auto widenedStep = static_cast<qint64>(curve.step) * stride;
                    if (widenedStep > std::numeric_limits<int>::max()) {
                        return AutomationError::invalidArgument(
                            QStringLiteral("max_points"),
                            QStringLiteral(
                                "The requested bound cannot preserve a valid draw step"));
                    }
                    QList<int> values;
                    values.reserve(static_cast<qsizetype>(quota));
                    for (qint64 valueIndex = 0; valueIndex < sourceSize; valueIndex += stride)
                        values.append(curve.values.at(static_cast<qsizetype>(valueIndex)));
                    curve.step = static_cast<int>(widenedStep);
                    curve.values = std::move(values);
                }
                remainingBudget -= curve.values.size();
                remainingSource -= sourceSize;
                --remainingCurves;
            }

            QJsonArray curves;
            qint64 returnedPointCount = 0;
            for (const auto &curve : projected) {
                curves.append(encodeCurve(curve));
                returnedPointCount += curve.type == CurveDraftDto::Type::Anchor
                                          ? curve.nodes.size()
                                          : curve.values.size();
            }
            QJsonObject result{
                {QStringLiteral("clip_id"),              snapshot.clipId.value()              },
                {QStringLiteral("name"),                 parameterName(snapshot.name)         },
                {QStringLiteral("layer"),                parameterType(snapshot.type)         },
                {QStringLiteral("curves"),               curves                               },
                {QStringLiteral("source_point_count"),   sourcePointCount                     },
                {QStringLiteral("returned_point_count"), returnedPointCount                   },
                {QStringLiteral("downsampled"),          returnedPointCount < sourcePointCount},
            };
            if (range) {
                result.insert(QStringLiteral("range"), QJsonObject{
                                                           {QStringLiteral("start"), range->first },
                                                           {QStringLiteral("end"),   range->second},
                });
            }
            return result;
        }

        QJsonObject encodePlayback(const PlaybackSnapshotDto &snapshot) {
            const auto states = AutomationWire::publicStringValueDomainValues(
                AutomationWire::PublicValueDomain::PlaybackState);
            const auto state = states.value(static_cast<qsizetype>(snapshot.state));
            return {
                {QStringLiteral("state"),         state                },
                {QStringLiteral("playable"),      snapshot.playable    },
                {QStringLiteral("position"),      snapshot.position    },
                {QStringLiteral("last_position"), snapshot.lastPosition},
                {QStringLiteral("loop"),
                 QJsonObject{
                     {QStringLiteral("enabled"), snapshot.loop.enabled},
                     {QStringLiteral("start"), snapshot.loop.start},
                     {QStringLiteral("end"), snapshot.loop.end()},
                 }                                                                          },
            };
        }

        AutomationResult<QJsonObject>
            playbackStateMutationResult(CoreRuntime &runtime,
                                        AutomationResult<MutationResult> mutation) {
            if (!mutation)
                return mutation.getError();
            auto playback = runtime.playback().getPlayback(runtime.documentVersion().documentId);
            if (!playback)
                return playback.getError();
            QJsonArray warnings;
            for (const auto &warning : mutation.get().warnings)
                warnings.append(warning);
            return QJsonObject{
                {QStringLiteral("changed"),  mutation.get().changed        },
                {QStringLiteral("playback"), encodePlayback(playback.get())},
                {QStringLiteral("warnings"), warnings                      },
            };
        }

        AutomationResult<QJsonObject>
            playbackDocumentMutationResult(CoreRuntime &runtime,
                                           AutomationResult<MutationResult> mutation) {
            if (!mutation)
                return mutation.getError();
            auto playback = runtime.playback().getPlayback(runtime.documentVersion().documentId);
            if (!playback)
                return playback.getError();
            auto result = encodeMutation(mutation.get());
            result.insert(QStringLiteral("playback"), encodePlayback(playback.get()));
            return result;
        }

        AutomationResult<QJsonObject>
            validateFileArguments(AutomationFileGuard &guard,
                                  const AutomationWire::ToolContract &contract,
                                  const QJsonObject &arguments) {
            static const auto itemValidationErrorKey =
                QStringLiteral("__automation_item_validation_error");
            const auto encodeItemValidationError = [](const AutomationError &failure) {
                return QJsonObject{
                    {QStringLiteral("code"),       static_cast<int>(failure.code)},
                    {QStringLiteral("message"),    failure.message               },
                    {QStringLiteral("field_path"), failure.fieldPath             },
                };
            };
            auto result = arguments;
            const auto access = contract.fileAccess;
            if (access == AutomationWire::FileAccess::None)
                return result;
            const auto purpose = access == AutomationWire::FileAccess::Write
                                     ? FileAccessPurpose::Write
                                     : FileAccessPurpose::Read;
            if (arguments.contains(QStringLiteral("path"))) {
                auto authorized =
                    guard.authorize(arguments.value(QStringLiteral("path")).toString(), purpose);
                if (!authorized)
                    return authorized.getError();
                result.insert(QStringLiteral("path"), authorized.get().canonicalPath);
            }
            if (arguments.contains(QStringLiteral("items"))) {
                QJsonArray items;
                const auto bestEffort =
                    arguments.value(QStringLiteral("failure_policy")).toString() ==
                        QStringLiteral("best_effort") &&
                    (contract.operationId == OperationIds::documents::import_batch ||
                     contract.operationId == OperationIds::audio_clips::import_batch);
                const auto inputItems = arguments.value(QStringLiteral("items")).toArray();
                for (qsizetype index = 0; index < inputItems.size(); ++index) {
                    const auto &value = inputItems.at(index);
                    auto item = value.toObject();
                    auto authorized =
                        guard.authorize(item.value(QStringLiteral("path")).toString(), purpose);
                    if (!authorized && !bestEffort)
                        return authorized.getError();
                    if (!authorized) {
                        auto failure = authorized.getError();
                        failure.fieldPath = QStringLiteral("items[%1].path").arg(index);
                        item.insert(itemValidationErrorKey, encodeItemValidationError(failure));
                        item.insert(QStringLiteral("path"), QString());
                    } else {
                        item.insert(QStringLiteral("path"), authorized.get().canonicalPath);
                    }
                    items.append(item);
                }
                result.insert(QStringLiteral("items"), items);
            }
            return result;
        }

        std::optional<AutomationError> itemValidationError(const QJsonObject &item) {
            const auto encoded =
                item.value(QStringLiteral("__automation_item_validation_error")).toObject();
            if (encoded.isEmpty())
                return std::nullopt;
            AutomationError failure;
            failure.code =
                static_cast<AutomationErrorCode>(encoded.value(QStringLiteral("code")).toInt());
            failure.message = encoded.value(QStringLiteral("message")).toString();
            failure.fieldPath = encoded.value(QStringLiteral("field_path")).toString();
            return failure;
        }

        AutomationResult<PublicPreparedAudioPath>
            prepareAuthorizedAudioPath(PublicAutomationHostServices &services,
                                       AutomationFileGuard &guard, const QString &canonicalPath) {
            if (!services.prepareAudioPath) {
                return unavailable(QStringLiteral("Audio path preparation service is unavailable"));
            }
            const AuthorizedPath authorizedPath{canonicalPath, FileAccessPurpose::Read};
            auto authorized = guard.reauthorize(authorizedPath);
            if (!authorized)
                return authorized.getError();
            auto prepared = services.prepareAudioPath(canonicalPath);
            if (!prepared)
                return prepared.getError();
            if (prepared.get().sha512.trimmed().isEmpty()) {
                return error(AutomationErrorCode::IoError,
                             QStringLiteral("Audio path preparation did not produce a hash"),
                             QStringLiteral("path"));
            }
            authorized = guard.reauthorize(authorizedPath);
            if (!authorized)
                return authorized.getError();
            return prepared;
        }

        std::function<AutomationResult<AutomationUnit>(const QString &)>
            sourcePathAuthorizer(AutomationFileGuard &guard) {
            auto frozen = std::make_shared<std::optional<AuthorizedPath>>();
            return [&guard, frozen](const QString &path) -> AutomationResult<AutomationUnit> {
                if (!*frozen) {
                    auto authorized = guard.authorize(path, FileAccessPurpose::Read);
                    if (!authorized)
                        return authorized.getError();
                    *frozen = authorized.get();
                    return AutomationUnit{};
                }
                auto authorized = guard.reauthorize(**frozen);
                if (!authorized)
                    return authorized.getError();
                return AutomationUnit{};
            };
        }
    }

    PublicAutomationRegistry::PublicAutomationRegistry(CoreRuntime &runtime,
                                                       AutomationAccessPolicy &accessPolicy,
                                                       AutomationFileGuard &fileGuard,
                                                       AdmissionController &admissionController,
                                                       PublicAutomationHostServices hostServices)
        : m_runtime(runtime), m_accessPolicy(accessPolicy), m_fileGuard(fileGuard),
          m_admissionController(admissionController), m_hostServices(std::move(hostServices)) {
        if (m_hostServices.editorInstanceId.isNull())
            m_hostServices.editorInstanceId = QUuid::createUuid();
        registerBindings();
    }

    PublicAutomationRegistry::~PublicAutomationRegistry() {
        m_lifetimeState->active.store(false, std::memory_order_release);
    }

    const QList<AutomationWire::ToolContract> &PublicAutomationRegistry::contracts() const {
        return AutomationWire::publicToolContracts();
    }

    QStringList PublicAutomationRegistry::bindingIds() const {
        auto result = m_handlers.keys();
        std::sort(result.begin(), result.end());
        return result;
    }

    QList<AutomationWire::ToolContract> PublicAutomationRegistry::enabledContracts() const {
        return m_accessPolicy.enabledContracts();
    }

    bool PublicAutomationRegistry::isComplete() const {
        auto expected = AutomationWire::publicToolIds();
        auto actual = bindingIds();
        std::sort(expected.begin(), expected.end());
        return actual == expected;
    }

    void PublicAutomationRegistry::addBinding(const QLatin1StringView toolName, Handler handler) {
        const QString operationId(toolName);
        const auto *contract = AutomationWire::findPublicTool(operationId);
        Q_ASSERT(contract);
        Q_ASSERT(!m_handlers.contains(operationId));
        m_handlers.insert(operationId, std::move(handler));
    }

    AutomationResult<QJsonObject>
        PublicAutomationRegistry::invoke(const QString &operationId, const QJsonObject &arguments,
                                         const PublicInvocationContext &context) {
        const auto *contract = AutomationWire::findPublicTool(operationId);
        const auto handler = m_handlers.constFind(operationId);
        if (!contract || handler == m_handlers.cend()) {
            auto result = error(AutomationErrorCode::OperationUnavailable,
                                QStringLiteral("Public operation is not registered"),
                                QStringLiteral("operation_id"));
            result.operationId = operationId;
            return result;
        }
        if (!m_accessPolicy.isAllowed(*contract)) {
            auto result =
                error(AutomationErrorCode::PermissionDenied,
                      QStringLiteral("Public operation is disabled by the active profile"));
            result.operationId = operationId;
            return result;
        }
        const auto inputValidation =
            AutomationWire::validateJsonValue(arguments, contract->inputSchema);
        if (!inputValidation.valid()) {
            const auto &issue = inputValidation.issues.constFirst();
            auto result =
                error(AutomationErrorCode::InvalidArgument, issue.message, issue.instancePath);
            result.operationId = operationId;
            return result;
        }
        auto effectiveArguments = arguments;
        const auto saveCurrent = operationId == OperationIds::documents::save &&
                                 !effectiveArguments.contains(QStringLiteral("path"));
        if (saveCurrent) {
            auto document = m_runtime.documents().getDocument(documentId(effectiveArguments));
            if (!document) {
                auto result = document.getError();
                result.operationId = operationId;
                return result;
            }
            if (document.get().path.trimmed().isEmpty()) {
                AutomationError result;
                result.code = AutomationErrorCode::PathRequired;
                result.fieldPath = QStringLiteral("path");
                result.message = QStringLiteral("The current document does not have a save path");
                result.operationId = operationId;
                return result;
            }
            effectiveArguments.insert(QStringLiteral("path"), document.get().path);
            effectiveArguments.insert(QStringLiteral("overwrite_policy"),
                                      QStringLiteral("overwrite"));
        }
        auto authorized = validateFileArguments(m_fileGuard, *contract, effectiveArguments);
        if (!authorized) {
            auto result = authorized.getError();
            result.operationId = operationId;
            return result;
        }
        if (operationId.startsWith(QStringLiteral("inference.")) &&
            effectiveArguments.contains(QStringLiteral("scope"))) {
            auto scope = validateInferenceScope(
                m_runtime, documentId(effectiveArguments),
                effectiveArguments.value(QStringLiteral("scope")).toObject());
            if (!scope) {
                auto result = scope.getError();
                result.operationId = operationId;
                return result;
            }
        }
        auto lease = m_admissionController.tryAcquire(contract->syncMode ==
                                                      AutomationWire::SyncMode::Asynchronous);
        if (!lease) {
            auto result = lease.getError();
            result.operationId = operationId;
            return result;
        }
        auto result = handler.value()(authorized.get(), context);
        if (!result) {
            auto decorated = result.getError();
            decorated.operationId = operationId;
            return decorated;
        }
        if (contract->syncMode == AutomationWire::SyncMode::Asynchronous) {
            const auto taskId =
                TaskId::fromString(result.get().value(QStringLiteral("task_id")).toString());
            if (!taskId.isNull()) {
                auto retainedLease = std::make_shared<AdmissionLease>(lease.get());
                m_runtime.automationTasks().setTerminalCallback(
                    taskId, [retainedLease](const AutomationTaskSnapshot &) {});
            }
        }
        return result;
    }

    void PublicAutomationRegistry::registerBindings() {
        addBinding(ToolNames::application_get_info,
                   [this](const QJsonObject &, const PublicInvocationContext &) {
                       auto result = m_runtime.application().getInfo();
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(QJsonObject{
                           {QStringLiteral("name"),     result.get().name    },
                           {QStringLiteral("version"),  result.get().version },
                           {QStringLiteral("platform"), result.get().platform},
                           {QStringLiteral("build_id"), result.get().buildId },
                       });
                   });
        addBinding(ToolNames::application_get_status, [this](const QJsonObject &,
                                                             const PublicInvocationContext &) {
            const auto policy = m_accessPolicy.snapshot();
            QJsonArray documents;
            if (m_hostServices.documentStatus) {
                documents = m_hostServices.documentStatus();
            } else {
                const auto version = m_runtime.documentVersion();
                if (!version.documentId.isNull())
                    documents.append(encodeDocumentVersion(version));
            }
            while (documents.size() > 1)
                documents.removeLast();
            QJsonArray windows;
            if (m_hostServices.windowStatus) {
                windows = m_hostServices.windowStatus();
            } else {
                const auto version = m_runtime.documentVersion();
                if (!m_runtime.windowId().isNull() && !version.documentId.isNull()) {
                    windows.append(QJsonObject{
                        {QStringLiteral("window_id"),   m_runtime.windowId().toString()},
                        {QStringLiteral("document_id"), version.documentId.toString()  },
                    });
                }
            }
            while (windows.size() > 1)
                windows.removeLast();
            return AutomationResult<QJsonObject>(QJsonObject{
                {QStringLiteral("editor_instance_id"),
                 m_hostServices.editorInstanceId.toString(QUuid::WithoutBraces)                             },
                {QStringLiteral("host_mode"),          m_hostServices.hostMode                              },
                {QStringLiteral("profile"),            AutomationWire::automationProfileName(policy.profile)},
                {QStringLiteral("toolset_version"),
                 static_cast<qint64>(AutomationWire::PublicToolsetVersion)                                  },
                {QStringLiteral("documents"),          documents                                            },
                {QStringLiteral("windows"),            windows                                              },
            });
        });
        const auto addTerminationBinding = [this](const QLatin1StringView toolName,
                                                  const ApplicationTerminationMode mode,
                                                  const QString &action) {
            addBinding(toolName,
                       [this, mode, action](const QJsonObject &arguments,
                                            const PublicInvocationContext &invocation) {
                           const bool discardChanges =
                               arguments.value(QStringLiteral("discard_changes")).toBool(false);
                           const GuiCommandContext context{
                               .windowId = m_runtime.windowId(),
                               .source = InvocationSource::PublicMcp,
                               .clientId = invocation.clientId,
                           };
                           auto result = m_runtime.application().requestTermination(
                               context, mode, discardChanges);
                           if (!result)
                               return AutomationResult<QJsonObject>(result.getError());
                           return AutomationResult<QJsonObject>(QJsonObject{
                               {QStringLiteral("accepted"),        true          },
                               {QStringLiteral("action"),          action        },
                               {QStringLiteral("discard_changes"), discardChanges},
                           });
                       });
        };
        addTerminationBinding(ToolNames::application_request_exit,
                              ApplicationTerminationMode::Exit, QStringLiteral("exit"));
        addTerminationBinding(ToolNames::application_request_restart,
                              ApplicationTerminationMode::Restart, QStringLiteral("restart"));
        addBinding(ToolNames::documents_get,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto result = m_runtime.documents().getDocument(documentId(arguments));
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       auto project = m_runtime.project().getProject(documentId(arguments));
                       if (!project)
                           return AutomationResult<QJsonObject>(project.getError());
                       return AutomationResult<QJsonObject>(
                           queryResult(result.get().document, QStringLiteral("snapshot"),
                                       encodeDocument(result.get(), project.get())));
                   });
        addBinding(ToolNames::documents_list_recent,
                   [this](const QJsonObject &, const PublicInvocationContext &) {
                       auto result = m_runtime.settings().getRecentProjectFiles();
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       QJsonArray projects;
                       for (const auto &path : result.get()) {
                           const QFileInfo info(path);
                           projects.append(QJsonObject{
                               {QStringLiteral("path"),      QDir::cleanPath(path)},
                               {QStringLiteral("file_name"), info.fileName()      },
                               {QStringLiteral("exists"),    info.exists()        },
                           });
                       }
                       return AutomationResult<QJsonObject>(QJsonObject{
                           {QStringLiteral("projects"), projects}
                       });
                   });
        addBinding(ToolNames::notes_list, [this](const QJsonObject &arguments,
                                                 const PublicInvocationContext &) {
            auto result = m_runtime.notes().getNotes(
                documentId(arguments), ClipId(arguments.value(QStringLiteral("clip_id")).toInt()));
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            QJsonArray notes;
            for (const auto &note : result.get()) {
                auto encoded = encodeNoteDraft(note.data);
                encoded.insert(QStringLiteral("note_id"), note.id.value());
                encoded.insert(QStringLiteral("clip_id"), note.clipId.value());
                notes.append(encoded);
            }
            auto page = paginateJson(
                m_collectionCursorCodec, notes, arguments, QStringLiteral("editor-public-notes/v1"),
                QJsonObject{
                    {QStringLiteral("document"),
                     encodeDocumentVersion(m_runtime.documentVersion())                    },
                    {QStringLiteral("clip_id"),  arguments.value(QStringLiteral("clip_id"))},
                    {QStringLiteral("notes"),    notes                                     }
            });
            if (!page)
                return AutomationResult<QJsonObject>(page.getError());
            auto encoded =
                queryResult(m_runtime.documentVersion(), QStringLiteral("notes"), page.get().items);
            if (!page.get().nextCursor.isEmpty())
                encoded.insert(QStringLiteral("next_cursor"), page.get().nextCursor);
            return AutomationResult<QJsonObject>(std::move(encoded));
        });
        addBinding(ToolNames::parameters_get, [this](const QJsonObject &arguments,
                                                     const PublicInvocationContext &) {
            auto result = m_runtime.parameters().getParameter(
                documentId(arguments), ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                parameterName(arguments.value(QStringLiteral("name")).toString()),
                parameterType(arguments.value(QStringLiteral("layer")).toString()));
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            auto encoded = encodeParameter(result.get(), arguments);
            if (!encoded)
                return AutomationResult<QJsonObject>(encoded.getError());
            return AutomationResult<QJsonObject>(
                queryResult(result.get().document, QStringLiteral("snapshot"), encoded.get()));
        });
        addBinding(ToolNames::parameters_get_capabilities, [this](const QJsonObject &arguments,
                                                                  const PublicInvocationContext &) {
            const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
            auto result = m_runtime.parameters().getCapabilities(documentId(arguments), clipId);
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            QJsonArray parameters;
            for (const auto &capability : result.get().parameters) {
                QJsonArray layers;
                for (const auto type : capability.types)
                    layers.append(parameterType(type));
                QJsonArray curveTypes;
                if (capability.supportsDraw)
                    curveTypes.append(QStringLiteral("draw"));
                if (capability.supportsAnchor)
                    curveTypes.append(QStringLiteral("anchor"));
                QJsonArray interpolations;
                for (const auto mode : capability.interpolations)
                    interpolations.append(interpolation(mode));
                parameters.append(QJsonObject{
                    {QStringLiteral("name"),           parameterName(capability.name)},
                    {QStringLiteral("layers"),         layers                        },
                    {QStringLiteral("curve_types"),    curveTypes                    },
                    {QStringLiteral("interpolations"), interpolations                },
                    {QStringLiteral("editable"),       capability.editable           },
                    {QStringLiteral("range"),
                     QJsonObject{
                         {QStringLiteral("minimum"), capability.valueSpec.minimum},
                         {QStringLiteral("maximum"), capability.valueSpec.maximum},
                         {QStringLiteral("step"), capability.valueSpec.step},
                         {QStringLiteral("unit"), capability.valueSpec.unit},
                     }                                                               },
                });
            }
            return AutomationResult<QJsonObject>(
                queryResult(result.get().document, QStringLiteral("capabilities"),
                            QJsonObject{
                                {QStringLiteral("clip_id"),    clipId.value()},
                                {QStringLiteral("parameters"), parameters    }
            }));
        });
        addBinding(ToolNames::timeline_get, [this](const QJsonObject &arguments,
                                                   const PublicInvocationContext &) {
            auto result = m_runtime.timeline().getTimeline(documentId(arguments));
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            QJsonArray tempos;
            for (const auto &tempo : result.get().tempos) {
                tempos.append(QJsonObject{
                    {QStringLiteral("tick"),  tempo.pos  },
                    {QStringLiteral("tempo"), tempo.value}
                });
            }
            QJsonArray signatures;
            for (const auto &signature : result.get().timeSignatures) {
                signatures.append(QJsonObject{
                    {QStringLiteral("bar_index"),   signature.barIndex   },
                    {QStringLiteral("numerator"),   signature.numerator  },
                    {QStringLiteral("denominator"), signature.denominator},
                });
            }
            return AutomationResult<QJsonObject>(
                queryResult(result.get().document, QStringLiteral("snapshot"),
                            QJsonObject{
                                {QStringLiteral("tempos"),          tempos    },
                                {QStringLiteral("time_signatures"), signatures}
            }));
        });
        addBinding(ToolNames::history_get_state,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto result = m_runtime.history().getState(documentId(arguments));
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(queryResult(
                           result.get().document, QStringLiteral("snapshot"),
                           QJsonObject{
                               {QStringLiteral("can_undo"),      result.get().canUndo    },
                               {QStringLiteral("can_redo"),      result.get().canRedo    },
                               {QStringLiteral("on_save_point"), result.get().onSavePoint},
                               {QStringLiteral("undo_name"),     result.get().undoName   },
                               {QStringLiteral("redo_name"),     result.get().redoName   },
                       }));
                   });
        addBinding(ToolNames::voices_list, [this](const QJsonObject &arguments,
                                                  const PublicInvocationContext &) {
            auto result = m_runtime.packages().getInstalledPackages();
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            const auto query = arguments.value(QStringLiteral("query")).toString();
            const auto packageFilter = arguments.value(QStringLiteral("package_id")).toString();
            QJsonArray voices;
            for (const auto &package : result.get()) {
                if (!packageFilter.isEmpty() && package.id != packageFilter)
                    continue;
                for (const auto &singer : package.singers) {
                    if (!query.isEmpty() && !singer.name.contains(query, Qt::CaseInsensitive) &&
                        !singer.singerId.contains(query, Qt::CaseInsensitive)) {
                        continue;
                    }
                    voices.append(QJsonObject{
                        {QStringLiteral("package_id"),      singer.packageId                },
                        {QStringLiteral("package_version"), singer.packageVersion.toString()},
                        {QStringLiteral("singer_id"),       singer.singerId                 },
                        {QStringLiteral("name"),            singer.name                     },
                    });
                }
            }
            auto page = paginateJson(m_collectionCursorCodec, voices, arguments,
                                     QStringLiteral("editor-public-voices/v1"),
                                     QJsonObject{
                                         {QStringLiteral("query"),      query        },
                                         {QStringLiteral("package_id"), packageFilter},
                                         {QStringLiteral("singers"),    voices       }
            });
            if (!page)
                return AutomationResult<QJsonObject>(page.getError());
            QJsonObject encoded{
                {QStringLiteral("singers"), page.get().items}
            };
            if (!page.get().nextCursor.isEmpty())
                encoded.insert(QStringLiteral("next_cursor"), page.get().nextCursor);
            return AutomationResult<QJsonObject>(std::move(encoded));
        });
        addBinding(ToolNames::voices_describe, [this](const QJsonObject &arguments,
                                                      const PublicInvocationContext &) {
            const auto voice = arguments.value(QStringLiteral("singer")).toObject();
            const auto singerId = voice.value(QStringLiteral("singer_id")).toString();
            const auto packageId = voice.value(QStringLiteral("package_id")).toString();
            auto packageVersion =
                parsePackageVersion(voice, QStringLiteral("singer.package_version"));
            if (!packageVersion)
                return AutomationResult<QJsonObject>(packageVersion.getError());
            auto result = m_runtime.packages().getInstalledPackages();
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            for (const auto &package : result.get()) {
                for (const auto &singer : package.singers) {
                    if (singer.singerId == singerId && singer.packageId == packageId &&
                        singer.packageVersion == packageVersion.get()) {
                        const auto resolutionStates = AutomationWire::publicStringValueDomainValues(
                            AutomationWire::PublicValueDomain::VoiceResolutionState);
                        const auto resolutionState = singer.info.resolutionState();
                        const bool resolved = resolutionState == ResolutionState::Resolved;
                        const auto defaultLanguage = singer.info.defaultLanguage();
                        QJsonArray languageIds;
                        QJsonArray languages;
                        bool defaultG2pReady = false;
                        for (const auto &language : singer.info.languages()) {
                            const auto languageId = language.id();
                            const auto g2pId = language.g2p();
                            const bool g2pReady =
                                resolved && !g2pId.isEmpty() && g2pId != QStringLiteral("unknown");
                            languageIds.append(languageId);
                            languages.append(QJsonObject{
                                {QStringLiteral("language_id"), languageId                   },
                                {QStringLiteral("name"),        language.name()              },
                                {QStringLiteral("g2p_id"),      g2pId                        },
                                {QStringLiteral("g2p_ready"),   g2pReady                     },
                                {QStringLiteral("default"),     languageId == defaultLanguage},
                            });
                            if (languageId == defaultLanguage)
                                defaultG2pReady = g2pReady;
                        }
                        QJsonArray speakers;
                        const auto speakerInfos = singer.info.speakers();
                        const QString defaultSpeaker =
                            speakerInfos.isEmpty() ? QString() : speakerInfos.constFirst().id();
                        for (const auto &speaker : speakerInfos) {
                            speakers.append(QJsonObject{
                                {QStringLiteral("speaker_id"), speaker.id()                  },
                                {QStringLiteral("name"),       speaker.name()                },
                                {QStringLiteral("languages"),  languageIds                   },
                                {QStringLiteral("mixable"),    speaker.mixable()             },
                                {QStringLiteral("default"),    speaker.id() == defaultSpeaker},
                            });
                        }
                        const auto &capability = singer.info.capability();
                        const bool mixingSupported =
                            capability && capability->mixableSpeakers.size() > 1;
                        return AutomationResult<QJsonObject>(QJsonObject{
                            {QStringLiteral("snapshot"),
                             QJsonObject{
                                 {QStringLiteral("package_id"), singer.packageId},
                                 {QStringLiteral("package_version"),
                                  singer.packageVersion.toString()},
                                 {QStringLiteral("singer_id"), singer.singerId},
                                 {QStringLiteral("name"), singer.name},
                                 {QStringLiteral("speakers"), speakers},
                                 {QStringLiteral("languages"), languages},
                                 {QStringLiteral("default_speaker_id"),
                                  defaultSpeaker.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                           : QJsonValue(defaultSpeaker)},
                                 {QStringLiteral("default_language"),
                                  defaultLanguage.isEmpty() ? QJsonValue(QJsonValue::Null)
                                                            : QJsonValue(defaultLanguage)},
                                 {QStringLiteral("g2p_ready"), defaultG2pReady},
                                 {QStringLiteral("resolution_state"),
                                  resolutionStates.value(static_cast<qsizetype>(resolutionState))},
                                 {QStringLiteral("mixing_supported"), mixingSupported},
                             }},
                        });
                    }
                }
            }
            return AutomationResult<QJsonObject>(error(AutomationErrorCode::NotFound,
                                                       QStringLiteral("Singer was not found"),
                                                       QStringLiteral("singer")));
        });
        addBinding(ToolNames::tracks_insert, [this](const QJsonObject &arguments,
                                                    const PublicInvocationContext &invocation) {
            QString defaultLanguage = QStringLiteral("unknown");
            const auto settings = m_runtime.settings().getSettings();
            if (settings && !settings.get().general.defaultSingingLanguage.isEmpty())
                defaultLanguage = settings.get().general.defaultSingingLanguage;
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            QList<int> effectiveColors;
            effectiveColors.reserve(project.get().tracks.size());
            for (const auto &track : project.get().tracks)
                effectiveColors.append(track.data.colorIndex);
            const auto insertionIndex = arguments.value(QStringLiteral("index")).toInteger();
            const bool canResolveColors =
                insertionIndex >= 0 && insertionIndex <= effectiveColors.size();
            QList<TrackDraftDto> tracks;
            QList<ResolvedValue> resolvedValues;
            const auto inputTracks = arguments.value(QStringLiteral("tracks")).toArray();
            tracks.reserve(inputTracks.size());
            for (qsizetype index = 0; index < inputTracks.size(); ++index) {
                auto object = inputTracks.at(index).toObject();
                const auto path = QStringLiteral("/tracks/%1/").arg(index);
                if (!object.contains(QStringLiteral("name"))) {
                    const auto name = QCoreApplication::translate("TrackController", "New Track");
                    object.insert(QStringLiteral("name"), name);
                    resolvedValues.append({path + QStringLiteral("name"), name});
                }
                if (!object.contains(QStringLiteral("color_index"))) {
                    int colorIndex = 0;
                    if (canResolveColors) {
                        const auto targetIndex = insertionIndex + index;
                        const auto previousColor =
                            targetIndex > 0 ? effectiveColors.at(targetIndex - 1) : -1;
                        colorIndex = previousColor < 0 ? 0
                                                       : (previousColor + 1) %
                                                             AutomationWire::TrackPaletteColorCount;
                    }
                    object.insert(QStringLiteral("color_index"), colorIndex);
                    resolvedValues.append({path + QStringLiteral("color_index"), colorIndex});
                }
                if (canResolveColors) {
                    const auto targetIndex = insertionIndex + index;
                    effectiveColors.insert(targetIndex,
                                           object.value(QStringLiteral("color_index")).toInt());
                }
                object.insert(QStringLiteral("default_language"), defaultLanguage);
                auto track = decodeTrack(object);
                track.resolveColorIndex = false;
                tracks.append(std::move(track));
            }
            auto result = m_runtime.project().insertTracks(
                commandContext(arguments, invocation),
                arguments.value(QStringLiteral("index")).toInteger(), tracks);
            if (result)
                result.get().resolvedValues = std::move(resolvedValues);
            return mutationResult(std::move(result));
        });
        addBinding(ToolNames::tracks_remove, [this](const QJsonObject &arguments,
                                                    const PublicInvocationContext &invocation) {
            return mutationResult(m_runtime.project().removeTracks(
                commandContext(arguments, invocation),
                objectIds<TrackId>(arguments.value(QStringLiteral("track_ids")).toArray())));
        });
        addBinding(ToolNames::tracks_move, [this](const QJsonObject &arguments,
                                                  const PublicInvocationContext &invocation) {
            return mutationResult(m_runtime.project().moveTracks(
                commandContext(arguments, invocation),
                objectIds<TrackId>(arguments.value(QStringLiteral("track_ids")).toArray()),
                arguments.value(QStringLiteral("target_index")).toInteger()));
        });
        addBinding(ToolNames::tracks_rename,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().renameTrack(
                           commandContext(arguments, invocation),
                           TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                           arguments.value(QStringLiteral("name")).toString()));
                   });
        addBinding(ToolNames::tracks_set_color,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().setTrackColor(
                           commandContext(arguments, invocation),
                           TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                           arguments.value(QStringLiteral("color_index")).toInt()));
                   });
        addBinding(ToolNames::tracks_set_default_language,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().setTrackDefaultLanguage(
                           commandContext(arguments, invocation),
                           TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                           arguments.value(QStringLiteral("language_id")).toString()));
                   });
        addBinding(ToolNames::clips_insert, [this](const QJsonObject &arguments,
                                                   const PublicInvocationContext &invocation) {
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            auto timelineSnapshot = m_runtime.timeline().getTimeline(documentId(arguments));
            if (!timelineSnapshot)
                return AutomationResult<QJsonObject>(timelineSnapshot.getError());
            const Timeline timeline(timelineSnapshot.get().tempos,
                                    timelineSnapshot.get().timeSignatures);
            QHash<int, QString> trackLanguages;
            for (const auto &track : project.get().tracks)
                trackLanguages.insert(track.id.value(), track.data.defaultLanguage);
            QList<ClipInsertDto> clips;
            QList<ResolvedValue> resolvedValues;
            const auto inputClips = arguments.value(QStringLiteral("clips")).toArray();
            clips.reserve(inputClips.size());
            for (qsizetype index = 0; index < inputClips.size(); ++index) {
                auto object = inputClips.at(index).toObject();
                const auto path = QStringLiteral("/clips/%1/").arg(index);
                if (!object.contains(QStringLiteral("name"))) {
                    const auto name =
                        QCoreApplication::translate("TrackController", "New Singing Clip");
                    object.insert(QStringLiteral("name"), name);
                    resolvedValues.append({path + QStringLiteral("name"), name});
                }
                if (!object.contains(QStringLiteral("length"))) {
                    constexpr int bars = 4;
                    const int start = object.value(QStringLiteral("start")).toInt();
                    const int startBar = timeline.tickToTime(qMax(0, start)).measure;
                    const int length =
                        timeline.barToTick(startBar + bars) - timeline.barToTick(startBar);
                    object.insert(QStringLiteral("length"), length);
                    resolvedValues.append({path + QStringLiteral("length"), length});
                }
                const auto trackId = TrackId(object.value(QStringLiteral("track_id")).toInt());
                object.insert(QStringLiteral("default_language"),
                              trackLanguages.value(trackId.value()));
                clips.append({trackId, decodeClip(object)});
            }
            auto result =
                m_runtime.project().insertClips(commandContext(arguments, invocation), clips);
            if (result)
                result.get().resolvedValues = std::move(resolvedValues);
            return mutationResult(std::move(result));
        });
        addBinding(ToolNames::clips_remove, [this](const QJsonObject &arguments,
                                                   const PublicInvocationContext &invocation) {
            return mutationResult(m_runtime.project().removeClips(
                commandContext(arguments, invocation),
                objectIds<ClipId>(arguments.value(QStringLiteral("clip_ids")).toArray())));
        });
        addBinding(ToolNames::clips_rename,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().renameClip(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           arguments.value(QStringLiteral("name")).toString()));
                   });
        addBinding(ToolNames::clips_set_default_language,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().setSingingClipDefaultLanguage(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           arguments.value(QStringLiteral("language_id")).toString()));
                   });
        addBinding(ToolNames::notes_insert, [this](const QJsonObject &arguments,
                                                   const PublicInvocationContext &invocation) {
            const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
            QString effectiveDefaultLanguage;
            QString defaultLyric = QStringLiteral("la");
            auto settings = m_runtime.settings().getSettings();
            if (settings)
                effectiveDefaultLanguage = settings.get().general.defaultSingingLanguage;
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            for (const auto &track : project.get().tracks) {
                for (const auto &clip : track.clips) {
                    if (clip.id != clipId)
                        continue;
                    if (!clip.data.defaultLanguage.isEmpty())
                        effectiveDefaultLanguage = clip.data.defaultLanguage;
                    else if (clip.data.usesTrackVoiceContext) {
                        if (!track.data.defaultLanguage.isEmpty())
                            effectiveDefaultLanguage = track.data.defaultLanguage;
                        else if (!track.data.singerInfo.defaultLanguage().isEmpty())
                            effectiveDefaultLanguage = track.data.singerInfo.defaultLanguage();
                    } else if (!clip.data.ownSingerInfo.defaultLanguage().isEmpty()) {
                        effectiveDefaultLanguage = clip.data.ownSingerInfo.defaultLanguage();
                    }
                }
            }
            if (settings)
                defaultLyric = settings.get().general.defaultLyrics.value(effectiveDefaultLanguage,
                                                                          QStringLiteral("la"));
            QList<NoteDraftDto> notes;
            QList<ResolvedValue> resolvedValues;
            const auto inputNotes = arguments.value(QStringLiteral("notes")).toArray();
            notes.reserve(inputNotes.size());
            for (qsizetype index = 0; index < inputNotes.size(); ++index) {
                const auto object = inputNotes.at(index).toObject();
                if (object.contains(QStringLiteral("phonemes"))) {
                    bool anyOffset = false;
                    bool allOffsets = true;
                    for (const auto &phoneme : object.value(QStringLiteral("phonemes")).toArray()) {
                        const auto hasOffset =
                            phoneme.toObject().contains(QStringLiteral("offset"));
                        anyOffset = anyOffset || hasOffset;
                        allOffsets = allOffsets && hasOffset;
                    }
                    if (anyOffset && !allOffsets) {
                        return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                            QStringLiteral("notes.%1.phonemes").arg(index),
                            QStringLiteral("Phoneme offsets must be provided for every phoneme or "
                                           "omitted from all phonemes")));
                    }
                }
                auto note = decodeNote(object, {}, defaultLyric);
                const auto path = QStringLiteral("/notes/%1/").arg(index);
                if (!object.contains(QStringLiteral("lyric")))
                    resolvedValues.append({path + QStringLiteral("lyric"), note.lyric});
                notes.append(std::move(note));
            }
            auto result =
                m_runtime.notes().insertNotes(commandContext(arguments, invocation), clipId, notes);
            if (result)
                result.get().resolvedValues = std::move(resolvedValues);
            return mutationResult(std::move(result));
        });
        addBinding(ToolNames::notes_remove, [this](const QJsonObject &arguments,
                                                   const PublicInvocationContext &invocation) {
            return mutationResult(m_runtime.notes().removeNotes(
                commandContext(arguments, invocation),
                ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray())));
        });
        addBinding(ToolNames::notes_move,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().moveNotes(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                           arguments.value(QStringLiteral("delta_tick")).toInt(),
                           arguments.value(QStringLiteral("delta_key")).toInt()));
                   });
        addBinding(ToolNames::notes_resize_left,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().resizeNotesLeft(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                           arguments.value(QStringLiteral("delta_tick")).toInt(), 1));
                   });
        addBinding(ToolNames::notes_resize_right,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().resizeNotesRight(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                           arguments.value(QStringLiteral("delta_tick")).toInt(), 1));
                   });
        addBinding(ToolNames::notes_split_at,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().splitNoteAt(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           NoteId(arguments.value(QStringLiteral("note_id")).toInt()),
                           arguments.value(QStringLiteral("local_position")).toInt()));
                   });
        addBinding(ToolNames::notes_quantize,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().quantizeNotes(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                           arguments.value(QStringLiteral("quantize")).toInt(),
                           arguments.value(QStringLiteral("quantize_start")).toBool(),
                           arguments.value(QStringLiteral("quantize_length")).toBool()));
                   });
        addBinding(ToolNames::notes_set_lyric,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().setLyric(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           NoteId(arguments.value(QStringLiteral("note_id")).toInt()),
                           arguments.value(QStringLiteral("lyric")).toString()));
                   });
        addBinding(ToolNames::notes_set_phoneme_offsets,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       QList<int> offsets;
                       for (const auto &value :
                            arguments.value(QStringLiteral("offsets")).toArray())
                           offsets.append(value.toInt());
                       return mutationResult(m_runtime.notes().setPhonemeOffsets(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           NoteId(arguments.value(QStringLiteral("note_id")).toInt()), offsets));
                   });
        addBinding(
            ToolNames::notes_reset_phoneme_offsets,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.notes().resetPhonemeOffsets(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray())));
            });
        addBinding(ToolNames::parameters_replace,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       QList<CurveDraftDto> curves;
                       for (const auto &value : arguments.value(QStringLiteral("curves")).toArray())
                           curves.append(decodeCurve(value.toObject()));
                       return mutationResult(m_runtime.parameters().replaceParameter(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           parameterName(arguments.value(QStringLiteral("name")).toString()),
                           parameterType(arguments.value(QStringLiteral("layer")).toString()),
                           curves));
                   });
        addBinding(ToolNames::parameters_draw,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       QList<int> values;
                       for (const auto &value : arguments.value(QStringLiteral("values")).toArray())
                           values.append(value.toInt());
                       return mutationResult(m_runtime.parameters().drawParameter(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           parameterName(arguments.value(QStringLiteral("name")).toString()),
                           parameterType(arguments.value(QStringLiteral("layer")).toString()),
                           arguments.value(QStringLiteral("local_start")).toInt(),
                           arguments.value(QStringLiteral("step")).toInt(), std::move(values),
                           arguments.value(QStringLiteral("merge_mode")).toString() ==
                               QStringLiteral("overlay")));
                   });
        addBinding(ToolNames::parameters_erase,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.parameters().eraseParameter(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           parameterName(arguments.value(QStringLiteral("name")).toString()),
                           parameterType(arguments.value(QStringLiteral("layer")).toString()),
                           arguments.value(QStringLiteral("local_start")).toInt(),
                           arguments.value(QStringLiteral("local_end")).toInt()));
                   });
        addBinding(
            ToolNames::parameters_insert_anchors,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                QList<AnchorInsertDto> anchors;
                for (const auto &value : arguments.value(QStringLiteral("anchors")).toArray()) {
                    const auto anchor = value.toObject();
                    anchors.append({anchor.value(QStringLiteral("position")).toInt(),
                                    anchor.value(QStringLiteral("value")).toInt(),
                                    interpolation(anchor.value(QStringLiteral("interpolation"))
                                                      .toString(QStringLiteral("hermite")))});
                }
                return mutationResult(m_runtime.parameters().insertAnchors(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    parameterName(arguments.value(QStringLiteral("name")).toString()),
                    parameterType(arguments.value(QStringLiteral("layer")).toString()),
                    CurveId(arguments.value(QStringLiteral("curve_id")).toInt()), anchors));
            });
        addBinding(
            ToolNames::parameters_create_anchor_curve,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                QList<AnchorInsertDto> anchors;
                for (const auto &value : arguments.value(QStringLiteral("anchors")).toArray()) {
                    const auto anchor = value.toObject();
                    anchors.append({anchor.value(QStringLiteral("position")).toInt(),
                                    anchor.value(QStringLiteral("value")).toInt(),
                                    interpolation(anchor.value(QStringLiteral("interpolation"))
                                                      .toString(QStringLiteral("hermite")))});
                }
                return mutationResult(m_runtime.parameters().createAnchorCurve(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    parameterName(arguments.value(QStringLiteral("name")).toString()),
                    parameterType(arguments.value(QStringLiteral("layer")).toString()),
                    arguments.value(QStringLiteral("client_ref")).toString(), anchors));
            });
        addBinding(ToolNames::parameters_merge_anchor_curves,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.parameters().mergeAnchorCurves(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           parameterName(arguments.value(QStringLiteral("name")).toString()),
                           parameterType(arguments.value(QStringLiteral("layer")).toString()),
                           CurveId(arguments.value(QStringLiteral("target_curve_id")).toInt()),
                           CurveId(arguments.value(QStringLiteral("source_curve_id")).toInt())));
                   });
        addBinding(
            ToolNames::parameters_move_anchors,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                QList<AnchorMoveDto> moves;
                for (const auto &value : arguments.value(QStringLiteral("moves")).toArray()) {
                    const auto move = value.toObject();
                    moves.append({AnchorId(move.value(QStringLiteral("anchor_id")).toInt()),
                                  move.value(QStringLiteral("position")).toInt(),
                                  move.value(QStringLiteral("value")).toInt()});
                }
                return mutationResult(m_runtime.parameters().moveAnchors(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    parameterName(arguments.value(QStringLiteral("name")).toString()),
                    parameterType(arguments.value(QStringLiteral("layer")).toString()), moves));
            });
        addBinding(
            ToolNames::parameters_remove_anchors,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.parameters().removeAnchors(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    parameterName(arguments.value(QStringLiteral("name")).toString()),
                    parameterType(arguments.value(QStringLiteral("layer")).toString()),
                    objectIds<AnchorId>(arguments.value(QStringLiteral("anchor_ids")).toArray())));
            });
        addBinding(
            ToolNames::parameters_set_anchor_interpolation,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                return mutationResult(m_runtime.parameters().setAnchorInterpolations(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    parameterName(arguments.value(QStringLiteral("name")).toString()),
                    parameterType(arguments.value(QStringLiteral("layer")).toString()),
                    objectIds<AnchorId>(arguments.value(QStringLiteral("anchor_ids")).toArray()),
                    interpolation(arguments.value(QStringLiteral("interpolation")).toString())));
            });
        addBinding(ToolNames::parameters_bake, [this](const QJsonObject &arguments,
                                                      const PublicInvocationContext &invocation) {
            return mutationResult(m_runtime.parameters().bakeParameter(
                commandContext(arguments, invocation),
                ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                parameterName(arguments.value(QStringLiteral("name")).toString()),
                arguments.contains(QStringLiteral("local_start"))
                    ? std::optional<int>(arguments.value(QStringLiteral("local_start")).toInt())
                    : std::nullopt,
                arguments.contains(QStringLiteral("local_end"))
                    ? std::optional<int>(arguments.value(QStringLiteral("local_end")).toInt())
                    : std::nullopt));
        });
        addBinding(ToolNames::tracks_set_voice, [this](const QJsonObject &arguments,
                                                       const PublicInvocationContext &invocation) {
            auto voice = resolveVoiceSelection(m_runtime,
                                               arguments.value(QStringLiteral("voice")).toObject(),
                                               QStringLiteral("voice"));
            if (!voice)
                return AutomationResult<QJsonObject>(voice.getError());
            return mutationResult(m_runtime.parameters().selectTrackSingleSpeaker(
                commandContext(arguments, invocation),
                TrackId(arguments.value(QStringLiteral("track_id")).toInt()), voice.get().first,
                voice.get().second));
        });
        addBinding(ToolNames::speaker_mix_set_fixed,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto mix = resolveSpeakerMix(
                           m_runtime, arguments.value(QStringLiteral("mix")).toObject(),
                           QStringLiteral("mix"));
                       if (!mix)
                           return AutomationResult<QJsonObject>(mix.getError());
                       const auto target = decodeSpeakerMixTarget(
                           arguments.value(QStringLiteral("target")).toObject());
                       const auto speaker = mix.get().second.sources.isEmpty()
                                                ? SpeakerInfo{}
                                                : mix.get().second.sources.constFirst().speaker;
                       return mutationResult(m_runtime.parameters().setFixedSpeakerMix(
                           commandContext(arguments, invocation), target, mix.get().first, speaker,
                           mix.get().second));
                   });
        addBinding(ToolNames::speaker_mix_get, [this](const QJsonObject &arguments,
                                                      const PublicInvocationContext &) {
            auto result = m_runtime.parameters().getSpeakerMix(
                documentId(arguments),
                decodeSpeakerMixTarget(arguments.value(QStringLiteral("target")).toObject()));
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            if (result.get().singer.isEmpty() || result.get().speaker.isEmpty()) {
                return AutomationResult<QJsonObject>(
                    error(AutomationErrorCode::NotFound,
                          QStringLiteral("The target does not have an assigned voice"),
                          QStringLiteral("target")));
            }
            return AutomationResult<QJsonObject>(queryResult(
                result.get().document, QStringLiteral("snapshot"), encodeSpeakerMix(result.get())));
        });
        addBinding(ToolNames::speaker_mix_presets_list, [this](const QJsonObject &arguments,
                                                               const PublicInvocationContext &) {
            auto result = m_runtime.presets().getSpeakerMixPresets();
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            const auto singer = arguments.value(QStringLiteral("singer")).toObject();
            const auto packageId = singer.value(QStringLiteral("package_id")).toString();
            const auto singerId = singer.value(QStringLiteral("singer_id")).toString();
            std::optional<QVersionNumber> packageVersion;
            if (!singer.isEmpty()) {
                auto parsed = parsePackageVersion(singer, QStringLiteral("singer.package_version"));
                if (!parsed)
                    return AutomationResult<QJsonObject>(parsed.getError());
                packageVersion = parsed.get();
            }
            QJsonArray presets;
            for (const auto &preset : result.get()) {
                if ((!packageId.isEmpty() && preset.packageId != packageId) ||
                    (packageVersion && preset.packageVersion != *packageVersion) ||
                    (!singerId.isEmpty() && preset.singerId != singerId)) {
                    continue;
                }
                presets.append(encodeSpeakerMixPreset(preset));
            }
            return AutomationResult<QJsonObject>(QJsonObject{
                {QStringLiteral("presets"), presets}
            });
        });
        addBinding(ToolNames::speaker_mix_presets_save,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto preset = decodeSpeakerMixPreset(
                           m_runtime, arguments.value(QStringLiteral("preset")).toObject());
                       if (!preset)
                           return AutomationResult<QJsonObject>(preset.getError());
                       const auto context = applicationCommandContext(arguments, invocation);
                       auto result =
                           m_runtime.presets().saveSpeakerMixPreset(context, preset.get());
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(QJsonObject{
                           {QStringLiteral("preset"),         encodeSpeakerMixPreset(result.get())},
                           {QStringLiteral("changed"),        !context.validateOnly               },
                           {QStringLiteral("validated_only"), context.validateOnly                },
                       });
                   });
        addBinding(ToolNames::speaker_mix_presets_delete,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto result = m_runtime.presets().deleteSpeakerMixPreset(
                           applicationCommandContext(arguments, invocation),
                           arguments.value(QStringLiteral("preset_id")).toString());
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(QJsonObject{
                           {QStringLiteral("changed"),        result.get().changed      },
                           {QStringLiteral("validated_only"), result.get().validatedOnly},
                       });
                   });
        addBinding(ToolNames::speaker_mix_presets_apply,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       auto preset = resolveSpeakerMixPreset(
                           m_runtime, arguments.value(QStringLiteral("preset_id")).toString());
                       if (!preset)
                           return AutomationResult<QJsonObject>(preset.getError());
                       const auto target = decodeSpeakerMixTarget(
                           arguments.value(QStringLiteral("target")).toObject());
                       const auto &singer = preset.get().second.first;
                       const auto &mix = preset.get().second.second;
                       const auto speaker =
                           mix.sources.isEmpty() ? SpeakerInfo{} : mix.sources.constFirst().speaker;
                       if (target.kind == SpeakerMixTargetKind::Track) {
                           return mutationResult(m_runtime.parameters().applyTrackSpeakerMix(
                               commandContext(arguments, invocation), TrackId(target.id), singer,
                               speaker, mix));
                       }
                       return mutationResult(m_runtime.parameters().applyClipSpeakerMix(
                           commandContext(arguments, invocation), ClipId(target.id), singer,
                           speaker, mix));
                   });
        addBinding(ToolNames::clips_use_track_voice,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.parameters().useTrackVoiceContext(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt())));
                   });
        addBinding(ToolNames::clips_set_voice, [this](const QJsonObject &arguments,
                                                      const PublicInvocationContext &invocation) {
            auto voice = resolveVoiceSelection(m_runtime,
                                               arguments.value(QStringLiteral("voice")).toObject(),
                                               QStringLiteral("voice"));
            if (!voice)
                return AutomationResult<QJsonObject>(voice.getError());
            return mutationResult(m_runtime.parameters().selectClipSingleSpeaker(
                commandContext(arguments, invocation),
                ClipId(arguments.value(QStringLiteral("clip_id")).toInt()), voice.get().first,
                voice.get().second));
        });
        addBinding(ToolNames::speaker_mix_enable_dynamic,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.parameters().enableClipDynamicSpeakerMix(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt())));
                   });
        addBinding(ToolNames::speaker_mix_disable_dynamic,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.parameters().disableClipDynamicSpeakerMix(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt())));
                   });
        addBinding(ToolNames::speaker_mix_set_dynamic_bypass,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(
                           m_runtime.parameters().setClipDynamicSpeakerMixBypassed(
                               commandContext(arguments, invocation),
                               ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                               arguments.value(QStringLiteral("bypassed")).toBool()));
                   });
        addBinding(ToolNames::tempos_set,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().setTempo(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("tick")).toInt(),
                           arguments.value(QStringLiteral("tempo")).toDouble()));
                   });
        addBinding(ToolNames::tempos_remove,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().deleteTempo(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("tick")).toInt()));
                   });
        addBinding(ToolNames::time_signatures_set,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().setTimeSignature(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("bar_index")).toInt(),
                           arguments.value(QStringLiteral("numerator")).toInt(),
                           arguments.value(QStringLiteral("denominator")).toInt()));
                   });
        addBinding(ToolNames::time_signatures_remove,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().deleteTimeSignature(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("bar_index")).toInt()));
                   });
        addBinding(ToolNames::master_get,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto result = m_runtime.timeline().getMaster(documentId(arguments));
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(
                           queryResult(m_runtime.documentVersion(), QStringLiteral("snapshot"),
                                       QJsonObject{
                                           {QStringLiteral("gain"),               result.get().gain()},
                                           {QStringLiteral("pan"),                result.get().pan() },
                                           {QStringLiteral("mute"),               result.get().mute()},
                                           {QStringLiteral("solo"),               result.get().solo()},
                                           {QStringLiteral("metering_available"), false              }
                       }));
                   });
        addBinding(ToolNames::history_undo, [this](const QJsonObject &arguments,
                                                   const PublicInvocationContext &invocation) {
            return mutationResult(m_runtime.history().undo(commandContext(arguments, invocation)));
        });
        addBinding(ToolNames::history_redo, [this](const QJsonObject &arguments,
                                                   const PublicInvocationContext &invocation) {
            return mutationResult(m_runtime.history().redo(commandContext(arguments, invocation)));
        });
        addBinding(ToolNames::application_get_file_access,
                   [this](const QJsonObject &, const PublicInvocationContext &) {
                       const auto snapshot = m_fileGuard.snapshot();
                       QJsonArray readRoots;
                       for (const auto &path : snapshot.readRoots)
                           readRoots.append(path);
                       QJsonArray writeRoots;
                       for (const auto &path : snapshot.writeRoots)
                           writeRoots.append(path);
                       QJsonArray grants;
                       for (const auto &path : snapshot.sessionReadGrants) {
                           grants.append(QJsonObject{
                               {QStringLiteral("path"),   path                  },
                               {QStringLiteral("access"), QStringLiteral("read")}
                           });
                       }
                       for (const auto &path : snapshot.sessionWriteGrants) {
                           grants.append(QJsonObject{
                               {QStringLiteral("path"),   path                   },
                               {QStringLiteral("access"), QStringLiteral("write")}
                           });
                       }
                       return AutomationResult<QJsonObject>(QJsonObject{
                           {QStringLiteral("read_roots"),     readRoots },
                           {QStringLiteral("write_roots"),    writeRoots},
                           {QStringLiteral("session_grants"), grants    },
                       });
                   });
        addBinding(ToolNames::documents_new, [this](const QJsonObject &arguments,
                                                    const PublicInvocationContext &invocation) {
            const auto context = replacementCommandContext(m_runtime, arguments, invocation);
            auto current = m_runtime.documents().getDocument(context.expected.documentId);
            if (!current)
                return AutomationResult<QJsonObject>(current.getError());
            if (!current.get().saved &&
                arguments.value(QStringLiteral("unsaved_policy")).toString() ==
                    QStringLiteral("reject")) {
                return AutomationResult<QJsonObject>(
                    error(AutomationErrorCode::Busy,
                          QStringLiteral("The current document has unsaved changes"),
                          QStringLiteral("unsaved_policy")));
            }
            const auto document = DocumentAutomationFacade::newDocumentDraft(
                arguments.value(QStringLiteral("template")).toString() ==
                QStringLiteral("default"));
            auto committed = m_runtime.documents().commitNewDocument(context, document);
            if (!committed)
                return AutomationResult<QJsonObject>(committed.getError());
            QString path;
            bool dirty = false;
            if (!committed.get().validatedOnly) {
                auto current =
                    m_runtime.documents().getDocument(committed.get().current.documentId);
                if (current) {
                    path = current.get().path;
                    dirty = !current.get().saved;
                }
            }
            return AutomationResult<QJsonObject>(
                encodeDocumentLifecycleResult(committed.get(), path, dirty));
        });
        addBinding(ToolNames::documents_open, [this](const QJsonObject &arguments,
                                                     const PublicInvocationContext &invocation) {
            if (!m_hostServices.openDocument)
                return AutomationResult<QJsonObject>(
                    unavailable(QStringLiteral("Document open service is unavailable")));
            auto formats = m_runtime.files().listFormats();
            if (!formats)
                return AutomationResult<QJsonObject>(formats.getError());
            const auto path = arguments.value(QStringLiteral("path")).toString();
            auto format = resolveFormat(formats.get(), path, QStringLiteral("open"),
                                        arguments.value(QStringLiteral("format_id")).toString());
            if (!format)
                return AutomationResult<QJsonObject>(format.getError());
            const auto options = arguments.value(QStringLiteral("options")).toObject();
            auto encoding = validateFormatOptions(format.get(), QStringLiteral("open"), options);
            if (!encoding)
                return AutomationResult<QJsonObject>(encoding.getError());
            const auto planDigest = arguments.value(QStringLiteral("plan_digest")).toString();
            if (!planDigest.isEmpty()) {
                auto plan = inspectFormatPath(m_runtime, formats.get(), path,
                                              QStringLiteral("open"), format.get().id);
                if (!plan)
                    return AutomationResult<QJsonObject>(plan.getError());
                if (plan.get().value(QStringLiteral("plan_digest")).toString() != planDigest) {
                    return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                        QStringLiteral("plan_digest"),
                        QStringLiteral("The open plan digest is stale or invalid")));
                }
            }
            PublicDocumentOpenRequest request{
                .command = replacementCommandContext(m_runtime, arguments, invocation),
                .canonicalPath = path,
                .formatId = format.get().id,
                .encoding = encoding.get(),
                .importTempo = options.value(QStringLiteral("import_tempo")).toBool(true),
                .importTimeSignature =
                    options.value(QStringLiteral("import_time_signatures")).toBool(true),
                .planDigest = planDigest,
                .revalidatePlan = planRevalidator(
                    m_runtime, m_fileGuard, formats.get(), path, QStringLiteral("open"),
                    format.get().id, planDigest, QStringLiteral("path"),
                    QStringLiteral("plan_digest")),
                .unsavedPolicy = arguments.value(QStringLiteral("unsaved_policy")).toString() ==
                                         QStringLiteral("discard")
                                     ? PublicUnsavedPolicy::Discard
                                     : PublicUnsavedPolicy::Reject,
            };
            return taskAcceptedResult(m_hostServices.openDocument(request));
        });
        addBinding(ToolNames::documents_import, [this](const QJsonObject &arguments,
                                                       const PublicInvocationContext &invocation) {
            if (!m_hostServices.importDocument)
                return AutomationResult<QJsonObject>(
                    unavailable(QStringLiteral("Document import service is unavailable")));
            auto formats = m_runtime.files().listFormats();
            if (!formats)
                return AutomationResult<QJsonObject>(formats.getError());
            const auto path = arguments.value(QStringLiteral("path")).toString();
            auto format = resolveFormat(formats.get(), path, QStringLiteral("import"),
                                        arguments.value(QStringLiteral("format_id")).toString());
            if (!format)
                return AutomationResult<QJsonObject>(format.getError());
            const auto options = arguments.value(QStringLiteral("options")).toObject();
            auto encoding = validateFormatOptions(format.get(), QStringLiteral("import"), options);
            if (!encoding)
                return AutomationResult<QJsonObject>(encoding.getError());
            const auto planDigest = arguments.value(QStringLiteral("plan_digest")).toString();
            if (!planDigest.isEmpty()) {
                auto plan = inspectFormatPath(m_runtime, formats.get(), path,
                                              QStringLiteral("import"), format.get().id);
                if (!plan)
                    return AutomationResult<QJsonObject>(plan.getError());
                if (plan.get().value(QStringLiteral("plan_digest")).toString() != planDigest) {
                    return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                        QStringLiteral("plan_digest"),
                        QStringLiteral("The import plan digest is stale or invalid")));
                }
            }
            PublicDocumentImportRequest request{
                .command = commandContext(arguments, invocation),
                .canonicalPath = path,
                .formatId = format.get().id,
                .encoding = encoding.get(),
                .importTempo = options.value(QStringLiteral("import_tempo")).toBool(true),
                .importTimeSignature =
                    options.value(QStringLiteral("import_time_signatures")).toBool(true),
                .planDigest = planDigest,
                .revalidatePlan = planRevalidator(
                    m_runtime, m_fileGuard, formats.get(), path, QStringLiteral("import"),
                    format.get().id, planDigest, QStringLiteral("path"),
                    QStringLiteral("plan_digest")),
                .mergeMode = QStringLiteral("append"),
            };
            return taskAcceptedResult(m_hostServices.importDocument(request));
        });
        addBinding(ToolNames::documents_save, [this](const QJsonObject &arguments,
                                                     const PublicInvocationContext &invocation) {
            auto document = m_runtime.documents().getDocument(documentId(arguments));
            if (!document)
                return AutomationResult<QJsonObject>(document.getError());
            const auto path = document.get().path;
            if (path.trimmed().isEmpty()) {
                AutomationError error;
                error.code = AutomationErrorCode::PathRequired;
                error.fieldPath = QStringLiteral("path");
                error.message = QStringLiteral("The current document has no save path");
                return AutomationResult<QJsonObject>(std::move(error));
            }
            auto reauthorized = m_fileGuard.reauthorize({path, FileAccessPurpose::Write});
            if (!reauthorized)
                return AutomationResult<QJsonObject>(reauthorized.getError());
            const auto overwritePolicy =
                arguments.value(QStringLiteral("overwrite_policy")).toString();
            return mutationResult(m_runtime.documents().saveDocument(
                commandContext(arguments, invocation), path,
                overwritePolicy.isEmpty() || overwritePolicy == QStringLiteral("overwrite")));
        });
        addBinding(ToolNames::formats_list, [this](const QJsonObject &arguments,
                                                   const PublicInvocationContext &) {
            auto result = m_runtime.files().listFormats();
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            QJsonArray formats;
            for (const auto &format : result.get()) {
                const auto purpose = arguments.value(QStringLiteral("purpose")).toString();
                if ((purpose == QStringLiteral("open") && !format.canOpen) ||
                    (purpose == QStringLiteral("import") && !format.canImport) ||
                    (purpose == QStringLiteral("export") && !format.canExport)) {
                    continue;
                }
                if (format.optionSchema.isEmpty() ||
                    !AutomationWire::checkJsonSchema(format.optionSchema).valid()) {
                    return AutomationResult<QJsonObject>(
                        error(AutomationErrorCode::InternalError,
                              QStringLiteral("Project format option schema is unavailable")));
                }
                QJsonArray extensions;
                for (const auto &extension : format.extensions)
                    extensions.append(extension);
                formats.append(QJsonObject{
                    {QStringLiteral("id"),                 format.id               },
                    {QStringLiteral("display_name"),       format.displayName      },
                    {QStringLiteral("extensions"),         extensions              },
                    {QStringLiteral("can_open"),           format.canOpen          },
                    {QStringLiteral("can_import"),         format.canImport        },
                    {QStringLiteral("can_export"),         format.canExport        },
                    {QStringLiteral("available"),          format.available        },
                    {QStringLiteral("unavailable_reason"), format.unavailableReason},
                    {QStringLiteral("option_schema"),      format.optionSchema     },
                });
            }
            return AutomationResult<QJsonObject>(QJsonObject{
                {QStringLiteral("formats"), formats}
            });
        });
        addBinding(ToolNames::audio_clips_import,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       if (!m_hostServices.importAudioClip)
                           return AutomationResult<QJsonObject>(
                               unavailable(QStringLiteral("Audio import service is unavailable")));
                       PublicAudioClipProperties properties;
                       properties.start = arguments.value(QStringLiteral("start")).toInt();
                       if (arguments.contains(QStringLiteral("name")))
                           properties.name = arguments.value(QStringLiteral("name")).toString();
                       if (arguments.contains(QStringLiteral("gain")))
                           properties.gain = arguments.value(QStringLiteral("gain")).toDouble();
                       if (arguments.contains(QStringLiteral("mute")))
                           properties.mute = arguments.value(QStringLiteral("mute")).toBool();
                       return taskAcceptedResult(m_hostServices.importAudioClip(
                           {commandContext(arguments, invocation),
                            TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                            arguments.value(QStringLiteral("path")).toString(),
                            properties,
                            {}}));
                   });
        addBinding(
            ToolNames::audio_clips_import_batch,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                if (!m_hostServices.importAudioClips)
                    return AutomationResult<QJsonObject>(
                        unavailable(QStringLiteral("Audio batch import service is unavailable")));
                PublicAudioClipBatchImportRequest request;
                request.command = commandContext(arguments, invocation);
                request.failurePolicy =
                    arguments.value(QStringLiteral("failure_policy")).toString() ==
                            QStringLiteral("best_effort")
                        ? PublicBatchFailurePolicy::BestEffort
                        : PublicBatchFailurePolicy::Atomic;
                for (const auto &value : arguments.value(QStringLiteral("items")).toArray()) {
                    const auto item = value.toObject();
                    request.items.append({
                        TrackId(item.value(QStringLiteral("track_id")).toInt()),
                        item.value(QStringLiteral("path")).toString(),
                        PublicAudioClipProperties{
                                                  .name = item.contains(QStringLiteral("name"))
                                        ? std::optional<QString>(
                                              item.value(QStringLiteral("name")).toString())
                                        : std::nullopt,
                                                  .start = item.value(QStringLiteral("start")).toInt(),
                                                  .gain = item.contains(QStringLiteral("gain"))
                                        ? std::optional<double>(
                                              item.value(QStringLiteral("gain")).toDouble())
                                        : std::nullopt,
                                                  .mute = item.contains(QStringLiteral("mute"))
                                        ? std::optional<bool>(
                                              item.value(QStringLiteral("mute")).toBool())
                                        : std::nullopt,
                                                  },
                        {},
                        itemValidationError(item),
                    });
                }
                return taskAcceptedResult(m_hostServices.importAudioClips(request));
            });
        addBinding(ToolNames::audio_clips_relocate,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       const auto path = arguments.value(QStringLiteral("path")).toString();
                       auto prepared =
                           prepareAuthorizedAudioPath(m_hostServices, m_fileGuard, path);
                       if (!prepared)
                           return AutomationResult<QJsonObject>(prepared.getError());
                       return mutationResult(m_runtime.project().relocateAudioClip(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()), path,
                           AudioPathInfo{{}, prepared.get().sha512}, prepared.get().formatData));
                   });
        addBinding(
            ToolNames::audio_clips_confirm_path,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                auto path = arguments.value(QStringLiteral("path")).toString();
                if (path.isEmpty()) {
                    auto project = m_runtime.project().getProject(documentId(arguments));
                    if (!project)
                        return AutomationResult<QJsonObject>(project.getError());
                    const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
                    for (const auto &track : project.get().tracks) {
                        for (const auto &clip : track.clips) {
                            if (clip.id != clipId)
                                continue;
                            if (clip.data.type != ClipDraftDto::Type::Audio) {
                                return AutomationResult<QJsonObject>(
                                    AutomationError::wrongObjectType(
                                        {ObjectKind::Clip, clipId.value()},
                                        QStringLiteral(
                                            "Path confirmation requires an audio clip")));
                            }
                            path = clip.data.audioPath;
                        }
                    }
                    if (path.isEmpty()) {
                        return AutomationResult<QJsonObject>(AutomationError::notFound(
                            {ObjectKind::Clip, clipId.value()},
                            QStringLiteral("The audio clip or its candidate path was not found")));
                    }
                }
                auto prepared = prepareAuthorizedAudioPath(m_hostServices, m_fileGuard, path);
                if (!prepared)
                    return AutomationResult<QJsonObject>(prepared.getError());
                return mutationResult(m_runtime.project().confirmAudioClipPath(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()), path,
                    AudioPathInfo{{}, prepared.get().sha512}, prepared.get().formatData));
            });
        addBinding(ToolNames::exports_midi_start, [this](
                                                      const QJsonObject &arguments,
                                                      const PublicInvocationContext &invocation) {
            auto resolvedContext = documentQueryCommandContext(m_runtime, arguments, invocation);
            if (!resolvedContext)
                return AutomationResult<QJsonObject>(resolvedContext.getError());
            const auto context = resolvedContext.get();
            const auto path = arguments.value(QStringLiteral("path")).toString();
            const auto allowOverwrite =
                arguments.value(QStringLiteral("overwrite_policy")).toString() ==
                QStringLiteral("overwrite");
            const auto encodedOptions = arguments.value(QStringLiteral("options")).toObject();
            auto project = m_runtime.project().getProject(context.expected.documentId);
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            auto options = decodeMidiExportOptions(project.get(), encodedOptions);
            if (!options)
                return AutomationResult<QJsonObject>(options.getError());
            if (context.validateOnly) {
                auto validated = m_runtime.files().prepareMidiExport(context, path, allowOverwrite,
                                                                     options.get());
                if (!validated)
                    return AutomationResult<QJsonObject>(validated.getError());
                return AutomationResult<QJsonObject>(
                    encodeTaskAccepted({{}, context.expected, true}));
            }
            auto prepared =
                m_runtime.files().prepareMidiExport(context, path, allowOverwrite, options.get());
            if (!prepared)
                return AutomationResult<QJsonObject>(prepared.getError());
            auto snapshot = m_runtime.automationTasks().createTask(
                OperationIds::exports::midi::start, context.expected, std::nullopt, {},
                invocation.clientId);
            const auto taskId = snapshot.taskId;
            auto *tasks = &m_runtime.automationTasks();
            auto *files = &m_runtime.files();
            auto *fileGuard = &m_fileGuard;
            const AuthorizedPath authorizedPath{path, FileAccessPurpose::Write};
            auto execute = [tasks, files, fileGuard, authorizedPath, taskId, context,
                            prepared = prepared.get()] {
                if (!tasks->markRunning(taskId)) {
                    tasks->beginCommitting(taskId);
                    return;
                }
                auto authorized = fileGuard->reauthorize(authorizedPath);
                if (!authorized) {
                    auto error = authorized.getError();
                    error.taskId = taskId;
                    tasks->fail(taskId, std::move(error));
                    return;
                }
                const auto committing = tasks->beginCommitting(taskId);
                if (!committing || !committing.get())
                    return;
                auto exported = files->writePreparedMidiExport(prepared);
                if (!exported) {
                    tasks->fail(taskId, exported.getError());
                    return;
                }
                authorized = fileGuard->reauthorize(authorizedPath);
                if (!authorized) {
                    auto error = authorized.getError();
                    error.taskId = taskId;
                    tasks->fail(taskId, std::move(error));
                    return;
                }
                MutationResult completed;
                completed.previous = context.expected;
                completed.current = context.expected;
                completed.validatedOnly = context.validateOnly;
                tasks->succeed(taskId, completed);
            };
            QThreadPool::globalInstance()->start(std::move(execute));
            return AutomationResult<QJsonObject>(
                encodeTaskAccepted({snapshot.taskId, context.expected, context.validateOnly}));
        });
        addBinding(ToolNames::exports_audio_get_capabilities,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       if (!m_hostServices.audioExportCapabilities)
                           return AutomationResult<QJsonObject>(unavailable(
                               QStringLiteral("Audio export capability service is unavailable")));
                       const auto document = documentId(arguments);
                       auto project = m_runtime.project().getProject(document);
                       if (!project)
                           return AutomationResult<QJsonObject>(project.getError());
                       auto capabilities = m_hostServices.audioExportCapabilities(document);
                       if (!capabilities)
                           return AutomationResult<QJsonObject>(capabilities.getError());
                       return AutomationResult<QJsonObject>(
                           queryResult(project.get().document, QStringLiteral("capabilities"),
                                       capabilities.get()));
                   });
        addBinding(ToolNames::exports_audio_preview, [this](const QJsonObject &arguments,
                                                            const PublicInvocationContext &) {
            const auto options = arguments.value(QStringLiteral("options")).toObject();
            const auto document = documentId(arguments);
            const auto previewPath = arguments.value(QStringLiteral("path")).toString();
            auto config = resolveAudioExportSources(m_runtime, document,
                                                    decodeAudioExportConfig(options, previewPath));
            if (!config)
                return AutomationResult<QJsonObject>(config.getError());
            auto result = m_runtime.audioExports().preview(document, config.get());
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            auto encoded = encodeAudioExportPreview(result.get());
            if (!encoded)
                return AutomationResult<QJsonObject>(encoded.getError());
            const auto raw = encoded.get();
            QJsonArray targets;
            for (const auto &value : raw.value(QStringLiteral("plan"))
                                         .toObject()
                                         .value(QStringLiteral("targets"))
                                         .toArray()) {
                targets.append(value.toObject().value(QStringLiteral("path")).toString());
            }
            QJsonArray diagnostics;
            for (const auto &value : raw.value(QStringLiteral("diagnostics")).toArray()) {
                const auto diagnostic = value.toObject();
                diagnostics.append(QJsonObject{
                    {QStringLiteral("code"),     diagnostic.value(QStringLiteral("code"))    },
                    {QStringLiteral("message"),  diagnostic.value(QStringLiteral("message")) },
                    {QStringLiteral("blocking"), diagnostic.value(QStringLiteral("blocking"))},
                });
            }
            const QJsonObject plan{
                {QStringLiteral("targets"),     targets                          },
                {QStringLiteral("diagnostics"), diagnostics                      },
                {QStringLiteral("plan_digest"), AutomationWire::sha256Digest(raw)}
            };
            return AutomationResult<QJsonObject>(
                queryResult(m_runtime.documentVersion(), QStringLiteral("plan"), plan));
        });
        addBinding(
            ToolNames::exports_audio_start,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto options = arguments.value(QStringLiteral("options")).toObject();
                AudioExportPolicyDto policy;
                policy.allowOverwrite =
                    arguments.value(QStringLiteral("overwrite_policy")).toString() ==
                    QStringLiteral("overwrite");
                auto config = resolveAudioExportSources(
                    m_runtime, documentId(arguments),
                    decodeAudioExportConfig(options,
                                            arguments.value(QStringLiteral("path")).toString()));
                if (!config)
                    return AutomationResult<QJsonObject>(config.getError());
                auto context = documentQueryCommandContext(m_runtime, arguments, invocation);
                if (!context)
                    return AutomationResult<QJsonObject>(context.getError());
                const AuthorizedPath authorizedPath{
                    arguments.value(QStringLiteral("path")).toString(), FileAccessPurpose::Write};
                auto derivedPaths = std::make_shared<QList<AuthorizedPath>>();
                return taskAcceptedResult(m_runtime.audioExports().start(
                    context.get(), config.get(), policy, {},
                    [guard = &m_fileGuard, derivedPaths](
                        const AudioExportPreviewDto &preview) -> AutomationResult<AutomationUnit> {
                        QList<AuthorizedPath> authorizedPaths;
                        authorizedPaths.reserve(preview.filePaths.size());
                        for (const auto &path : preview.filePaths) {
                            auto authorized = guard->authorize(path, FileAccessPurpose::Write);
                            if (!authorized)
                                return authorized.getError();
                            authorizedPaths.append(authorized.get());
                        }
                        *derivedPaths = std::move(authorizedPaths);
                        return AutomationUnit{};
                    },
                    [guard = &m_fileGuard, authorizedPath, derivedPaths] {
                        auto authorized = guard->reauthorize(authorizedPath);
                        if (!authorized)
                            return AutomationResult<AutomationUnit>(authorized.getError());
                        for (const auto &path : std::as_const(*derivedPaths)) {
                            authorized = guard->reauthorize(path);
                            if (!authorized)
                                return AutomationResult<AutomationUnit>(authorized.getError());
                        }
                        return AutomationResult<AutomationUnit>(AutomationUnit{});
                    }));
            });
        addBinding(ToolNames::extract_get_capabilities, [this](const QJsonObject &arguments,
                                                               const PublicInvocationContext &) {
            const auto clipId =
                ClipId(arguments.value(QStringLiteral("source_audio_clip_id")).toInt());
            if (!m_hostServices.extractionCapabilities)
                return AutomationResult<QJsonObject>(
                    unavailable(QStringLiteral("Extraction capability service is unavailable")));
            const auto document = documentId(arguments);
            auto project = m_runtime.project().getProject(document);
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            bool sourceFound = false;
            for (const auto &track : project.get().tracks) {
                for (const auto &clip : track.clips) {
                    if (clip.id != clipId)
                        continue;
                    sourceFound = true;
                    if (clip.data.type != ClipDraftDto::Type::Audio) {
                        return AutomationResult<QJsonObject>(AutomationError::wrongObjectType(
                            {ObjectKind::Clip, clipId.value()},
                            QStringLiteral("Extraction requires an audio clip")));
                    }
                    break;
                }
                if (sourceFound)
                    break;
            }
            if (!sourceFound) {
                return AutomationResult<QJsonObject>(
                    AutomationError::notFound({ObjectKind::Clip, clipId.value()},
                                              QStringLiteral("Source audio clip was not found")));
            }
            auto capabilities = m_hostServices.extractionCapabilities(document, clipId);
            if (!capabilities)
                return AutomationResult<QJsonObject>(capabilities.getError());
            return AutomationResult<QJsonObject>(queryResult(
                project.get().document, QStringLiteral("capabilities"), capabilities.get()));
        });
        addBinding(
            ToolNames::extract_pitch_start,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto options = arguments.value(QStringLiteral("options")).toObject();
                PitchExtractionOptionsDto extractionOptions;
                extractionOptions.modelId = options.value(QStringLiteral("model_id")).toString();
                extractionOptions.authorizeSource = sourcePathAuthorizer(m_fileGuard);
                return taskAcceptedResult(m_runtime.extractions().startPitch(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("source_audio_clip_id")).toInt()),
                    ClipId(arguments.value(QStringLiteral("target_singing_clip_id")).toInt()),
                    std::move(extractionOptions)));
            });
        addBinding(
            ToolNames::extract_midi_start,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto options = arguments.value(QStringLiteral("options")).toObject();
                MidiExtractionOptionsDto extractionOptions;
                extractionOptions.modelId = options.value(QStringLiteral("model_id")).toString();
                extractionOptions.authorizeSource = sourcePathAuthorizer(m_fileGuard);
                extractionOptions.defaultLanguage =
                    options.value(QStringLiteral("default_language")).toString();
                extractionOptions.defaultLyric =
                    options.value(QStringLiteral("default_lyric")).toString();
                extractionOptions.clientRef =
                    options.value(QStringLiteral("client_ref")).toString();
                if (options.contains(QStringLiteral("minimum_note_length")))
                    extractionOptions.minimumNoteLength =
                        options.value(QStringLiteral("minimum_note_length")).toInt();
                const auto destination = arguments.value(QStringLiteral("destination")).toObject();
                extractionOptions.destinationMode =
                    destination.value(QStringLiteral("mode")).toString();
                const bool hasTargetClip = destination.contains(QStringLiteral("target_clip_id"));
                if (extractionOptions.destinationMode == QStringLiteral("create_clip") &&
                    hasTargetClip) {
                    return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                        QStringLiteral("destination.target_clip_id"),
                        QStringLiteral("create_clip does not accept a target clip")));
                }
                if (extractionOptions.destinationMode == QStringLiteral("merge_into_clip") &&
                    !hasTargetClip) {
                    return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                        QStringLiteral("destination.target_clip_id"),
                        QStringLiteral("merge_into_clip requires a target clip")));
                }
                extractionOptions.targetTrackId =
                    TrackId(destination.value(QStringLiteral("target_track_id")).toInt());
                if (hasTargetClip) {
                    extractionOptions.targetClipId =
                        ClipId(destination.value(QStringLiteral("target_clip_id")).toInt());
                }
                extractionOptions.targetStart = destination.value(QStringLiteral("start")).toInt();
                return taskAcceptedResult(m_runtime.extractions().startMidi(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("source_audio_clip_id")).toInt()),
                    std::move(extractionOptions)));
            });
        addBinding(ToolNames::inference_get_capabilities, [this](const QJsonObject &arguments,
                                                                 const PublicInvocationContext &) {
            const auto scope = arguments.value(QStringLiteral("scope")).toObject();
            if (!m_hostServices.inferenceCapabilities)
                return AutomationResult<QJsonObject>(
                    unavailable(QStringLiteral("Inference capability service is unavailable")));
            auto capabilities = m_hostServices.inferenceCapabilities(documentId(arguments), scope);
            if (!capabilities)
                return AutomationResult<QJsonObject>(capabilities.getError());
            return AutomationResult<QJsonObject>(queryResult(
                m_runtime.documentVersion(), QStringLiteral("capabilities"), capabilities.get()));
        });
        addBinding(ToolNames::inference_start,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       if (!m_hostServices.startInference)
                           return AutomationResult<QJsonObject>(unavailable(
                               QStringLiteral("Inference orchestration service is unavailable")));
                       PublicInferenceStartRequest request;
                       request.command = commandContext(arguments, invocation);
                       request.scope = arguments.value(QStringLiteral("scope")).toObject();
                       request.options = arguments.value(QStringLiteral("options")).toObject();
                       for (const auto &value : arguments.value(QStringLiteral("stages")).toArray())
                           request.stages.append(value.toString());
                       if (request.stages.isEmpty())
                           request.stages = InferenceAutomationFacade::supportedStages();
                       return taskAcceptedResult(m_hostServices.startInference(request));
                   });
        addBinding(ToolNames::inference_reset_stage,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       if (!m_hostServices.resetInferenceStage)
                           return AutomationResult<QJsonObject>(unavailable(QStringLiteral(
                               "Inference reset orchestration service is unavailable")));
                       return mutationResult(m_hostServices.resetInferenceStage({
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("scope")).toObject(),
                           arguments.value(QStringLiteral("stage")).toString(),
                       }));
                   });
        addBinding(ToolNames::tasks_list, [this](const QJsonObject &arguments,
                                                 const PublicInvocationContext &) {
            const auto applicationScope = arguments.value(QStringLiteral("scope")).toString() ==
                                          QStringLiteral("application");
            QList<AutomationTaskSnapshot> filtered;
            if (applicationScope) {
                filtered = m_runtime.automationTasks().listApplication();
            } else {
                auto tasks = m_runtime.tasks().listTasks(documentId(arguments));
                if (!tasks)
                    return AutomationResult<QJsonObject>(tasks.getError());
                filtered = tasks.get();
            }
            const auto stateFilter = arguments.value(QStringLiteral("state")).toString();
            const auto kindFilter = arguments.value(QStringLiteral("kind")).toString();
            filtered.removeIf([&](const AutomationTaskSnapshot &task) {
                return !isPublicTaskKind(task.operationId) ||
                       (!stateFilter.isEmpty() && taskStateName(task.state) != stateFilter) ||
                       (!kindFilter.isEmpty() && task.operationId != kindFilter);
            });
            QJsonArray snapshot;
            for (const auto &task : std::as_const(filtered))
                snapshot.append(encodeTaskSnapshot(task));
            QString digestError;
            const auto snapshotDigest = AutomationWire::sha256Digest(
                QJsonObject{
                    {QStringLiteral("scope"),
                     applicationScope ? QStringLiteral("application") : QStringLiteral("document")},
                    {QStringLiteral("document"),
                     applicationScope
                         ? QJsonValue(QJsonValue::Null)
                         : QJsonValue(encodeDocumentVersion(m_runtime.documentVersion()))         },
                    {QStringLiteral("state"),    stateFilter                                      },
                    {QStringLiteral("kind"),     kindFilter                                       },
                    {QStringLiteral("tasks"),    snapshot                                         },
            },
                &digestError);
            if (!digestError.isEmpty()) {
                return AutomationResult<QJsonObject>(
                    error(AutomationErrorCode::InternalError,
                          QStringLiteral("Task cursor snapshot could not be encoded")));
            }
            qint64 offset = 0;
            const auto cursorText = arguments.value(QStringLiteral("cursor")).toString();
            if (!cursorText.isEmpty()) {
                const auto parsed = m_taskCursorCodec.parse(
                    cursorText, QStringLiteral("editor-public-tasks-list/v1"), snapshotDigest);
                if (!parsed.valid()) {
                    return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                        QStringLiteral("cursor"), QStringLiteral("Task cursor is invalid")));
                }
                offset = *parsed.offset;
            }
            if (offset < 0 || offset > filtered.size()) {
                return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                    QStringLiteral("cursor"), QStringLiteral("Task cursor is invalid")));
            }
            const auto limit = arguments.value(QStringLiteral("limit")).toInt(filtered.size());
            QJsonArray resultItems;
            const auto end = std::min<qint64>(offset + limit, filtered.size());
            for (auto index = offset; index < end; ++index)
                resultItems.append(encodeTaskSnapshot(filtered.at(static_cast<qsizetype>(index))));
            QJsonObject result{
                {QStringLiteral("scope"),
                 applicationScope ? QStringLiteral("application") : QStringLiteral("document")    },
                {QStringLiteral("document"),
                 applicationScope ? QJsonValue(QJsonValue::Null)
                                  : QJsonValue(encodeDocumentVersion(m_runtime.documentVersion()))},
                {QStringLiteral("tasks"),    resultItems                                          },
            };
            if (end < filtered.size()) {
                const auto nextCursor = m_taskCursorCodec.issue(
                    QStringLiteral("editor-public-tasks-list/v1"), snapshotDigest, end);
                if (nextCursor.isEmpty()) {
                    return AutomationResult<QJsonObject>(
                        error(AutomationErrorCode::InternalError,
                              QStringLiteral("Task cursor could not be issued"),
                              QStringLiteral("next_cursor")));
                }
                result.insert(QStringLiteral("next_cursor"), nextCursor);
            }
            return AutomationResult<QJsonObject>(std::move(result));
        });
        addBinding(ToolNames::tasks_get, [this](const QJsonObject &arguments,
                                                const PublicInvocationContext &) {
            const auto taskId =
                TaskId::fromString(arguments.value(QStringLiteral("task_id")).toString());
            const auto applicationScope = arguments.value(QStringLiteral("scope")).toString() ==
                                          QStringLiteral("application");
            auto result = applicationScope
                              ? m_runtime.automationTasks().getApplication(taskId)
                              : m_runtime.tasks().getTask(documentId(arguments), taskId);
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            if (!isPublicTaskKind(result.get().operationId)) {
                return AutomationResult<QJsonObject>(AutomationError::taskNotFound(
                    TaskId::fromString(arguments.value(QStringLiteral("task_id")).toString())));
            }
            return AutomationResult<QJsonObject>(encodeTaskSnapshot(result.get()));
        });
        addBinding(ToolNames::tasks_cancel, [this](const QJsonObject &arguments,
                                                   const PublicInvocationContext &invocation) {
            const auto taskId =
                TaskId::fromString(arguments.value(QStringLiteral("task_id")).toString());
            const auto applicationScope = arguments.value(QStringLiteral("scope")).toString() ==
                                          QStringLiteral("application");
            auto snapshot = applicationScope
                                ? m_runtime.automationTasks().getApplication(taskId)
                                : m_runtime.tasks().getTask(documentId(arguments), taskId);
            if (!snapshot)
                return AutomationResult<QJsonObject>(snapshot.getError());
            if (!isPublicTaskKind(snapshot.get().operationId)) {
                return AutomationResult<QJsonObject>(AutomationError::taskNotFound(taskId));
            }
            AutomationResult<AutomationTaskSnapshot> result =
                applicationScope ? m_runtime.automationTasks().requestCancelApplication(taskId)
                                 : [&]() -> AutomationResult<AutomationTaskSnapshot> {
                auto context = documentQueryCommandContext(m_runtime, arguments, invocation);
                if (!context)
                    return context.getError();
                return m_runtime.tasks().cancelTask(context.get(), taskId);
            }();
            if (!result)
                return AutomationResult<QJsonObject>(result.getError());
            return AutomationResult<QJsonObject>(encodeTaskSnapshot(result.get()));
        });
        addBinding(ToolNames::playback_get_state,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto result = m_runtime.playback().getPlayback(documentId(arguments));
                       if (!result)
                           return AutomationResult<QJsonObject>(result.getError());
                       return AutomationResult<QJsonObject>(queryResult(
                           result.get().document, QStringLiteral("snapshot"),
                           encodePlayback(result.get())));
                   });
        addBinding(ToolNames::playback_play,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return playbackStateMutationResult(
                           m_runtime, m_runtime.playback().play(playbackCommandContext(
                                          m_runtime, arguments, invocation)));
                   });
        addBinding(ToolNames::playback_pause,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return playbackStateMutationResult(
                           m_runtime, m_runtime.playback().pause(playbackCommandContext(
                                          m_runtime, arguments, invocation)));
                   });
        addBinding(ToolNames::playback_stop,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return playbackStateMutationResult(
                           m_runtime, m_runtime.playback().stop(playbackCommandContext(
                                          m_runtime, arguments, invocation)));
                   });
        addBinding(ToolNames::playback_seek, [this](const QJsonObject &arguments,
                                                    const PublicInvocationContext &invocation) {
            return playbackStateMutationResult(
                m_runtime,
                m_runtime.playback().seek(playbackCommandContext(m_runtime, arguments, invocation),
                                          arguments.value(QStringLiteral("position")).toDouble()));
        });
        addBinding(ToolNames::master_set_gain,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().setMasterGain(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("gain")).toDouble()));
                   });
        addBinding(ToolNames::playback_set_loop, [this](const QJsonObject &arguments,
                                                        const PublicInvocationContext &invocation) {
            const auto start = playbackLoopTick(arguments, QStringLiteral("start"));
            if (!start)
                return AutomationResult<QJsonObject>(start.getError());
            const auto end = playbackLoopTick(arguments, QStringLiteral("end"));
            if (!end)
                return AutomationResult<QJsonObject>(end.getError());
            return playbackDocumentMutationResult(
                m_runtime, m_runtime.playback().setLoop(
                               commandContext(arguments, invocation),
                               LoopSettings(true, start.get(), end.get() - start.get())));
        });
        addBinding(ToolNames::playback_set_loop_enabled,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return playbackDocumentMutationResult(
                           m_runtime, m_runtime.playback().setLoopEnabled(
                                          commandContext(arguments, invocation),
                                          arguments.value(QStringLiteral("enabled")).toBool()));
                   });
        addBinding(ToolNames::playback_clear_loop,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return playbackDocumentMutationResult(
                           m_runtime,
                           m_runtime.playback().clearLoop(commandContext(arguments, invocation)));
                   });

        addBinding(ToolNames::documents_save_as, [this](const QJsonObject &arguments,
                                                        const PublicInvocationContext &invocation) {
            auto path = arguments.value(QStringLiteral("path")).toString();
            const auto extensionPolicy =
                arguments.value(QStringLiteral("extension_policy")).toString();
            if ((extensionPolicy == QStringLiteral("append_if_missing") ||
                 extensionPolicy == QStringLiteral("replace")) &&
                QFileInfo(path).suffix().compare(QStringLiteral("dspx"), Qt::CaseInsensitive) !=
                    0) {
                if (extensionPolicy == QStringLiteral("replace")) {
                    QFileInfo info(path);
                    path = QDir(info.absolutePath())
                               .filePath(info.completeBaseName() + QStringLiteral(".dspx"));
                } else if (QFileInfo(path).suffix().isEmpty()) {
                    path += QStringLiteral(".dspx");
                }
            }
            auto authorized = m_fileGuard.authorize(path, FileAccessPurpose::Write);
            if (!authorized)
                return AutomationResult<QJsonObject>(authorized.getError());
            return mutationResult(m_runtime.documents().saveDocumentAs(
                commandContext(arguments, invocation), authorized.get().canonicalPath,
                arguments.value(QStringLiteral("overwrite_policy")).toString() ==
                    QStringLiteral("overwrite")));
        });

        addBinding(
            ToolNames::documents_import_batch,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                if (!m_hostServices.importDocuments) {
                    return AutomationResult<QJsonObject>(unavailable(
                        QStringLiteral("Document batch import service is unavailable")));
                }
                auto formats = m_runtime.files().listFormats();
                if (!formats)
                    return AutomationResult<QJsonObject>(formats.getError());
                PublicDocumentBatchImportRequest request;
                request.command = commandContext(arguments, invocation);
                request.failurePolicy =
                    arguments.value(QStringLiteral("failure_policy")).toString() ==
                            QStringLiteral("best_effort")
                        ? PublicBatchFailurePolicy::BestEffort
                        : PublicBatchFailurePolicy::Atomic;
                for (const auto &value : arguments.value(QStringLiteral("items")).toArray()) {
                    const auto item = value.toObject();
                    const auto path = item.value(QStringLiteral("path")).toString();
                    PublicDocumentBatchImportItem requestItem{
                        path,
                        item.value(QStringLiteral("format_id")).toString(),
                        item.value(QStringLiteral("options")).toObject(),
                        item.value(QStringLiteral("plan_digest")).toString(),
                    };
                    if (auto failure = itemValidationError(item)) {
                        requestItem.validationError = std::move(*failure);
                        request.items.append(std::move(requestItem));
                        continue;
                    }
                    auto format = resolveFormat(formats.get(), path, QStringLiteral("import"),
                                                requestItem.formatId);
                    if (!format) {
                        auto failure = format.getError();
                        failure.fieldPath = failure.fieldPath == QStringLiteral("format_id")
                                                ? QStringLiteral("items.format_id")
                                                : QStringLiteral("items.path");
                        requestItem.validationError = std::move(failure);
                        request.items.append(std::move(requestItem));
                        continue;
                    }
                    auto encoding =
                        validateFormatOptions(format.get(), QStringLiteral("import"),
                                              requestItem.options, QStringLiteral("items.options"));
                    if (!encoding) {
                        requestItem.validationError = encoding.getError();
                        request.items.append(std::move(requestItem));
                        continue;
                    }
                    if (!encoding.get().isEmpty()) {
                        requestItem.options.insert(QStringLiteral("encoding"), encoding.get());
                    }
                    if (!requestItem.planDigest.isEmpty()) {
                        auto plan = inspectFormatPath(m_runtime, formats.get(), path,
                                                      QStringLiteral("import"), format.get().id);
                        if (!plan) {
                            auto failure = plan.getError();
                            failure.fieldPath = QStringLiteral("items.path");
                            requestItem.validationError = std::move(failure);
                            request.items.append(std::move(requestItem));
                            continue;
                        }
                        if (requestItem.planDigest !=
                            plan.get().value(QStringLiteral("plan_digest")).toString()) {
                            requestItem.validationError = AutomationError::invalidArgument(
                                QStringLiteral("items.plan_digest"),
                                QStringLiteral("The import plan digest is stale or invalid"));
                            request.items.append(std::move(requestItem));
                            continue;
                        }
                    }
                    requestItem.formatId = format.get().id;
                    requestItem.revalidatePlan = planRevalidator(
                        m_runtime, m_fileGuard, formats.get(), requestItem.canonicalPath,
                        QStringLiteral("import"), requestItem.formatId, requestItem.planDigest,
                        QStringLiteral("items.path"), QStringLiteral("items.plan_digest"));
                    request.items.append(std::move(requestItem));
                }
                return taskAcceptedResult(m_hostServices.importDocuments(request));
            });

        addBinding(ToolNames::formats_inspect, [this](const QJsonObject &arguments,
                                                      const PublicInvocationContext &) {
            auto formats = m_runtime.files().listFormats();
            if (!formats)
                return AutomationResult<QJsonObject>(formats.getError());
            return inspectFormatPath(m_runtime, formats.get(),
                                     arguments.value(QStringLiteral("path")).toString(),
                                     arguments.value(QStringLiteral("purpose")).toString());
        });

        addBinding(ToolNames::tracks_list, [this](const QJsonObject &arguments,
                                                  const PublicInvocationContext &) {
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            QJsonArray tracks;
            for (qsizetype index = 0; index < project.get().tracks.size(); ++index)
                tracks.append(encodeTrackSnapshot(project.get().tracks.at(index), index));
            auto page = paginateJson(
                m_collectionCursorCodec, tracks, arguments,
                QStringLiteral("editor-public-tracks/v1"),
                QJsonObject{
                    {QStringLiteral("document"), encodeDocumentVersion(project.get().document)},
                    {QStringLiteral("tracks"),   tracks                                       }
            });
            if (!page)
                return AutomationResult<QJsonObject>(page.getError());
            auto result =
                queryResult(project.get().document, QStringLiteral("tracks"), page.get().items);
            if (!page.get().nextCursor.isEmpty())
                result.insert(QStringLiteral("next_cursor"), page.get().nextCursor);
            return AutomationResult<QJsonObject>(std::move(result));
        });

        addBinding(ToolNames::tracks_get, [this](const QJsonObject &arguments,
                                                 const PublicInvocationContext &) {
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            const auto trackId = TrackId(arguments.value(QStringLiteral("track_id")).toInt());
            for (qsizetype index = 0; index < project.get().tracks.size(); ++index) {
                const auto &track = project.get().tracks.at(index);
                if (track.id == trackId) {
                    auto snapshot = encodeTrackSnapshot(track, index);
                    const auto language = !track.data.defaultLanguage.isEmpty()
                                              ? track.data.defaultLanguage
                                              : track.data.singerInfo.defaultLanguage();
                    snapshot.insert(QStringLiteral("voice_context"),
                                    encodeVoiceContext(track.data.singerInfo,
                                                       track.data.speakerInfo,
                                                       track.data.singerInfo,
                                                       track.data.speakerInfo, false, language));
                    return AutomationResult<QJsonObject>(queryResult(
                        project.get().document, QStringLiteral("snapshot"), std::move(snapshot)));
                }
            }
            return AutomationResult<QJsonObject>(AutomationError::notFound(
                {ObjectKind::Track, trackId.value()}, QStringLiteral("Track was not found")));
        });

        addBinding(ToolNames::tracks_set_gain,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().setTrackGain(
                           commandContext(arguments, invocation),
                           TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                           arguments.value(QStringLiteral("gain")).toDouble()));
                   });
        addBinding(ToolNames::tracks_set_pan,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().setTrackPan(
                           commandContext(arguments, invocation),
                           TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                           arguments.value(QStringLiteral("pan")).toDouble()));
                   });
        addBinding(ToolNames::tracks_set_mute,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().setTrackMute(
                           commandContext(arguments, invocation),
                           TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                           arguments.value(QStringLiteral("mute")).toBool()));
                   });
        addBinding(ToolNames::tracks_set_solo,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().setTrackSolo(
                           commandContext(arguments, invocation),
                           TrackId(arguments.value(QStringLiteral("track_id")).toInt()),
                           arguments.value(QStringLiteral("solo")).toBool()));
                   });

        addBinding(ToolNames::tracks_clear_voice,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.parameters().clearTrackVoice(
                           commandContext(arguments, invocation),
                           TrackId(arguments.value(QStringLiteral("track_id")).toInt())));
                   });

        addBinding(ToolNames::master_set_pan,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().setMasterPan(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("pan")).toDouble()));
                   });
        addBinding(ToolNames::master_set_mute,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().setMasterMute(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("mute")).toBool()));
                   });
        addBinding(ToolNames::master_set_solo,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.timeline().setMasterSolo(
                           commandContext(arguments, invocation),
                           arguments.value(QStringLiteral("solo")).toBool()));
                   });

        addBinding(ToolNames::clips_list, [this](const QJsonObject &arguments,
                                                 const PublicInvocationContext &) {
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            const auto trackFilter =
                arguments.contains(QStringLiteral("track_id"))
                    ? TrackId(arguments.value(QStringLiteral("track_id")).toInt())
                    : TrackId{};
            const auto typeFilter = arguments.value(QStringLiteral("type")).toString();
            const auto range = arguments.value(QStringLiteral("range")).toObject();
            const auto hasRange = !range.isEmpty();
            const auto rangeStart = range.value(QStringLiteral("start")).toInt();
            const auto rangeEnd = range.value(QStringLiteral("end")).toInt();
            QJsonArray clips;
            for (const auto &track : project.get().tracks) {
                if (trackFilter.isValid() && track.id != trackFilter)
                    continue;
                for (const auto &clip : track.clips) {
                    const auto type = clip.data.type == ClipDraftDto::Type::Singing
                                          ? QStringLiteral("singing")
                                          : QStringLiteral("audio");
                    if (!typeFilter.isEmpty() && type != typeFilter)
                        continue;
                    const auto start = clip.data.properties.start;
                    const auto end = start + clip.data.properties.length;
                    if (hasRange && (end <= rangeStart || start >= rangeEnd))
                        continue;
                    clips.append(encodeClipSnapshot(clip));
                }
            }
            auto page = paginateJson(
                m_collectionCursorCodec, clips, arguments, QStringLiteral("editor-public-clips/v1"),
                QJsonObject{
                    {QStringLiteral("document"), encodeDocumentVersion(project.get().document)},
                    {QStringLiteral("track_id"), arguments.value(QStringLiteral("track_id"))  },
                    {QStringLiteral("type"),     typeFilter                                   },
                    {QStringLiteral("range"),    range                                        },
                    {QStringLiteral("clips"),    clips                                        }
            });
            if (!page)
                return AutomationResult<QJsonObject>(page.getError());
            auto result =
                queryResult(project.get().document, QStringLiteral("clips"), page.get().items);
            if (!page.get().nextCursor.isEmpty())
                result.insert(QStringLiteral("next_cursor"), page.get().nextCursor);
            return AutomationResult<QJsonObject>(std::move(result));
        });

        addBinding(ToolNames::clips_get, [this](const QJsonObject &arguments,
                                                const PublicInvocationContext &) {
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
            for (const auto &track : project.get().tracks) {
                for (const auto &clip : track.clips) {
                    if (clip.id == clipId) {
                        auto snapshot = encodeClipSnapshot(clip);
                        if (clip.data.type == ClipDraftDto::Type::Singing) {
                            const auto effectiveSinger = clip.data.usesTrackVoiceContext
                                                             ? track.data.singerInfo
                                                             : clip.data.ownSingerInfo;
                            const auto effectiveSpeaker = clip.data.usesTrackVoiceContext
                                                              ? track.data.speakerInfo
                                                              : clip.data.ownSpeakerInfo;
                            auto language = clip.data.defaultLanguage;
                            if (language.isEmpty()) {
                                language = clip.data.usesTrackVoiceContext
                                               ? (!track.data.defaultLanguage.isEmpty()
                                                      ? track.data.defaultLanguage
                                                      : effectiveSinger.defaultLanguage())
                                               : effectiveSinger.defaultLanguage();
                            }
                            snapshot.insert(QStringLiteral("voice_context"),
                                            encodeVoiceContext(
                                                clip.data.ownSingerInfo, clip.data.ownSpeakerInfo,
                                                effectiveSinger, effectiveSpeaker,
                                                clip.data.usesTrackVoiceContext, language));
                        } else {
                            snapshot.insert(QStringLiteral("voice_context"),
                                            QJsonValue(QJsonValue::Null));
                        }
                        return AutomationResult<QJsonObject>(queryResult(project.get().document,
                                                                         QStringLiteral("snapshot"),
                                                                         std::move(snapshot)));
                    }
                }
            }
            return AutomationResult<QJsonObject>(AutomationError::notFound(
                {ObjectKind::Clip, clipId.value()}, QStringLiteral("Clip was not found")));
        });

        addBinding(ToolNames::clips_duplicate, [this](const QJsonObject &arguments,
                                                      const PublicInvocationContext &invocation) {
            const auto destination = arguments.value(QStringLiteral("destination")).toObject();
            ClipDuplicateDestinationDto decoded;
            if (destination.contains(QStringLiteral("target_track_id"))) {
                decoded.targetTrackId =
                    TrackId(destination.value(QStringLiteral("target_track_id")).toInt());
            }
            decoded.targetStart = destination.value(QStringLiteral("start")).toInt();
            return mutationResult(m_runtime.project().duplicateClips(
                commandContext(arguments, invocation),
                objectIds<ClipId>(arguments.value(QStringLiteral("clip_ids")).toArray()), decoded));
        });

        addBinding(ToolNames::clips_move, [this](const QJsonObject &arguments,
                                                 const PublicInvocationContext &invocation) {
            QList<ClipMoveDto> moves;
            for (const auto &value : arguments.value(QStringLiteral("moves")).toArray()) {
                const auto move = value.toObject();
                moves.append({ClipId(move.value(QStringLiteral("clip_id")).toInt()),
                              TrackId(move.value(QStringLiteral("target_track_id")).toInt()),
                              move.value(QStringLiteral("start")).toInt()});
            }
            return mutationResult(
                m_runtime.project().moveClips(commandContext(arguments, invocation), moves));
        });

        addBinding(ToolNames::clips_resize_left,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().resizeClipLeft(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           arguments.value(QStringLiteral("start")).toInt()));
                   });
        addBinding(ToolNames::clips_resize_right,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().resizeClipRight(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           arguments.value(QStringLiteral("end")).toInt()));
                   });
        addBinding(ToolNames::clips_set_gain,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().setClipGain(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           arguments.value(QStringLiteral("gain")).toDouble()));
                   });
        addBinding(ToolNames::clips_set_mute,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.project().setClipMute(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           arguments.value(QStringLiteral("mute")).toBool()));
                   });

        addBinding(ToolNames::clips_clear_voice,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.parameters().clearClipVoice(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt())));
                   });

        addBinding(ToolNames::audio_clips_get, [this](const QJsonObject &arguments,
                                                      const PublicInvocationContext &) {
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            auto document = m_runtime.documents().getDocument(documentId(arguments));
            if (!document)
                return AutomationResult<QJsonObject>(document.getError());
            const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
            for (const auto &track : project.get().tracks) {
                for (const auto &clip : track.clips) {
                    if (clip.id != clipId)
                        continue;
                    if (clip.data.type != ClipDraftDto::Type::Audio) {
                        return AutomationResult<QJsonObject>(AutomationError::wrongObjectType(
                            {ObjectKind::Clip, clipId.value()},
                            QStringLiteral("Clip is not an audio clip")));
                    }
                    return AutomationResult<QJsonObject>(
                        queryResult(project.get().document, QStringLiteral("snapshot"),
                                    encodeAudioClipSnapshot(clip, document.get().path)));
                }
            }
            return AutomationResult<QJsonObject>(AutomationError::notFound(
                {ObjectKind::Clip, clipId.value()}, QStringLiteral("Clip was not found")));
        });

        addBinding(ToolNames::speaker_mix_keyframes_insert,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       std::optional<QVector<double>> weights;
                       if (arguments.contains(QStringLiteral("weights"))) {
                           weights.emplace();
                           for (const auto &value :
                                arguments.value(QStringLiteral("weights")).toArray())
                               weights->append(value.toDouble());
                       }
                       return mutationResult(m_runtime.parameters().insertSpeakerMixKeyframe(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           arguments.value(QStringLiteral("position")).toInt(), weights));
                   });
        addBinding(
            ToolNames::speaker_mix_keyframes_move,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                QList<QPair<SpeakerMixKeyframeId, int>> moves;
                for (const auto &value : arguments.value(QStringLiteral("moves")).toArray()) {
                    const auto move = value.toObject();
                    moves.append(
                        {SpeakerMixKeyframeId(move.value(QStringLiteral("keyframe_id")).toInt()),
                         move.value(QStringLiteral("position")).toInt()});
                }
                return mutationResult(m_runtime.parameters().moveSpeakerMixKeyframes(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()), moves));
            });
        addBinding(
            ToolNames::speaker_mix_keyframes_set_weights,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                QVector<double> weights;
                for (const auto &value : arguments.value(QStringLiteral("weights")).toArray())
                    weights.append(value.toDouble());
                return mutationResult(m_runtime.parameters().setSpeakerMixKeyframeWeights(
                    commandContext(arguments, invocation),
                    ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                    SpeakerMixKeyframeId(arguments.value(QStringLiteral("keyframe_id")).toInt()),
                    std::move(weights)));
            });
        addBinding(ToolNames::speaker_mix_keyframes_remove,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.parameters().removeSpeakerMixKeyframes(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           objectIds<SpeakerMixKeyframeId>(
                               arguments.value(QStringLiteral("keyframe_ids")).toArray())));
                   });

        addBinding(ToolNames::notes_search, [this](const QJsonObject &arguments,
                                                   const PublicInvocationContext &) {
            auto matches = m_runtime.notes().searchNotes(
                documentId(arguments), ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                arguments.value(QStringLiteral("query")).toString(),
                arguments.value(QStringLiteral("mode")).toString(),
                arguments.value(QStringLiteral("case_sensitive")).toBool(false),
                arguments.value(QStringLiteral("regex")).toBool(false));
            if (!matches)
                return AutomationResult<QJsonObject>(matches.getError());
            QJsonArray encoded;
            for (const auto &match : matches.get()) {
                encoded.append(QJsonObject{
                    {QStringLiteral("note_id"),     match.noteId.value()},
                    {QStringLiteral("local_start"), match.localStart    },
                    {QStringLiteral("length"),      match.length        },
                    {QStringLiteral("lyric"),       match.lyric         },
                });
            }
            return AutomationResult<QJsonObject>(
                queryResult(m_runtime.documentVersion(), QStringLiteral("matches"), encoded));
        });

        addBinding(ToolNames::notes_duplicate,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().duplicateNotes(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("source_clip_id")).toInt()),
                           objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                           ClipId(arguments.value(QStringLiteral("target_clip_id")).toInt()),
                           arguments.value(QStringLiteral("target_start")).toInt()));
                   });

        addBinding(ToolNames::notes_set_language,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       const auto clipId =
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
                       auto project = m_runtime.project().getProject(documentId(arguments));
                       if (!project)
                           return AutomationResult<QJsonObject>(project.getError());
                       const auto language =
                           decodeLanguageSelection(arguments.value(QStringLiteral("language")), {});
                       return mutationResult(m_runtime.notes().setLanguages(
                           commandContext(arguments, invocation), clipId,
                           objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray()),
                           language));
                   });

        addBinding(ToolNames::notes_set_pronunciation,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().setPronunciation(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           NoteId(arguments.value(QStringLiteral("note_id")).toInt()),
                           arguments.value(QStringLiteral("source")).toString() ==
                               QStringLiteral("original"),
                           arguments.value(QStringLiteral("pronunciation")).toString()));
                   });

        addBinding(ToolNames::notes_reset_pronunciation,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().resetPronunciation(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           NoteId(arguments.value(QStringLiteral("note_id")).toInt())));
                   });

        addBinding(
            ToolNames::notes_set_phonemes,
            [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
                const auto noteId = NoteId(arguments.value(QStringLiteral("note_id")).toInt());
                auto notes = m_runtime.notes().getNotes(documentId(arguments), clipId);
                if (!notes)
                    return AutomationResult<QJsonObject>(notes.getError());
                std::optional<NoteSnapshotDto> selectedNote;
                for (const auto &note : notes.get()) {
                    if (note.id == noteId) {
                        selectedNote = note;
                        break;
                    }
                }
                if (!selectedNote) {
                    return AutomationResult<QJsonObject>(AutomationError::notFound(
                        {ObjectKind::Note, noteId.value()}, QStringLiteral("Note was not found")));
                }
                auto language = selectedNote->data.language;
                if (language.isEmpty()) {
                    auto project = m_runtime.project().getProject(documentId(arguments));
                    if (!project)
                        return AutomationResult<QJsonObject>(project.getError());
                    language = effectiveDefaultLanguage(project.get(), clipId);
                }
                auto phonemes = selectedNote->data.phonemes;
                phonemes.nameSeq.edited.clear();
                for (const auto &value : arguments.value(QStringLiteral("names")).toArray()) {
                    PhonemeName name;
                    name.language = language;
                    name.name = value.toString();
                    phonemes.nameSeq.edited.append(std::move(name));
                }
                phonemes.offsetSeq.edited.clear();
                return mutationResult(m_runtime.notes().setPhonemes(
                    commandContext(arguments, invocation), clipId, noteId, phonemes));
            });

        addBinding(ToolNames::notes_reset_phonemes,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &invocation) {
                       return mutationResult(m_runtime.notes().resetPhonemes(
                           commandContext(arguments, invocation),
                           ClipId(arguments.value(QStringLiteral("clip_id")).toInt()),
                           NoteId(arguments.value(QStringLiteral("note_id")).toInt())));
                   });

        addBinding(ToolNames::notes_fill_lyrics, [this](const QJsonObject &arguments,
                                                        const PublicInvocationContext &invocation) {
            const auto clipId = ClipId(arguments.value(QStringLiteral("clip_id")).toInt());
            auto notes = m_runtime.notes().getNotes(documentId(arguments), clipId);
            if (!notes)
                return AutomationResult<QJsonObject>(notes.getError());
            QSet<int> selected;
            const auto selectedIds =
                objectIds<NoteId>(arguments.value(QStringLiteral("note_ids")).toArray());
            for (const auto id : selectedIds) {
                if (selected.contains(id.value())) {
                    return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                        QStringLiteral("note_ids"),
                        QStringLiteral("Fill-lyrics note IDs must be unique")));
                }
                selected.insert(id.value());
            }
            QList<NoteSnapshotDto> ordered;
            for (const auto &note : notes.get()) {
                if (selected.contains(note.id.value()))
                    ordered.append(note);
            }
            std::sort(ordered.begin(), ordered.end(), [](const auto &left, const auto &right) {
                return left.data.localStart < right.data.localStart;
            });
            if (ordered.size() != selected.size()) {
                return AutomationResult<QJsonObject>(AutomationError::notFound(
                    {ObjectKind::Note, -1}, QStringLiteral("A selected note was not found")));
            }
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            const auto options = arguments.value(QStringLiteral("options")).toObject();
            const auto defaultLanguage = effectiveDefaultLanguage(project.get(), clipId);
            const auto languageSelection = options.value(QStringLiteral("language")).toObject();
            const bool followSinger =
                options.contains(QStringLiteral("language")) &&
                languageSelection.value(QStringLiteral("mode")) == QStringLiteral("follow_singer");
            const auto languageOverride =
                options.contains(QStringLiteral("language"))
                    ? decodeLanguageSelection(options.value(QStringLiteral("language")),
                                              defaultLanguage)
                    : QString();
            if (languageSelection.value(QStringLiteral("mode")) == QStringLiteral("explicit")) {
                bool supported = false;
                for (const auto &track : project.get().tracks) {
                    for (const auto &clip : track.clips) {
                        if (clip.id != clipId)
                            continue;
                        const auto &singer = clip.data.usesTrackVoiceContext
                                                 ? track.data.singerInfo
                                                 : clip.data.ownSingerInfo;
                        const auto languages = singer.languages();
                        supported = std::any_of(languages.cbegin(), languages.cend(),
                                                [&languageOverride](const auto &language) {
                                                    return language.id() == languageOverride;
                                                });
                    }
                }
                if (!supported) {
                    return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                        QStringLiteral("/options/language/language_id"),
                        QStringLiteral("The requested lyric language is unavailable")));
                }
            }
            QStringList priority{languageOverride.isEmpty() ? defaultLanguage : languageOverride};
            std::vector<std::string> priorityLanguages;
            for (const auto &language : std::as_const(priority)) {
                if (!language.isEmpty())
                    priorityLanguages.push_back(language.toStdString());
            }
            const auto splitterId =
                options.value(QStringLiteral("splitter_id")).toString(QStringLiteral("auto"));
            const auto taggerId =
                options.value(QStringLiteral("tagger_id")).toString(QStringLiteral("auto"));
            if (taggerId != QStringLiteral("auto")) {
                return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                    QStringLiteral("options.tagger_id"),
                    QStringLiteral("The requested lyric tagger is unavailable")));
            }
            QList<QList<LangNote>> split;
            if (splitterId == QStringLiteral("character")) {
                split = FillLyric::LyricSplitter::splitByChar(
                    arguments.value(QStringLiteral("text")).toString(), priorityLanguages);
            } else if (splitterId == QStringLiteral("auto")) {
                split = FillLyric::LyricSplitter::splitAuto(
                    arguments.value(QStringLiteral("text")).toString(), priorityLanguages);
            } else {
                return AutomationResult<QJsonObject>(AutomationError::invalidArgument(
                    QStringLiteral("options.splitter_id"),
                    QStringLiteral("The requested lyric splitter is unavailable")));
            }
            QList<LangNote> lyrics;
            for (const auto &line : split) {
                for (const auto &word : line)
                    lyrics.append(word);
            }
            const bool skipSlur = options.value(QStringLiteral("skip_slur")).toBool(false);
            QList<NoteWordEditDto> edits;
            qsizetype lyricIndex = 0;
            for (const auto &note : ordered) {
                if (skipSlur && Note::isSlurLyric(note.data.lyric))
                    continue;
                if (lyricIndex >= lyrics.size())
                    break;
                const auto &word = lyrics.at(lyricIndex++);
                NoteWordEditDto edit;
                edit.noteId = note.id;
                edit.lyric = word.lyric;
                edit.language =
                    followSinger ? QString()
                                 : (languageOverride.isEmpty() ? word.language : languageOverride);
                edit.pronunciation = Pronunciation(word.syllable, word.syllableRevised);
                edit.pronunciationCandidates = word.candidates;
                edit.phonemes =
                    (Note::isSlurLyric(edit.lyric) || Note::isSyllabificationLyric(edit.lyric))
                        ? Phonemes{}
                        : note.data.phonemes;
                edit.replacePronunciation = word.revised;
                edit.replacePronunciationCandidates = true;
                edits.append(std::move(edit));
            }
            return mutationResult(m_runtime.notes().setWordProperties(
                commandContext(arguments, invocation), clipId, edits));
        });

        addBinding(ToolNames::exports_midi_get_capabilities,
                   [this](const QJsonObject &arguments, const PublicInvocationContext &) {
                       auto project = m_runtime.project().getProject(documentId(arguments));
                       if (!project)
                           return AutomationResult<QJsonObject>(project.getError());
                       QJsonArray trackIds;
                       QJsonArray clipIds;
                       for (const auto &track : project.get().tracks) {
                           trackIds.append(track.id.value());
                           for (const auto &clip : track.clips) {
                               if (clip.data.type == ClipDraftDto::Type::Singing)
                                   clipIds.append(clip.id.value());
                           }
                       }
                       QJsonObject optionSchema;
                       if (const auto *contract =
                               AutomationWire::findPublicTool(OperationIds::exports::midi::start)) {
                           optionSchema = contract->inputSchema.value(QStringLiteral("properties"))
                                              .toObject()
                                              .value(QStringLiteral("options"))
                                              .toObject();
                       }
                       return AutomationResult<QJsonObject>(queryResult(
                           project.get().document, QStringLiteral("capabilities"),
                           QJsonObject{
                               {QStringLiteral("track_ids"),        trackIds                         },
                               {QStringLiteral("clip_ids"),         clipIds                          },
                               {QStringLiteral("formats"),          QJsonArray{QStringLiteral("mid")}},
                               {QStringLiteral("lyrics_supported"), true                             },
                               {QStringLiteral("option_schema"),    optionSchema                     },
                       }));
                   });

        addBinding(ToolNames::exports_midi_preview, [this](const QJsonObject &arguments,
                                                           const PublicInvocationContext &) {
            auto project = m_runtime.project().getProject(documentId(arguments));
            if (!project)
                return AutomationResult<QJsonObject>(project.getError());
            const auto options = arguments.value(QStringLiteral("options")).toObject();
            auto decoded = decodeMidiExportOptions(project.get(), options);
            if (!decoded)
                return AutomationResult<QJsonObject>(decoded.getError());

            auto validated = m_runtime.files().previewMidiExport(
                documentId(arguments), arguments.value(QStringLiteral("path")).toString(),
                decoded.get());
            if (!validated)
                return AutomationResult<QJsonObject>(validated.getError());

            QJsonArray diagnostics;
            const auto path = arguments.value(QStringLiteral("path")).toString();
            if (QFileInfo::exists(path)) {
                diagnostics.append(QJsonObject{
                    {QStringLiteral("code"),     QStringLiteral("target_exists")                  },
                    {QStringLiteral("message"),
                     QStringLiteral("The target already exists and requires overwrite permission")},
                    {QStringLiteral("blocking"), true                                             },
                });
            }
            const auto selection = effectiveMidiSelection(project.get(), decoded.get());
            QJsonObject plan{
                {QStringLiteral("targets"),                 QJsonArray{path}                   },
                {QStringLiteral("diagnostics"),             diagnostics                        },
                {QStringLiteral("track_ids"),               selection.first                    },
                {QStringLiteral("clip_ids"),                selection.second                   },
                {QStringLiteral("include_tempo"),           decoded.get().includeTempo         },
                {QStringLiteral("include_time_signatures"), decoded.get().includeTimeSignatures},
                {QStringLiteral("include_lyrics"),          decoded.get().includeLyrics        },
            };
            plan.insert(QStringLiteral("plan_digest"), AutomationWire::sha256Digest(plan));
            return AutomationResult<QJsonObject>(
                queryResult(project.get().document, QStringLiteral("plan"), plan));
        });

        addBinding(ToolNames::inference_get_status, [this](const QJsonObject &arguments,
                                                           const PublicInvocationContext &) {
            const auto document = documentId(arguments);
            auto snapshot = m_runtime.documents().getDocument(document);
            if (!snapshot)
                return AutomationResult<QJsonObject>(snapshot.getError());
            if (!m_hostServices.inferenceStatus) {
                return AutomationResult<QJsonObject>(
                    unavailable(QStringLiteral("Inference status service is unavailable")));
            }
            auto status = m_hostServices.inferenceStatus(
                document, arguments.value(QStringLiteral("scope")).toObject());
            if (!status)
                return AutomationResult<QJsonObject>(status.getError());
            return AutomationResult<QJsonObject>(
                queryResult(snapshot.get().document, QStringLiteral("status"), status.get()));
        });

        registerAdvancedGuiBindings();
        registerAdvancedApplicationBindings();

        Q_ASSERT(m_handlers.size() == contracts().size());
        Q_ASSERT(isComplete());
    }

} // namespace Automation
