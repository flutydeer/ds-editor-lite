#include "Automation/Mcp/McpRequestDispatcher.h"
#include "Automation/OperationIds.h"
#include "Automation/Public/AdmissionController.h"
#include "Automation/Public/AutomationAccessPolicy.h"
#include "Automation/Public/AutomationFileGuard.h"
#include "Automation/Public/PublicAutomationRegistry.h"
#include "../PublicAutomationToolsetExpectations.h"
#include "TestRuntime.h"

#include <lite/AutomationWire/McpProtocol.h>
#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/PublicConstants.h>
#include <lite/AutomationWire/PublicToolContract.h>
#include <lite/ProjectConverters/MidiConverter.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVersionNumber>

#include <algorithm>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <utility>

namespace {
    namespace Mcp = AutomationWire::Mcp;

    int failures = 0;

    struct PackageRefreshTestControl {
        QString privatePath;
        int starts = 0;
        bool deferNext = false;
        bool failNext = false;
        Automation::PackageRefreshCommitGate pendingCommitGate;
        Automation::PackageRefreshCompletion pendingCompletion;
    };

    class PreviewAudioExportJob final : public Automation::IAudioExportJob {
    public:
        explicit PreviewAudioExportJob(Automation::AudioExportConfigDto config)
            : m_config(std::move(config)) {
        }

        Automation::AudioExportPreviewDto preview() const override {
            return {
                .baseDirectory = m_config.fileDirectory,
                .filePaths = {QDir(m_config.fileDirectory).filePath(m_config.fileName)},
                .warningFlags =
                    m_config.fileName == QStringLiteral("preview.wav")
                        ? Automation::AudioExportDuplicatedFile | Automation::AudioExportLossyFormat
                        : 0,
            };
        }

        Automation::AudioExportBackendResult waitUntilReady() override {
            return {.state = Automation::AudioExportBackendState::Succeeded};
        }

        Automation::AudioExportBackendResult
            execute(const Automation::AudioExportObserver &) override {
            return {.state = Automation::AudioExportBackendState::Succeeded};
        }

        void cancel() override {
        }

        void cleanup() override {
        }

    private:
        Automation::AudioExportConfigDto m_config;
    };

    void expect(const bool condition, const QString &message) {
        if (condition)
            return;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        ++failures;
    }

    void reportFailure(const QString &operationId,
                       const Automation::AutomationResult<QJsonObject> &result) {
        if (result)
            return;
        const auto &failure = result.getError();
        QTextStream(stderr) << "DETAIL " << operationId << ": "
                            << Automation::errorCodeName(failure.code) << ", " << failure.fieldPath
                            << ", " << failure.message << Qt::endl;
    }

    QJsonValue schemaSample(const QJsonObject &schema, const QString &field,
                            const Automation::DocumentVersion &document) {
        if (schema.contains(QStringLiteral("const")))
            return schema.value(QStringLiteral("const"));
        const auto choices = schema.value(QStringLiteral("enum")).toArray();
        if (!choices.isEmpty())
            return choices.first();
        const auto branches = schema.value(QStringLiteral("oneOf")).toArray();
        if (!branches.isEmpty())
            return schemaSample(branches.first().toObject(), field, document);
        const auto type = schema.value(QStringLiteral("type")).toString();
        if (type == QStringLiteral("object")) {
            QJsonObject result;
            const auto properties = schema.value(QStringLiteral("properties")).toObject();
            for (const auto &required : schema.value(QStringLiteral("required")).toArray()) {
                const auto name = required.toString();
                result.insert(name,
                              schemaSample(properties.value(name).toObject(), name, document));
            }
            const auto minimumProperties =
                schema.value(QStringLiteral("minProperties")).toInteger();
            for (auto it = properties.begin();
                 result.size() < minimumProperties && it != properties.end(); ++it) {
                if (!result.contains(it.key()))
                    result.insert(it.key(),
                                  schemaSample(it.value().toObject(), it.key(), document));
            }
            return result;
        }
        if (type == QStringLiteral("array")) {
            QJsonArray result;
            const auto count =
                std::max<qint64>(1, schema.value(QStringLiteral("minItems")).toInteger());
            for (qint64 index = 0; index < count; ++index)
                result.append(schemaSample(schema.value(QStringLiteral("items")).toObject(), field,
                                           document));
            return result;
        }
        if (type == QStringLiteral("boolean"))
            return false;
        if (type == QStringLiteral("integer") || type == QStringLiteral("number")) {
            if (field == QStringLiteral("expected_revision"))
                return static_cast<qint64>(document.revision);
            return schema.contains(QStringLiteral("minimum"))
                       ? schema.value(QStringLiteral("minimum"))
                       : QJsonValue(0);
        }
        if (type == QStringLiteral("null"))
            return QJsonValue(QJsonValue::Null);
        if (field == QStringLiteral("document_id"))
            return document.documentId.toString();
        if (field == QStringLiteral("task_id") ||
            schema.value(QStringLiteral("format")).toString() == QStringLiteral("uuid")) {
            return QStringLiteral("00000000-0000-4000-8000-000000000001");
        }
        return QStringLiteral("x");
    }

    QJsonObject validInputSample(const AutomationWire::ToolContract &contract,
                                 const Automation::DocumentVersion &document) {
        return schemaSample(contract.inputSchema, {}, document).toObject();
    }

    Automation::PublicAutomationHostServices hostServices(Automation::CoreRuntime &runtime) {
        Automation::PublicAutomationHostServices services;
        const auto startTask = [&runtime](const Automation::CommandContext &command,
                                          const QString &operation) {
            auto taskId = std::make_shared<Automation::TaskId>();
            const auto task = runtime.automationTasks().createTask(
                operation, command.expected, std::nullopt,
                [&runtime, taskId] { runtime.automationTasks().cancel(*taskId); },
                command.clientId);
            *taskId = task.taskId;
            return Automation::AutomationResult<Automation::TaskAcceptedResult>(
                Automation::TaskAcceptedResult{task.taskId, command.expected, false});
        };
        services.openDocument = [startTask](const Automation::PublicDocumentOpenRequest &request) {
            return startTask(request.command, QStringLiteral("documents.open"));
        };
        services.importDocument =
            [startTask](const Automation::PublicDocumentImportRequest &request) {
                return startTask(request.command, QStringLiteral("documents.import"));
            };
        services.importAudioClip =
            [startTask](const Automation::PublicAudioClipImportRequest &request) {
                return startTask(request.command, QStringLiteral("audio_clips.import"));
            };
        services.importAudioClips =
            [startTask](const Automation::PublicAudioClipBatchImportRequest &request) {
                return startTask(request.command, QStringLiteral("audio_clips.import_batch"));
            };
        services.audioExportCapabilities = [&runtime](const Automation::DocumentId &documentId) {
            QJsonArray sources;
            const auto project = runtime.project().getProject(documentId);
            if (project) {
                for (const auto &track : project.get().tracks) {
                    sources.append(QJsonObject{
                        {QStringLiteral("id"),        track.id.value()       },
                        {QStringLiteral("name"),
                         track.data.name.isEmpty()
                             ? QStringLiteral("Track %1").arg(track.id.value())
                             : track.data.name                               },
                        {QStringLiteral("kind"),      QStringLiteral("track")},
                        {QStringLiteral("available"), true                   },
                    });
                }
            }
            return Automation::AutomationResult<QJsonValue>(QJsonObject{
                {QStringLiteral("formats"),       QJsonArray{QStringLiteral("wav")}   },
                {QStringLiteral("sample_rates"),  QJsonArray{44100}                   },
                {QStringLiteral("channel_modes"), QJsonArray{QStringLiteral("stereo")}},
                {QStringLiteral("mixing_modes"),  QJsonArray{QStringLiteral("mixed")} },
                {QStringLiteral("source_modes"),
                 QJsonArray{QStringLiteral("all"), QStringLiteral("custom")}          },
                {QStringLiteral("sources"),       sources                             },
            });
        };
        services.extractionCapabilities = [](const Automation::DocumentId &,
                                             const Automation::ClipId clipId) {
            const auto optionSchema = [](const QString &operationId) {
                const auto *contract = AutomationWire::findPublicTool(operationId);
                return AutomationWire::JsonSchema::document(
                    contract->inputSchema.value(QStringLiteral("properties"))
                        .toObject()
                        .value(QStringLiteral("options"))
                        .toObject());
            };
            return Automation::AutomationResult<QJsonValue>(QJsonObject{
                {QStringLiteral("source_audio_clip_id"), clipId.value()                  },
                {QStringLiteral("pitch"),
                 QJsonObject{
                     {QStringLiteral("supported"), true},
                     {QStringLiteral("source_supported"), false},
                     {QStringLiteral("available"), false},
                     {QStringLiteral("module_state"), QStringLiteral("unavailable")},
                     {QStringLiteral("unavailable_reason"), QStringLiteral("test unavailable")},
                     {QStringLiteral("models"), QJsonArray{}},
                     {QStringLiteral("option_schema"),
                      optionSchema(QStringLiteral("extract.pitch.start"))},
                     {QStringLiteral("range_support"),
                      QJsonObject{
                          {QStringLiteral("source_range"), QStringLiteral("visible_audio_clip")},
                          {QStringLiteral("custom_frequency"), false},
                      }},
                 }                                                                       },
                {QStringLiteral("midi"),
                 QJsonObject{
                     {QStringLiteral("supported"), true},
                     {QStringLiteral("source_supported"), false},
                     {QStringLiteral("available"), false},
                     {QStringLiteral("module_state"), QStringLiteral("unavailable")},
                     {QStringLiteral("unavailable_reason"), QStringLiteral("test unavailable")},
                     {QStringLiteral("models"), QJsonArray{}},
                     {QStringLiteral("option_schema"),
                      optionSchema(QStringLiteral("extract.midi.start"))},
                     {QStringLiteral("range_support"),
                      QJsonObject{
                          {QStringLiteral("source_range"), QStringLiteral("visible_audio_clip")},
                          {QStringLiteral("minimum_note_length"),
                           QJsonObject{
                               {QStringLiteral("minimum"), 1},
                               {QStringLiteral("maximum"), std::numeric_limits<int>::max()},
                           }},
                      }},
                 }                                                                       },
                {QStringLiteral("languages"),            QJsonArray{QStringLiteral("zh")}},
            });
        };
        services.inferenceCapabilities = [](const Automation::DocumentId &,
                                            const QJsonObject &scope) {
            return Automation::AutomationResult<QJsonValue>(QJsonObject{
                {QStringLiteral("scope"),     scope                                 },
                {QStringLiteral("stages"),    QJsonArray{QStringLiteral("duration")}},
                {QStringLiteral("providers"), QJsonArray{}                          },
                {QStringLiteral("devices"),   QJsonArray{}                          },
                {QStringLiteral("models"),    QJsonArray{}                          },
            });
        };
        services.inferenceStatus = [](const Automation::DocumentId &, const QJsonObject &scope) {
            QJsonArray stages;
            for (const auto &stage : Automation::InferenceAutomationFacade::supportedStages()) {
                stages.append(QJsonObject{
                    {QStringLiteral("stage"),   stage                       },
                    {QStringLiteral("state"),   QStringLiteral("idle")      },
                    {QStringLiteral("reason"),  QString()                   },
                    {QStringLiteral("task_id"), QJsonValue(QJsonValue::Null)},
                });
            }
            return Automation::AutomationResult<QJsonValue>(QJsonObject{
                {QStringLiteral("scope"),  scope },
                {QStringLiteral("stages"), stages},
            });
        };
        services.startInference =
            [startTask](const Automation::PublicInferenceStartRequest &request) {
                return startTask(request.command, QStringLiteral("inference.start"));
            };
        services.resetInferenceStage = [](const Automation::PublicInferenceResetRequest &request) {
            Automation::MutationResult result;
            result.previous = request.command.expected;
            result.current = request.command.expected;
            result.validatedOnly = request.command.validateOnly;
            return Automation::AutomationResult<Automation::MutationResult>(result);
        };
        services.documentStatus = [&runtime] {
            const auto document = runtime.documentVersion();
            const QJsonObject entry{
                {QStringLiteral("document_id"), document.documentId.toString()        },
                {QStringLiteral("revision"),    static_cast<qint64>(document.revision)},
                {QStringLiteral("active"),      true                                  },
            };
            return QJsonArray{entry, entry};
        };
        services.windowStatus = [&runtime] {
            const QJsonObject entry{
                {QStringLiteral("window_id"),   QStringLiteral("registry-window")              },
                {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
                {QStringLiteral("active"),      true                                           },
            };
            return QJsonArray{entry, entry};
        };
        return services;
    }

    QJsonObject commandArguments(const Automation::DocumentVersion &document) {
        return {
            {QStringLiteral("document_id"),       document.documentId.toString()        },
            {QStringLiteral("expected_revision"), static_cast<qint64>(document.revision)},
        };
    }

    QList<int> createdObjectIds(const QJsonObject &mutation, const QString &kind) {
        QList<int> result;
        for (const auto &value : mutation.value(QStringLiteral("created_objects")).toArray()) {
            const auto object = value.toObject().value(QStringLiteral("object")).toObject();
            if (object.value(QStringLiteral("kind")).toString() == kind)
                result.append(object.value(QStringLiteral("id")).toInt(-1));
        }
        return result;
    }

    QJsonValue resolvedValue(const QJsonObject &mutation, const QString &fieldPath) {
        for (const auto &value : mutation.value(QStringLiteral("resolved_values")).toArray()) {
            const auto resolved = value.toObject();
            if (resolved.value(QStringLiteral("field_path")).toString() == fieldPath)
                return resolved.value(QStringLiteral("value"));
        }
        return QJsonValue(QJsonValue::Undefined);
    }

    std::optional<Automation::TrackSnapshotDto> trackSnapshot(Automation::CoreRuntime &runtime,
                                                              const Automation::TrackId trackId) {
        const auto project = runtime.project().getProject(runtime.documentVersion().documentId);
        if (!project)
            return std::nullopt;
        for (const auto &track : project.get().tracks) {
            if (track.id == trackId)
                return track;
        }
        return std::nullopt;
    }

    std::optional<Automation::ClipSnapshotDto> clipSnapshot(Automation::CoreRuntime &runtime,
                                                            const Automation::ClipId clipId) {
        const auto project = runtime.project().getProject(runtime.documentVersion().documentId);
        if (!project)
            return std::nullopt;
        for (const auto &track : project.get().tracks) {
            for (const auto &clip : track.clips) {
                if (clip.id == clipId)
                    return clip;
            }
        }
        return std::nullopt;
    }

    QJsonObject mergeCommandArguments(const Automation::DocumentVersion &document,
                                      const QJsonObject &operationArguments) {
        auto result = commandArguments(document);
        for (auto it = operationArguments.constBegin(); it != operationArguments.constEnd(); ++it)
            result.insert(it.key(), it.value());
        return result;
    }

    QJsonObject publicTrackSnapshot(Automation::PublicAutomationRegistry &registry,
                                    Automation::CoreRuntime &runtime,
                                    const Automation::TrackId trackId) {
        const auto result = registry.invoke(
            QStringLiteral("tracks.get"),
            QJsonObject{
                {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
                {QStringLiteral("track_id"),    trackId.value()                                },
        });
        return result ? result.get().value(QStringLiteral("snapshot")).toObject() : QJsonObject{};
    }

    QJsonObject publicClipSnapshot(Automation::PublicAutomationRegistry &registry,
                                   Automation::CoreRuntime &runtime,
                                   const Automation::ClipId clipId) {
        const auto result = registry.invoke(
            QStringLiteral("clips.get"),
            QJsonObject{
                {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
                {QStringLiteral("clip_id"),     clipId.value()                                 },
        });
        return result ? result.get().value(QStringLiteral("snapshot")).toObject() : QJsonObject{};
    }

    QJsonObject publicMasterSnapshot(Automation::PublicAutomationRegistry &registry,
                                     Automation::CoreRuntime &runtime) {
        const auto result = registry.invoke(
            QStringLiteral("master.get"),
            QJsonObject{
                {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()}
        });
        return result ? result.get().value(QStringLiteral("snapshot")).toObject() : QJsonObject{};
    }

    void verifyScalarEdit(Automation::PublicAutomationRegistry &registry,
                          Automation::CoreRuntime &runtime, const QString &operationId,
                          const QJsonObject &target, const QString &inputField,
                          const QJsonValue &value, const QString &outputField,
                          const std::function<QJsonObject()> &snapshot, const QString &label) {
        const auto beforeSnapshot = snapshot();
        auto arguments = mergeCommandArguments(runtime.documentVersion(), target);
        arguments.insert(inputField, value);
        const auto before = runtime.documentVersion();
        const auto changed = registry.invoke(operationId, arguments,
                                             {.clientId = QStringLiteral("scalar-") + operationId});
        const auto afterChange = runtime.documentVersion();
        const auto afterSnapshot = snapshot();
        const auto undo = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("scalar-undo-") + operationId});
        const auto restoredSnapshot = snapshot();
        const auto valid = changed && changed.get().value(QStringLiteral("changed")).toBool() &&
                           afterChange.revision == before.revision + 1 &&
                           runtime.documentVersion().revision == before.revision + 2 &&
                           afterSnapshot.value(outputField) == value && undo &&
                           undo.get().value(QStringLiteral("changed")).toBool() &&
                           restoredSnapshot.value(outputField) == beforeSnapshot.value(outputField);
        if (!valid) {
            QTextStream(stderr)
                << "DETAIL " << operationId << ": changed=" << bool(changed)
                << (changed ? QString() : QStringLiteral(", error=") + changed.getError().message)
                << ", revisions=" << before.revision << '/' << afterChange.revision << '/'
                << runtime.documentVersion().revision
                << ", observed=" << afterSnapshot.value(outputField).toVariant().toString()
                << ", expected=" << value.toVariant().toString() << ", undo=" << bool(undo)
                << ", restored=" << restoredSnapshot.value(outputField).toVariant().toString()
                << ", original=" << beforeSnapshot.value(outputField).toVariant().toString()
                << Qt::endl;
        }
        expect(valid, label + QStringLiteral(" must be one independently undoable History edit"));
    }

    QList<int> trackOrder(Automation::CoreRuntime &runtime) {
        QList<int> result;
        const auto project = runtime.project().getProject(runtime.documentVersion().documentId);
        if (!project)
            return result;
        for (const auto &track : project.get().tracks)
            result.append(track.id.value());
        return result;
    }

    QJsonObject expectedDocumentStatistics(const Automation::ProjectSnapshotDto &project) {
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
                lengthTicks = std::max(lengthTicks, static_cast<qint64>(properties.start) +
                                                        properties.clipStart + properties.clipLen);
                if (clip.data.type == Automation::ClipDraftDto::Type::Singing) {
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

    struct PublicEditingFixture {
        Automation::TrackId trackId;
        Automation::TrackId batchTrackA;
        Automation::TrackId batchTrackB;
        Automation::ClipId scalarClipId;
        Automation::ClipId noteClipId;
        Automation::ClipId mixClipId;
        QList<Automation::NoteId> noteIds;
    };

    std::optional<QJsonObject> invokeSchemaValid(Automation::PublicAutomationRegistry &registry,
                                                 const QString &operationId,
                                                 const QJsonObject &arguments,
                                                 const QString &label) {
        const auto result = registry.invoke(
            operationId, arguments, {.clientId = QStringLiteral("registry-l3-") + operationId});
        reportFailure(operationId, result);
        const auto *contract = AutomationWire::findPublicTool(operationId);
        bool schemaValid = false;
        if (result && contract) {
            const auto validation =
                AutomationWire::validateJsonValue(result.get(), contract->outputSchema);
            schemaValid = validation.valid();
            if (!schemaValid && !validation.issues.isEmpty()) {
                const auto &issue = validation.issues.first();
                QTextStream(stderr)
                    << "DETAIL " << operationId << " output schema: " << issue.schemaPath << ", "
                    << issue.instancePath << ", " << issue.message
                    << ", output=" << QJsonDocument(result.get()).toJson(QJsonDocument::Compact)
                    << Qt::endl;
            }
        }
        expect(schemaValid, label + QStringLiteral(" must return its declared output schema"));
        return result ? std::optional<QJsonObject>(result.get()) : std::nullopt;
    }

    void verifyAdvancedGuiBindings(Automation::PublicAutomationRegistry &registry,
                                   Automation::CoreRuntime &runtime,
                                   const PublicEditingFixture &fixture) {
        const auto before = runtime.documentVersion();
        const QJsonObject window{
            {QStringLiteral("window_id"), runtime.windowId().toString()},
        };
        const QJsonObject document{
            {QStringLiteral("window_id"),   runtime.windowId().toString()},
            {QStringLiteral("document_id"), before.documentId.toString() },
        };
        QJsonObject revision = document;
        revision.insert(QStringLiteral("expected_revision"), static_cast<qint64>(before.revision));
        const auto with = [](QJsonObject base, const QJsonObject &fields) {
            for (auto it = fields.constBegin(); it != fields.constEnd(); ++it)
                base.insert(it.key(), it.value());
            return base;
        };

        invokeSchemaValid(registry, QStringLiteral("workspace.get"), window,
                          QStringLiteral("workspace.get"));
        invokeSchemaValid(registry, QStringLiteral("workspace.set_panel_visibility"),
                          with(window,
                               {
                                   {QStringLiteral("track_panel_visible"), true },
                                   {QStringLiteral("clip_editor_visible"), false}
        }),
                          QStringLiteral("workspace.set_panel_visibility"));
        invokeSchemaValid(registry, QStringLiteral("track_panel.get"), document,
                          QStringLiteral("track_panel.get"));
        invokeSchemaValid(registry, QStringLiteral("track_panel.set_viewport"),
                          with(document,
                               {
                                   {QStringLiteral("center_tick"),        2400.0},
                                   {QStringLiteral("center_track_index"), 1.0   },
                                   {QStringLiteral("horizontal_scale"),   1.5   },
                                   {QStringLiteral("vertical_scale"),     1.25  }
        }),
                          QStringLiteral("track_panel.set_viewport"));
        invokeSchemaValid(registry, QStringLiteral("track_panel.reveal_clips"),
                          with(revision,
                               {
                                   {QStringLiteral("track_id"), fixture.trackId.value()}
        }),
                          QStringLiteral("track_panel.reveal_clips"));
        invokeSchemaValid(registry, QStringLiteral("track_panel.set_auto_page_turn"),
                          with(document,
                               {
                                   {QStringLiteral("enabled"), false}
        }),
                          QStringLiteral("track_panel.set_auto_page_turn"));
        invokeSchemaValid(registry, QStringLiteral("track_panel.select_track"),
                          with(revision,
                               {
                                   {QStringLiteral("track_id"), fixture.trackId.value()}
        }),
                          QStringLiteral("track_panel.select_track"));
        invokeSchemaValid(
            registry, QStringLiteral("track_panel.select_clips"),
            with(revision,
                 {
                     {QStringLiteral("clip_ids"),
                      QJsonArray{fixture.scalarClipId.value(), fixture.noteClipId.value()}},
                     {QStringLiteral("primary_clip_id"), fixture.scalarClipId.value()     }
        }),
            QStringLiteral("track_panel.select_clips"));
        const auto selectedTrackPanel =
            invokeSchemaValid(registry, QStringLiteral("track_panel.get"), document,
                              QStringLiteral("selected track panel state"));
        const auto selectedClipIds =
            selectedTrackPanel
                ? selectedTrackPanel->value(QStringLiteral("selected_clip_ids")).toArray()
                : QJsonArray{};
        expect(selectedTrackPanel && selectedClipIds.size() == 2 &&
                   selectedClipIds.first() == fixture.scalarClipId.value() &&
                   selectedClipIds.last() == fixture.noteClipId.value() &&
                   selectedTrackPanel->value(QStringLiteral("primary_clip_id")) ==
                       fixture.scalarClipId.value(),
               QStringLiteral("track selection order and primary clip must remain independent"));
        const auto invalidPrimaryClip = registry.invoke(
            QStringLiteral("track_panel.select_clips"),
            with(revision,
                 {
                     {QStringLiteral("clip_ids"),        QJsonArray{fixture.scalarClipId.value()}},
                     {QStringLiteral("primary_clip_id"), fixture.noteClipId.value()              }
        }));
        expect(!invalidPrimaryClip &&
                   invalidPrimaryClip.getError().code ==
                       Automation::AutomationErrorCode::InvalidArgument &&
                   invalidPrimaryClip.getError().fieldPath == QStringLiteral("primary_clip_id"),
               QStringLiteral("track clip primary must belong to the selected set"));
        invokeSchemaValid(registry, QStringLiteral("track_panel.clear_selection"),
                          with(revision,
                               {
                                   {QStringLiteral("target"), QStringLiteral("clips")}
        }),
                          QStringLiteral("track_panel.clear_selection"));

        const auto initialClipEditor =
            invokeSchemaValid(registry, QStringLiteral("clip_editor.get"), document,
                              QStringLiteral("clip_editor.get"));
        expect(initialClipEditor &&
                   initialClipEditor->value(QStringLiteral("piano"))
                       .toObject()
                       .value(QStringLiteral("visible"))
                       .toBool() &&
                   initialClipEditor->value(QStringLiteral("parameters"))
                       .toObject()
                       .value(QStringLiteral("visible"))
                       .toBool(),
               QStringLiteral("clip editor subregion visibility must not depend on active focus"));
        invokeSchemaValid(registry, QStringLiteral("clip_editor.set_active_clip"),
                          with(revision,
                               {
                                   {QStringLiteral("clip_id"), fixture.noteClipId.value()}
        }),
                          QStringLiteral("clip_editor.set_active_clip"));
        invokeSchemaValid(registry, QStringLiteral("clip_editor.set_time_viewport"),
                          with(document,
                               {
                                   {QStringLiteral("center_tick"),      3600.0},
                                   {QStringLiteral("horizontal_scale"), 2.0   }
        }),
                          QStringLiteral("clip_editor.set_time_viewport"));
        invokeSchemaValid(registry, QStringLiteral("clip_editor.set_auto_page_turn"),
                          with(document,
                               {
                                   {QStringLiteral("enabled"), false}
        }),
                          QStringLiteral("clip_editor.set_auto_page_turn"));
        invokeSchemaValid(registry, QStringLiteral("clip_editor.show_region"),
                          with(document,
                               {
                                   {QStringLiteral("region"), QStringLiteral("parameters")}
        }),
                          QStringLiteral("clip_editor.show_region"));
        invokeSchemaValid(registry, QStringLiteral("clip_editor.piano.set_pitch_viewport"),
                          with(document,
                               {
                                   {QStringLiteral("center_key_index"), 64.0},
                                   {QStringLiteral("vertical_scale"),   1.75}
        }),
                          QStringLiteral("clip_editor.piano.set_pitch_viewport"));
        invokeSchemaValid(
            registry, QStringLiteral("clip_editor.piano.reveal_notes"),
            with(revision,
                 {
                     {QStringLiteral("note_ids"), QJsonArray{fixture.noteIds.first().value()}}
        }),
            QStringLiteral("clip_editor.piano.reveal_notes"));
        invokeSchemaValid(registry, QStringLiteral("clip_editor.piano.set_edit_mode"),
                          with(document,
                               {
                                   {QStringLiteral("mode"), QStringLiteral("draw_note")}
        }),
                          QStringLiteral("clip_editor.piano.set_edit_mode"));
        invokeSchemaValid(
            registry, QStringLiteral("clip_editor.piano.set_quantize"),
            with(document,
                 {
                     {QStringLiteral("quantize"), 16  },
                     {QStringLiteral("enabled"),  true}
        }),
            QStringLiteral("clip_editor.piano.set_quantize"));
        invokeSchemaValid(
            registry, QStringLiteral("clip_editor.piano.select_notes"),
            with(revision,
                 {
                     {QStringLiteral("note_ids"),
                      QJsonArray{fixture.noteIds.first().value(), fixture.noteIds.last().value()}},
                     {QStringLiteral("primary_note_id"), fixture.noteIds.first().value()         }
        }),
            QStringLiteral("clip_editor.piano.select_notes"));
        const auto selectedClipEditor =
            invokeSchemaValid(registry, QStringLiteral("clip_editor.get"), document,
                              QStringLiteral("selected clip editor state"));
        const auto selectedPiano =
            selectedClipEditor ? selectedClipEditor->value(QStringLiteral("piano")).toObject()
                               : QJsonObject{};
        const auto selectedNoteIds =
            selectedPiano.value(QStringLiteral("selected_note_ids")).toArray();
        expect(selectedClipEditor && selectedNoteIds.first() == fixture.noteIds.first().value() &&
                   selectedNoteIds.last() == fixture.noteIds.last().value() &&
                   selectedPiano.value(QStringLiteral("primary_note_id")) ==
                       fixture.noteIds.first().value(),
               QStringLiteral("piano selection order and primary note must remain independent"));
        const auto invalidPrimaryNote = registry.invoke(
            QStringLiteral("clip_editor.piano.select_notes"),
            with(revision,
                 {
                     {QStringLiteral("note_ids"),        QJsonArray{fixture.noteIds.first().value()}},
                     {QStringLiteral("primary_note_id"), fixture.noteIds.last().value()             }
        }));
        expect(!invalidPrimaryNote &&
                   invalidPrimaryNote.getError().code ==
                       Automation::AutomationErrorCode::InvalidArgument &&
                   invalidPrimaryNote.getError().fieldPath == QStringLiteral("primary_note_id"),
               QStringLiteral("piano primary note must belong to the selected set"));
        invokeSchemaValid(registry, QStringLiteral("clip_editor.piano.clear_selection"), revision,
                          QStringLiteral("clip_editor.piano.clear_selection"));

        invokeSchemaValid(registry, QStringLiteral("clip_editor.parameters.set_foreground"),
                          with(revision,
                               {
                                   {QStringLiteral("parameter"), QStringLiteral("breathiness")}
        }),
                          QStringLiteral("clip_editor.parameters.set_foreground"));
        invokeSchemaValid(registry, QStringLiteral("clip_editor.parameters.set_background"),
                          with(revision,
                               {
                                   {QStringLiteral("parameter"), QStringLiteral("tension")}
        }),
                          QStringLiteral("clip_editor.parameters.set_background"));
        const auto invalidForegroundParameter = registry.invoke(
            QStringLiteral("clip_editor.parameters.set_foreground"),
            with(revision, {
                               {QStringLiteral("parameter"), QStringLiteral("pitch")}
        }));
        const auto invalidBackgroundParameter = registry.invoke(
            QStringLiteral("clip_editor.parameters.set_background"),
            with(revision, {
                               {QStringLiteral("parameter"), QStringLiteral("speaker_mix")}
        }));
        expect(!invalidForegroundParameter && !invalidBackgroundParameter &&
                   invalidForegroundParameter.getError().code ==
                       Automation::AutomationErrorCode::InvalidArgument &&
                   invalidBackgroundParameter.getError().code ==
                       Automation::AutomationErrorCode::InvalidArgument,
               QStringLiteral(
                   "parameter GUI schemas must reject unsupported foreground/background values"));
        invokeSchemaValid(registry, QStringLiteral("clip_editor.parameters.swap"), revision,
                          QStringLiteral("clip_editor.parameters.swap"));
        invokeSchemaValid(registry, QStringLiteral("clip_editor.parameters.set_tool"),
                          with(revision,
                               {
                                   {QStringLiteral("tool"), QStringLiteral("anchor")}
        }),
                          QStringLiteral("clip_editor.parameters.set_tool"));
        invokeSchemaValid(registry, QStringLiteral("clip_editor.parameters.set_value_viewport"),
                          with(revision,
                               {
                                   {QStringLiteral("center_ratio"),   0.75},
                                   {QStringLiteral("vertical_scale"), 2.0 }
        }),
                          QStringLiteral("clip_editor.parameters.set_value_viewport"));

        auto staleRevision = revision;
        staleRevision.insert(QStringLiteral("expected_revision"),
                             static_cast<qint64>(before.revision + 1));
        staleRevision.insert(QStringLiteral("track_id"), fixture.trackId.value());
        const auto staleSelection =
            registry.invoke(QStringLiteral("track_panel.select_track"), staleRevision);
        expect(!staleSelection && staleSelection.getError().code ==
                                      Automation::AutomationErrorCode::RevisionConflict,
               QStringLiteral("GUI object selection must reject a stale expected revision"));
        const auto missingRevision = registry.invoke(
            QStringLiteral("track_panel.select_track"),
            with(document, {
                               {QStringLiteral("track_id"), fixture.trackId.value()}
        }));
        expect(!missingRevision && missingRevision.getError().code ==
                                       Automation::AutomationErrorCode::InvalidArgument,
               QStringLiteral("GUI object selection must require expected_revision"));
        expect(
            runtime.documentVersion() == before,
            QStringLiteral("all L3 GUI bindings must leave document revision and History intact"));
    }

    void verifyAdvancedApplicationBindings(Automation::PublicAutomationRegistry &registry,
                                           Automation::CoreRuntime &runtime,
                                           const QString &allowedPackagePath,
                                           const QString &builtinLyricRuleId,
                                           const QString &customLyricRuleId,
                                           PackageRefreshTestControl &refreshControl) {
        const auto before = runtime.documentVersion();
        const auto query = invokeSchemaValid(
            registry, QStringLiteral("settings.query"),
            QJsonObject{
                {QStringLiteral("domains"),
                 QJsonArray{QStringLiteral("audio_device"), QStringLiteral("render")}}
        },
            QStringLiteral("filtered settings query"));
        const auto queriedDomains =
            query ? query->value(QStringLiteral("domains")).toObject() : QJsonObject{};
        const auto audioBefore = queriedDomains.value(QStringLiteral("audio_device"))
                                     .toObject()
                                     .value(QStringLiteral("configured"));
        const auto renderBefore = queriedDomains.value(QStringLiteral("render"))
                                      .toObject()
                                      .value(QStringLiteral("configured"));
        expect(queriedDomains.size() == 2 && !audioBefore.isUndefined() &&
                   !renderBefore.isUndefined(),
               QStringLiteral("settings.query must return only requested domains"));
        invokeSchemaValid(
            registry, QStringLiteral("automation.get_options"),
            QJsonObject{
                {QStringLiteral("operation_id"),      QStringLiteral("settings.audio_device.update")},
                {QStringLiteral("field_path"),        QStringLiteral("/gain")                       },
                {QStringLiteral("partial_arguments"), QJsonObject{}                                 }
        },
            QStringLiteral("settings range options"));

        struct SettingsUpdate {
            const char *operationId;
            QJsonObject arguments;
        };

        const QList<SettingsUpdate> updates{
            {"settings.ui_language.update",
             {{QStringLiteral("ui_language"), QStringLiteral("zh_CN")}}                                        },
            {"settings.singing.update",
             {{QStringLiteral("default_language"), QStringLiteral("zh")}}                                      },
            {"settings.theme.update",                    {{QStringLiteral("theme_id"), QStringLiteral("dark")}}},
            {"settings.audio_device.update",
             {{QStringLiteral("gain"), 0.5}, {QStringLiteral("validate_only"), true}}                          },
            {"settings.playback_behavior.update",        {{QStringLiteral("behavior"), 1}}                     },
            {"settings.compute_device.update",
             {{QStringLiteral("execution_provider"), QStringLiteral("CPU")},
              {QStringLiteral("validate_only"), true}}                                                         },
            {"settings.render.update",                   {{QStringLiteral("depth"), 0.5}}                      },
            {"settings.singer_session_retention.update", {{QStringLiteral("capacity"), 4}}                     },
            {"settings.package_search_paths.update",
             {{QStringLiteral("paths"), QJsonArray{allowedPackagePath}},
              {QStringLiteral("validate_only"), true}}                                                         },
        };
        for (const auto &update : updates) {
            invokeSchemaValid(registry, QString::fromLatin1(update.operationId), update.arguments,
                              QString::fromLatin1(update.operationId));
        }
        invokeSchemaValid(registry, QStringLiteral("settings.audio_device.update"),
                          QJsonObject{
                              {QStringLiteral("device_name"),   QString()},
                              {QStringLiteral("validate_only"), true     },
        },
                          QStringLiteral("default audio device update"));
        const auto sparseAudio =
            invokeSchemaValid(registry, QStringLiteral("settings.audio_device.update"),
                              QJsonObject{
                                  {QStringLiteral("gain"), 0.5}
        },
                              QStringLiteral("sparse audio update"));
        expect(sparseAudio && sparseAudio->value(QStringLiteral("configured"))
                                      .toObject()
                                      .value(QStringLiteral("device_name")) ==
                                  audioBefore.toObject().value(QStringLiteral("device_name")),
               QStringLiteral("sparse settings updates must preserve omitted fields"));
        const auto sparseRender =
            invokeSchemaValid(registry, QStringLiteral("settings.render.update"),
                              QJsonObject{
                                  {QStringLiteral("depth"), 0.5}
        },
                              QStringLiteral("sparse render update"));
        expect(sparseRender && sparseRender->value(QStringLiteral("configured"))
                                       .toObject()
                                       .value(QStringLiteral("sampling_steps")) ==
                                   renderBefore.toObject().value(QStringLiteral("sampling_steps")),
               QStringLiteral("sparse render updates must preserve omitted fields"));
        const auto emptyUpdate =
            registry.invoke(QStringLiteral("settings.theme.update"), QJsonObject{});
        expect(!emptyUpdate &&
                   emptyUpdate.getError().code == Automation::AutomationErrorCode::InvalidArgument,
               QStringLiteral("settings updates must reject an empty patch"));
        const auto unknownDomain = registry.invoke(
            QStringLiteral("settings.query"),
            QJsonObject{
                {QStringLiteral("domains"), QJsonArray{QStringLiteral("automation")}}
        });
        expect(!unknownDomain && unknownDomain.getError().code ==
                                     Automation::AutomationErrorCode::InvalidArgument,
               QStringLiteral("settings.query must reject a non-public domain"));
        const auto unknownLanguage =
            registry.invoke(QStringLiteral("settings.ui_language.update"),
                            QJsonObject{
                                {QStringLiteral("ui_language"), QStringLiteral("not-installed")}
        });
        expect(!unknownLanguage && unknownLanguage.getError().code ==
                                       Automation::AutomationErrorCode::InvalidArgument,
               QStringLiteral("settings updates must reject values absent from settings.query"));

        const auto packages = invokeSchemaValid(registry, QStringLiteral("packages.list"), {},
                                                QStringLiteral("packages.list"));
        const auto packageItems =
            packages ? packages->value(QStringLiteral("packages")).toArray() : QJsonArray{};
        bool packagePathsRedacted = packageItems.size() == 2;
        for (const auto &item : packageItems) {
            packagePathsRedacted &=
                item.toObject().value(QStringLiteral("canonical_path")).isNull();
        }
        expect(packagePathsRedacted,
               QStringLiteral("packages.list must redact paths outside the allowed roots"));
        const auto package = invokeSchemaValid(
            registry, QStringLiteral("packages.describe"),
            QJsonObject{
                {QStringLiteral("package_id"), QStringLiteral("registry-package")}
        },
            QStringLiteral("packages.describe"));
        expect(package && package->value(QStringLiteral("package"))
                              .toObject()
                              .value(QStringLiteral("canonical_path"))
                              .isNull(),
               QStringLiteral("packages.describe must redact paths outside the allowed roots"));
        const auto versionedPackage = invokeSchemaValid(
            registry, QStringLiteral("packages.describe"),
            QJsonObject{
                {QStringLiteral("package_id"), QStringLiteral("registry-package")},
                {QStringLiteral("version"),    QStringLiteral("1.0")             },
        },
            QStringLiteral("versioned packages.describe"));
        expect(versionedPackage &&
                   versionedPackage->value(QStringLiteral("package"))
                           .toObject()
                           .value(QStringLiteral("version")) == QStringLiteral("1.0"),
               QStringLiteral("packages.describe must accept an explicit installed version"));
        invokeSchemaValid(registry, QStringLiteral("automation.get_options"),
                          QJsonObject{
                              {QStringLiteral("operation_id"),      QStringLiteral("packages.describe")},
                              {QStringLiteral("field_path"),        QStringLiteral("/package_id")      },
                              {QStringLiteral("partial_arguments"), QJsonObject{}                      }
        },
                          QStringLiteral("package ID options"));
        invokeSchemaValid(
            registry, QStringLiteral("automation.get_options"),
            QJsonObject{
                {QStringLiteral("operation_id"),      QStringLiteral("packages.describe")       },
                {QStringLiteral("field_path"),        QStringLiteral("/version")                },
                {QStringLiteral("partial_arguments"),
                 QJsonObject{{QStringLiteral("package_id"), QStringLiteral("registry-package")}}},
        },
            QStringLiteral("package version options"));
        const auto unknownPackage =
            registry.invoke(QStringLiteral("packages.describe"),
                            QJsonObject{
                                {QStringLiteral("package_id"), QStringLiteral("not-installed")}
        });
        expect(!unknownPackage &&
                   unknownPackage.getError().code == Automation::AutomationErrorCode::NotFound,
               QStringLiteral("packages.describe must report an unknown installed package"));
        const auto validatedRefresh = registry.invoke(QStringLiteral("packages.refresh"),
                                                      QJsonObject{
                                                          {QStringLiteral("validate_only"), true}
        });
        expect(!validatedRefresh && validatedRefresh.getError().code ==
                                        Automation::AutomationErrorCode::InvalidArgument,
               QStringLiteral("package refresh must reject unsupported validate-only input"));
        const auto refresh = invokeSchemaValid(registry, QStringLiteral("packages.refresh"), {},
                                               QStringLiteral("package refresh"));
        const auto taskId =
            refresh ? refresh->value(QStringLiteral("task_id")).toString() : QString();
        const QJsonObject applicationTask{
            {QStringLiteral("scope"),   QStringLiteral("application")},
            {QStringLiteral("task_id"), taskId                       },
        };
        const auto task = invokeSchemaValid(registry, QStringLiteral("tasks.get"), applicationTask,
                                            QStringLiteral("application task get"));
        const auto refreshFailures = task ? task->value(QStringLiteral("application_result"))
                                                .toObject()
                                                .value(QStringLiteral("failures"))
                                                .toArray()
                                          : QJsonArray{};
        expect(
            task && task->value(QStringLiteral("scope")) == QStringLiteral("application") &&
                task->value(QStringLiteral("document")).isNull() &&
                task->value(QStringLiteral("state")) == QStringLiteral("succeeded") &&
                refreshFailures.size() == 1 &&
                refreshFailures.first().toObject().value(QStringLiteral("path")).isNull() &&
                !refreshFailures.first()
                     .toObject()
                     .value(QStringLiteral("reason"))
                     .toString()
                     .contains(refreshControl.privatePath),
            QStringLiteral(
                "package refresh must redact structured and free-text paths before task output"));
        const auto applicationTasks =
            invokeSchemaValid(registry, QStringLiteral("tasks.list"),
                              QJsonObject{
                                  {QStringLiteral("scope"), QStringLiteral("application")}
        },
                              QStringLiteral("application tasks list"));
        expect(
            applicationTasks && applicationTasks->value(QStringLiteral("document")).isNull() &&
                applicationTasks->value(QStringLiteral("tasks")).toArray().size() == 1,
            QStringLiteral("tasks.list must expose application tasks only in application scope"));
        const auto documentTasks =
            invokeSchemaValid(registry, QStringLiteral("tasks.list"),
                              QJsonObject{
                                  {QStringLiteral("scope"),       QStringLiteral("document")  },
                                  {QStringLiteral("document_id"), before.documentId.toString()}
        },
                              QStringLiteral("document tasks list"));
        expect(documentTasks && documentTasks->value(QStringLiteral("tasks")).toArray().isEmpty(),
               QStringLiteral("document task scope must not leak application tasks"));

        refreshControl.deferNext = true;
        const auto cancelableRefresh =
            invokeSchemaValid(registry, QStringLiteral("packages.refresh"), {},
                              QStringLiteral("cancelable package refresh"));
        const auto cancelableTaskId =
            cancelableRefresh ? cancelableRefresh->value(QStringLiteral("task_id")).toString()
                              : QString();
        const QJsonObject cancelableTask{
            {QStringLiteral("scope"),   QStringLiteral("application")},
            {QStringLiteral("task_id"), cancelableTaskId             },
        };
        const auto cancelRequested =
            invokeSchemaValid(registry, QStringLiteral("tasks.cancel"), cancelableTask,
                              QStringLiteral("cancel package refresh"));
        const bool commitRejected =
            refreshControl.pendingCommitGate && !refreshControl.pendingCommitGate();
        refreshControl.pendingCommitGate = {};
        refreshControl.pendingCompletion = {};
        const auto canceledTask =
            invokeSchemaValid(registry, QStringLiteral("tasks.get"), cancelableTask,
                              QStringLiteral("canceled package refresh task"));
        expect(cancelRequested &&
                   cancelRequested->value(QStringLiteral("state")) ==
                       QStringLiteral("cancel_requested") &&
                   commitRejected && canceledTask &&
                   canceledTask->value(QStringLiteral("state")) == QStringLiteral("canceled") &&
                   !canceledTask->contains(QStringLiteral("application_result")),
               QStringLiteral(
                   "packages.refresh cancellation must close its commit gate before publication"));

        refreshControl.failNext = true;
        const auto failedRefresh = invokeSchemaValid(registry, QStringLiteral("packages.refresh"),
                                                     {}, QStringLiteral("failed package refresh"));
        const QJsonObject failedTaskInput{
            {QStringLiteral("scope"),   QStringLiteral("application")},
            {QStringLiteral("task_id"),
             failedRefresh ? failedRefresh->value(QStringLiteral("task_id")).toString()
                           : QString()                               },
        };
        const auto failedTask =
            invokeSchemaValid(registry, QStringLiteral("tasks.get"), failedTaskInput,
                              QStringLiteral("failed package refresh task"));
        const auto publicError =
            failedTask ? failedTask->value(QStringLiteral("error")).toObject() : QJsonObject{};
        expect(
            failedTask && failedTask->value(QStringLiteral("state")) == QStringLiteral("failed") &&
                !publicError.value(QStringLiteral("message"))
                     .toString()
                     .contains(refreshControl.privatePath),
            QStringLiteral("package refresh errors must not expose unauthorized absolute paths"));

        invokeSchemaValid(registry, QStringLiteral("lyric_rules.list"),
                          QJsonObject{
                              {QStringLiteral("include_disabled"), true}
        },
                          QStringLiteral("lyric rules list"));
        invokeSchemaValid(
            registry, QStringLiteral("automation.get_options"),
            QJsonObject{
                {QStringLiteral("operation_id"),      QStringLiteral("lyric_rules.update")},
                {QStringLiteral("field_path"),        QStringLiteral("/rule_id")          },
                {QStringLiteral("partial_arguments"), QJsonObject{}                       }
        },
            QStringLiteral("lyric rule ID options"));
        invokeSchemaValid(registry, QStringLiteral("lyric_rules.create"),
                          QJsonObject{
                              {QStringLiteral("kind"),          QStringLiteral("splitter")        },
                              {QStringLiteral("name"),          QStringLiteral("Registry Draft")  },
                              {QStringLiteral("regexes"),       QJsonArray{QStringLiteral("[,.]")}},
                              {QStringLiteral("validate_only"), true                              }
        },
                          QStringLiteral("lyric rule create"));
        invokeSchemaValid(registry, QStringLiteral("lyric_rules.create"),
                          QJsonObject{
                              {QStringLiteral("kind"),          QStringLiteral("tagger")         },
                              {QStringLiteral("name"),          QStringLiteral("Registry Tagger")},
                              {QStringLiteral("language"),      QStringLiteral("en")             },
                              {QStringLiteral("entries"),
                               QJsonArray{QJsonObject{
                                   {QStringLiteral("type"), QStringLiteral("array")},
                                   {QStringLiteral("value"), QJsonArray{QStringLiteral("star")}},
                                   {QStringLiteral("tag"), QStringLiteral("word")},
                                   {QStringLiteral("discard"), false},
                               }}                                                                },
                              {QStringLiteral("validate_only"), true                             },
        },
                          QStringLiteral("tagger lyric rule create"));
        invokeSchemaValid(registry, QStringLiteral("lyric_rules.update"),
                          QJsonObject{
                              {QStringLiteral("rule_id"),       customLyricRuleId                 },
                              {QStringLiteral("name"),          QStringLiteral("Registry Renamed")},
                              {QStringLiteral("validate_only"), true                              }
        },
                          QStringLiteral("lyric rule update"));
        invokeSchemaValid(registry, QStringLiteral("lyric_rules.set_enabled"),
                          QJsonObject{
                              {QStringLiteral("rule_id"), builtinLyricRuleId},
                              {QStringLiteral("enabled"), false             },
        },
                          QStringLiteral("lyric rule set enabled"));
        invokeSchemaValid(registry, QStringLiteral("lyric_rules.move"),
                          QJsonObject{
                              {QStringLiteral("rule_id"),  customLyricRuleId},
                              {QStringLiteral("position"), 0                },
        },
                          QStringLiteral("lyric rule move"));
        invokeSchemaValid(registry, QStringLiteral("lyric_rules.delete"),
                          QJsonObject{
                              {QStringLiteral("rule_id"), customLyricRuleId},
        },
                          QStringLiteral("lyric rule delete"));
        invokeSchemaValid(registry, QStringLiteral("lyric_rules.test"),
                          QJsonObject{
                              {QStringLiteral("text"), QStringLiteral("hello world")}
        },
                          QStringLiteral("lyric rules test"));
        for (const auto operationId :
             {QStringLiteral("lyric_rules.update"), QStringLiteral("lyric_rules.delete")}) {
            QJsonObject arguments{
                {QStringLiteral("rule_id"), builtinLyricRuleId}
            };
            if (operationId.endsWith(QStringLiteral("update")))
                arguments.insert(QStringLiteral("name"), QStringLiteral("Forbidden"));
            const auto result = registry.invoke(operationId, arguments);
            expect(!result &&
                       result.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
                       result.getError().fieldPath.endsWith(QStringLiteral("rule_id")),
                   operationId + QStringLiteral(" must reject writes to built-in rule content"));
        }
        expect(runtime.documentVersion() == before,
               QStringLiteral("all L3 application bindings must leave document revision intact"));
    }

    void verifyPackageRefreshLifetime(Automation::CoreRuntime &runtime,
                                      Automation::AutomationAccessPolicy &access,
                                      Automation::AutomationFileGuard &fileGuard,
                                      Automation::AdmissionController &admission,
                                      PackageRefreshTestControl &refreshControl) {
        refreshControl.deferNext = true;
        QString taskId;
        {
            Automation::PublicAutomationRegistry transientRegistry(runtime, access, fileGuard,
                                                                   admission);
            const auto started = transientRegistry.invoke(QStringLiteral("packages.refresh"), {});
            expect(bool(started), QStringLiteral("transient registry refresh must start"));
            if (started)
                taskId = started.get().value(QStringLiteral("task_id")).toString();
        }

        const bool rejectedAfterDestruction =
            refreshControl.pendingCommitGate && !refreshControl.pendingCommitGate();
        if (refreshControl.pendingCompletion) {
            refreshControl.pendingCompletion(Automation::PackageRefreshResultDto{
                .packages = 1,
                .failures = {{refreshControl.privatePath, QStringLiteral("late result from %1")
                                                              .arg(refreshControl.privatePath)}},
            });
        }
        refreshControl.pendingCommitGate = {};
        refreshControl.pendingCompletion = {};

        const auto snapshot =
            runtime.automationTasks().getApplication(Automation::TaskId::fromString(taskId));
        expect(rejectedAfterDestruction && snapshot &&
                   snapshot.get().state == Automation::AutomationTaskState::Running &&
                   !snapshot.get().applicationResult,
               QStringLiteral(
                   "destroyed registries must reject commit and discard late package callbacks"));
        runtime.automationTasks().cancel(Automation::TaskId::fromString(taskId));
    }

    std::optional<PublicEditingFixture>
        verifyPublicEditingBehavior(Automation::PublicAutomationRegistry &registry,
                                    Automation::CoreRuntime &runtime) {
        const auto projectBeforeTracks =
            runtime.project().getProject(runtime.documentVersion().documentId);
        if (!projectBeforeTracks) {
            expect(false, QStringLiteral("public behavior project fixture must be queryable"));
            return std::nullopt;
        }

        auto nestedTrackInput = commandArguments(runtime.documentVersion());
        nestedTrackInput.insert(QStringLiteral("index"), projectBeforeTracks.get().tracks.size());
        nestedTrackInput.insert(QStringLiteral("tracks"),
                                QJsonArray{
                                    QJsonObject{{QStringLiteral("clips"), QJsonArray{}},
                                                {QStringLiteral("voice"), QJsonObject{}}}
        });
        const auto beforeNestedTrack = runtime.documentVersion();
        const auto nestedTrack =
            registry.invoke(QStringLiteral("tracks.insert"), nestedTrackInput,
                            {.clientId = QStringLiteral("shallow-track-rejection")});
        expect(!nestedTrack &&
                   nestedTrack.getError().code ==
                       Automation::AutomationErrorCode::InvalidArgument &&
                   runtime.documentVersion() == beforeNestedTrack,
               QStringLiteral("tracks.insert must reject nested clips and voice without mutation"));

        auto insertTracksInput = commandArguments(runtime.documentVersion());
        insertTracksInput.insert(QStringLiteral("index"), projectBeforeTracks.get().tracks.size());
        insertTracksInput.insert(
            QStringLiteral("tracks"),
            QJsonArray{
                QJsonObject{{QStringLiteral("client_ref"), QStringLiteral("batch-track-empty")}},
                QJsonObject{{QStringLiteral("client_ref"), QStringLiteral("batch-track-a")},
                            {QStringLiteral("name"), QStringLiteral("Batch Track A")},
                            {QStringLiteral("color_index"), 0}},
                QJsonObject{{QStringLiteral("client_ref"), QStringLiteral("batch-track-b")},
                            {QStringLiteral("name"), QStringLiteral("Batch Track B")}}
        });
        const auto trackInsertBase = runtime.documentVersion();
        const auto expectedDefaultColor =
            projectBeforeTracks.get().tracks.isEmpty()
                ? 0
                : (projectBeforeTracks.get().tracks.constLast().data.colorIndex + 1) %
                      AutomationWire::TrackPaletteColorCount;
        const auto insertedTracks =
            registry.invoke(QStringLiteral("tracks.insert"), insertTracksInput,
                            {.clientId = QStringLiteral("shallow-track-create")});
        const auto trackIds = insertedTracks
                                  ? createdObjectIds(insertedTracks.get(), QStringLiteral("track"))
                                  : QList<int>{};
        expect(insertedTracks && trackIds.size() == 3 &&
                   runtime.documentVersion().revision == trackInsertBase.revision + 1,
               QStringLiteral("tracks.insert must create three shallow tracks in one revision"));
        if (trackIds.size() != 3)
            return std::nullopt;

        PublicEditingFixture fixture{
            .trackId = Automation::TrackId(trackIds.at(0)),
            .batchTrackA = Automation::TrackId(trackIds.at(1)),
            .batchTrackB = Automation::TrackId(trackIds.at(2)),
        };
        const auto emptyTrack = trackSnapshot(runtime, fixture.trackId);
        const auto explicitZeroTrack = trackSnapshot(runtime, fixture.batchTrackA);
        const auto defaultAfterZeroTrack = trackSnapshot(runtime, fixture.batchTrackB);
        expect(emptyTrack.has_value(),
               QStringLiteral("minimal TrackCreate must remain queryable after insertion"));
        if (emptyTrack) {
            expect(emptyTrack->clips.isEmpty() && emptyTrack->data.clips.isEmpty(),
                   QStringLiteral("minimal TrackCreate must create an empty track"));
            expect(emptyTrack->data.name ==
                       QCoreApplication::translate("TrackController", "New Track"),
                   QStringLiteral("minimal TrackCreate must apply the GUI track name default"));
            expect(emptyTrack->data.colorIndex == expectedDefaultColor,
                   QStringLiteral("minimal TrackCreate must apply the GUI color default"));
            expect(emptyTrack->data.defaultLanguage == QStringLiteral("unknown"),
                   QStringLiteral("minimal TrackCreate must apply the GUI language default"));
            expect(emptyTrack->data.singerInfo.isEmpty() &&
                       emptyTrack->data.speakerInfo.isEmpty() &&
                       emptyTrack->data.speakerMixData.sources.isEmpty(),
                   QStringLiteral("minimal TrackCreate must not synthesize voice state"));
            expect(insertedTracks &&
                       resolvedValue(insertedTracks.get(), QStringLiteral("/tracks/0/name")) ==
                           emptyTrack->data.name,
                   QStringLiteral("minimal TrackCreate must report its resolved name"));
            expect(insertedTracks && resolvedValue(insertedTracks.get(),
                                                   QStringLiteral("/tracks/0/color_index")) ==
                                         expectedDefaultColor,
                   QStringLiteral("minimal TrackCreate must report its resolved color"));
            expect(
                insertedTracks &&
                    resolvedValue(insertedTracks.get(), QStringLiteral("/tracks/1/name"))
                        .isUndefined(),
                QStringLiteral("TrackCreate must not report a caller-supplied name as resolved"));
        }
        expect(explicitZeroTrack && defaultAfterZeroTrack &&
                   explicitZeroTrack->data.colorIndex == 0 &&
                   defaultAfterZeroTrack->data.colorIndex == 1,
               QStringLiteral("explicit palette index zero must remain zero and the next omitted "
                              "color must resolve to one"));
        expect(insertedTracks &&
                   resolvedValue(insertedTracks.get(), QStringLiteral("/tracks/1/color_index"))
                       .isUndefined() &&
                   resolvedValue(insertedTracks.get(), QStringLiteral("/tracks/2/color_index"))
                           .toInt() == 1,
               QStringLiteral("tracks.insert must report only server-resolved batch colors"));

        const auto undoTrackInsert = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("batch-track-insert-undo")});
        const auto tracksAbsentAfterUndo = !trackSnapshot(runtime, fixture.trackId) &&
                                           !trackSnapshot(runtime, fixture.batchTrackA) &&
                                           !trackSnapshot(runtime, fixture.batchTrackB);
        const auto redoTrackInsert = registry.invoke(
            QStringLiteral("history.redo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("batch-track-insert-redo")});
        const auto explicitZeroAfterRedo = trackSnapshot(runtime, fixture.batchTrackA);
        const auto defaultAfterZeroAfterRedo = trackSnapshot(runtime, fixture.batchTrackB);
        expect(
            undoTrackInsert && tracksAbsentAfterUndo && redoTrackInsert && explicitZeroAfterRedo &&
                defaultAfterZeroAfterRedo && explicitZeroAfterRedo->data.colorIndex == 0 &&
                defaultAfterZeroAfterRedo->data.colorIndex == 1 &&
                runtime.documentVersion().revision == trackInsertBase.revision + 3,
            QStringLiteral("batch track insertion colors must survive one History undo and redo"));

        const auto orderBeforeMove = trackOrder(runtime);
        auto moveTracksInput = commandArguments(runtime.documentVersion());
        moveTracksInput.insert(
            QStringLiteral("track_ids"),
            QJsonArray{fixture.batchTrackA.value(), fixture.batchTrackB.value()});
        moveTracksInput.insert(QStringLiteral("target_index"), 0);
        const auto trackMoveBase = runtime.documentVersion();
        const auto movedTracks = registry.invoke(QStringLiteral("tracks.move"), moveTracksInput,
                                                 {.clientId = QStringLiteral("batch-track-move")});
        const auto movedOrder = trackOrder(runtime);
        const auto undoTrackMove = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("batch-track-move-undo")});
        expect(
            movedTracks && runtime.documentVersion().revision == trackMoveBase.revision + 2 &&
                movedOrder.size() >= 2 && movedOrder.at(0) == fixture.batchTrackA.value() &&
                movedOrder.at(1) == fixture.batchTrackB.value() && undoTrackMove &&
                trackOrder(runtime) == orderBeforeMove,
            QStringLiteral("tracks.move batch must preserve order and undo as one History entry"));

        auto removeTracksInput = commandArguments(runtime.documentVersion());
        removeTracksInput.insert(
            QStringLiteral("track_ids"),
            QJsonArray{fixture.batchTrackA.value(), fixture.batchTrackB.value()});
        const auto trackRemoveBase = runtime.documentVersion();
        const auto removedTracks =
            registry.invoke(QStringLiteral("tracks.remove"), removeTracksInput,
                            {.clientId = QStringLiteral("batch-track-remove")});
        const auto tracksAbsent = !trackSnapshot(runtime, fixture.batchTrackA) &&
                                  !trackSnapshot(runtime, fixture.batchTrackB);
        const auto undoTrackRemove = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("batch-track-remove-undo")});
        expect(
            removedTracks && runtime.documentVersion().revision == trackRemoveBase.revision + 2 &&
                tracksAbsent && undoTrackRemove && trackSnapshot(runtime, fixture.batchTrackA) &&
                trackSnapshot(runtime, fixture.batchTrackB),
            QStringLiteral("tracks.remove batch must delete and restore both tracks in one undo"));

        const QJsonObject trackTarget{
            {QStringLiteral("track_id"), fixture.trackId.value()}
        };
        const auto trackPublic = [&] {
            return publicTrackSnapshot(registry, runtime, fixture.trackId);
        };
        verifyScalarEdit(registry, runtime, QStringLiteral("tracks.rename"), trackTarget,
                         QStringLiteral("name"), QStringLiteral("Renamed Track"),
                         QStringLiteral("name"), trackPublic, QStringLiteral("track rename"));
        verifyScalarEdit(registry, runtime, QStringLiteral("tracks.set_color"), trackTarget,
                         QStringLiteral("color_index"), 7, QStringLiteral("color_index"),
                         trackPublic, QStringLiteral("track color"));
        verifyScalarEdit(registry, runtime, QStringLiteral("tracks.set_gain"), trackTarget,
                         QStringLiteral("gain"), 0.35, QStringLiteral("gain"), trackPublic,
                         QStringLiteral("track gain"));
        verifyScalarEdit(registry, runtime, QStringLiteral("tracks.set_pan"), trackTarget,
                         QStringLiteral("pan"), -0.25, QStringLiteral("pan"), trackPublic,
                         QStringLiteral("track pan"));
        verifyScalarEdit(registry, runtime, QStringLiteral("tracks.set_mute"), trackTarget,
                         QStringLiteral("mute"), true, QStringLiteral("mute"), trackPublic,
                         QStringLiteral("track mute"));
        verifyScalarEdit(registry, runtime, QStringLiteral("tracks.set_solo"), trackTarget,
                         QStringLiteral("solo"), true, QStringLiteral("solo"), trackPublic,
                         QStringLiteral("track solo"));
        auto nestedClipInput = commandArguments(runtime.documentVersion());
        nestedClipInput.insert(
            QStringLiteral("clips"),
            QJsonArray{
                QJsonObject{{QStringLiteral("track_id"), fixture.trackId.value()},
                            {QStringLiteral("start"), 0},
                            {QStringLiteral("notes"), QJsonArray{}},
                            {QStringLiteral("parameters"), QJsonArray{}},
                            {QStringLiteral("speaker_mix"), QJsonObject{}}}
        });
        const auto beforeNestedClip = runtime.documentVersion();
        const auto nestedClip =
            registry.invoke(QStringLiteral("clips.insert"), nestedClipInput,
                            {.clientId = QStringLiteral("shallow-clip-rejection")});
        expect(!nestedClip &&
                   nestedClip.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
                   runtime.documentVersion() == beforeNestedClip,
               QStringLiteral("clips.insert must reject nested editing trees without mutation"));

        auto insertClipsInput = commandArguments(runtime.documentVersion());
        insertClipsInput.insert(
            QStringLiteral("clips"),
            QJsonArray{
                QJsonObject{{QStringLiteral("client_ref"), QStringLiteral("scalar-clip")},
                            {QStringLiteral("track_id"), fixture.trackId.value()},
                            {QStringLiteral("start"), 0}},
                QJsonObject{{QStringLiteral("client_ref"), QStringLiteral("note-clip")},
                            {QStringLiteral("track_id"), fixture.trackId.value()},
                            {QStringLiteral("start"), 2400},
                            {QStringLiteral("length"), 1920},
                            {QStringLiteral("name"), QStringLiteral("Notes")}},
                QJsonObject{{QStringLiteral("client_ref"), QStringLiteral("mix-clip")},
                            {QStringLiteral("track_id"), fixture.trackId.value()},
                            {QStringLiteral("start"), 4800},
                            {QStringLiteral("length"), 1920},
                            {QStringLiteral("name"), QStringLiteral("Mix")}},
        });
        const auto clipInsertBase = runtime.documentVersion();
        const auto insertedClips =
            registry.invoke(QStringLiteral("clips.insert"), insertClipsInput,
                            {.clientId = QStringLiteral("shallow-clip-create")});
        const auto clipIds = insertedClips
                                 ? createdObjectIds(insertedClips.get(), QStringLiteral("clip"))
                                 : QList<int>{};
        expect(
            insertedClips && clipIds.size() == 3 &&
                runtime.documentVersion().revision == clipInsertBase.revision + 1,
            QStringLiteral("clips.insert must create three empty singing clips in one revision"));
        if (clipIds.size() != 3)
            return std::nullopt;
        fixture.scalarClipId = Automation::ClipId(clipIds.at(0));
        fixture.noteClipId = Automation::ClipId(clipIds.at(1));
        fixture.mixClipId = Automation::ClipId(clipIds.at(2));
        const auto emptyClip = clipSnapshot(runtime, fixture.scalarClipId);
        expect(
            emptyClip &&
                emptyClip->data.properties.name ==
                    QCoreApplication::translate("TrackController", "New Singing Clip") &&
                emptyClip->data.properties.length == 7680 && emptyTrack &&
                emptyClip->data.defaultLanguage == emptyTrack->data.defaultLanguage &&
                insertedClips &&
                resolvedValue(insertedClips.get(), QStringLiteral("/clips/0/name")) ==
                    emptyClip->data.properties.name &&
                resolvedValue(insertedClips.get(), QStringLiteral("/clips/0/length")) == 7680 &&
                resolvedValue(insertedClips.get(), QStringLiteral("/clips/1/name")).isUndefined() &&
                emptyClip->data.notes.isEmpty() && emptyClip->data.params.isEmpty() &&
                emptyClip->data.usesTrackVoiceContext && emptyClip->data.ownSingerInfo.isEmpty() &&
                emptyClip->data.ownSpeakerMixData.sources.isEmpty(),
            QStringLiteral("minimal SingingClipCreate must apply and report GUI defaults on an "
                           "inherited clip"));

        const QJsonObject clipTarget{
            {QStringLiteral("clip_id"), fixture.scalarClipId.value()}
        };
        const auto clipPublic = [&] {
            return publicClipSnapshot(registry, runtime, fixture.scalarClipId);
        };
        verifyScalarEdit(registry, runtime, QStringLiteral("clips.rename"), clipTarget,
                         QStringLiteral("name"), QStringLiteral("Renamed Clip"),
                         QStringLiteral("name"), clipPublic, QStringLiteral("clip rename"));
        verifyScalarEdit(registry, runtime, QStringLiteral("clips.set_gain"), clipTarget,
                         QStringLiteral("gain"), 0.6, QStringLiteral("gain"), clipPublic,
                         QStringLiteral("clip gain"));
        verifyScalarEdit(registry, runtime, QStringLiteral("clips.set_mute"), clipTarget,
                         QStringLiteral("mute"), true, QStringLiteral("mute"), clipPublic,
                         QStringLiteral("clip mute"));
        const QJsonObject noTarget;
        const auto masterPublic = [&] { return publicMasterSnapshot(registry, runtime); };
        verifyScalarEdit(registry, runtime, QStringLiteral("master.set_gain"), noTarget,
                         QStringLiteral("gain"), 0.5, QStringLiteral("gain"), masterPublic,
                         QStringLiteral("master gain"));
        verifyScalarEdit(registry, runtime, QStringLiteral("master.set_pan"), noTarget,
                         QStringLiteral("pan"), -0.4, QStringLiteral("pan"), masterPublic,
                         QStringLiteral("master pan"));
        verifyScalarEdit(registry, runtime, QStringLiteral("master.set_mute"), noTarget,
                         QStringLiteral("mute"), true, QStringLiteral("mute"), masterPublic,
                         QStringLiteral("master mute"));
        verifyScalarEdit(registry, runtime, QStringLiteral("master.set_solo"), noTarget,
                         QStringLiteral("solo"), true, QStringLiteral("solo"), masterPublic,
                         QStringLiteral("master solo"));

        auto insertNotesInput = commandArguments(runtime.documentVersion());
        insertNotesInput.insert(QStringLiteral("clip_id"), fixture.noteClipId.value());
        insertNotesInput.insert(
            QStringLiteral("notes"),
            QJsonArray{
                QJsonObject{{QStringLiteral("client_ref"), QStringLiteral("note-a")},
                            {QStringLiteral("local_start"), 0},
                            {QStringLiteral("length"), 480},
                            {QStringLiteral("key_index"), 60}},
                QJsonObject{{QStringLiteral("client_ref"), QStringLiteral("note-b")},
                            {QStringLiteral("local_start"), 720},
                            {QStringLiteral("length"), 480},
                            {QStringLiteral("key_index"), 64},
                            {QStringLiteral("lyric"), QStringLiteral("seed")},
                            {QStringLiteral("language"),
                             QJsonObject{{QStringLiteral("mode"), QStringLiteral("explicit")},
                                         {QStringLiteral("language_id"), QStringLiteral("en")}}}}
        });
        const auto noteInsertBase = runtime.documentVersion();
        const auto insertedNotes =
            registry.invoke(QStringLiteral("notes.insert"), insertNotesInput,
                            {.clientId = QStringLiteral("minimal-note-insert")});
        const auto noteIds = insertedNotes
                                 ? createdObjectIds(insertedNotes.get(), QStringLiteral("note"))
                                 : QList<int>{};
        const auto noteSnapshots =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        expect(insertedNotes && noteIds.size() == 2 && noteSnapshots &&
                   noteSnapshots.get().size() == 2 &&
                   runtime.documentVersion().revision == noteInsertBase.revision + 1 &&
                   resolvedValue(insertedNotes.get(), QStringLiteral("/notes/0/lyric")) ==
                       QStringLiteral("la") &&
                   resolvedValue(insertedNotes.get(), QStringLiteral("/notes/0/language"))
                       .isUndefined() &&
                   noteSnapshots.get().at(0).data.lyric == QStringLiteral("la") &&
                   noteSnapshots.get().at(0).data.language.isEmpty() &&
                   noteSnapshots.get().at(1).data.lyric == QStringLiteral("seed") &&
                   noteSnapshots.get().at(1).data.language == QStringLiteral("en"),
               QStringLiteral("notes.insert must accept complete leaf data without voice context "
                              "while resolving omitted GUI defaults"));
        if (noteIds.size() != 2)
            return std::nullopt;
        fixture.noteIds = {Automation::NoteId(noteIds.at(0)), Automation::NoteId(noteIds.at(1))};

        const auto noteById = [&](const Automation::NoteId noteId) {
            const auto notes =
                runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
            if (notes) {
                for (const auto &note : notes.get()) {
                    if (note.id == noteId)
                        return std::optional<Automation::NoteSnapshotDto>(note);
                }
            }
            return std::optional<Automation::NoteSnapshotDto>{};
        };
        const auto firstNoteBeforeMove = noteById(fixture.noteIds.at(0));
        const auto secondNoteBeforeMove = noteById(fixture.noteIds.at(1));
        auto moveNotesInput = commandArguments(runtime.documentVersion());
        moveNotesInput.insert(QStringLiteral("clip_id"), fixture.noteClipId.value());
        moveNotesInput.insert(
            QStringLiteral("note_ids"),
            QJsonArray{fixture.noteIds.at(0).value(), fixture.noteIds.at(1).value()});
        moveNotesInput.insert(QStringLiteral("delta_tick"), 120);
        moveNotesInput.insert(QStringLiteral("delta_key"), 2);
        const auto noteMoveBase = runtime.documentVersion();
        const auto movedNotes = registry.invoke(QStringLiteral("notes.move"), moveNotesInput,
                                                {.clientId = QStringLiteral("batch-note-move")});
        const auto firstNoteMoved = noteById(fixture.noteIds.at(0));
        const auto secondNoteMoved = noteById(fixture.noteIds.at(1));
        const auto undoNoteMove = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("batch-note-move-undo")});
        const auto firstNoteRestored = noteById(fixture.noteIds.at(0));
        const auto secondNoteRestored = noteById(fixture.noteIds.at(1));
        expect(movedNotes && firstNoteBeforeMove && secondNoteBeforeMove && firstNoteMoved &&
                   secondNoteMoved && undoNoteMove && firstNoteRestored && secondNoteRestored &&
                   runtime.documentVersion().revision == noteMoveBase.revision + 2 &&
                   firstNoteMoved->data.localStart == firstNoteBeforeMove->data.localStart + 120 &&
                   secondNoteMoved->data.keyIndex == secondNoteBeforeMove->data.keyIndex + 2 &&
                   firstNoteRestored->data.localStart == firstNoteBeforeMove->data.localStart &&
                   secondNoteRestored->data.keyIndex == secondNoteBeforeMove->data.keyIndex,
               QStringLiteral("notes.move batch must move and restore both notes in one undo"));

        auto removeNotesInput = commandArguments(runtime.documentVersion());
        removeNotesInput.insert(QStringLiteral("clip_id"), fixture.noteClipId.value());
        removeNotesInput.insert(
            QStringLiteral("note_ids"),
            QJsonArray{fixture.noteIds.at(0).value(), fixture.noteIds.at(1).value()});
        const auto noteRemoveBase = runtime.documentVersion();
        const auto removedNotes =
            registry.invoke(QStringLiteral("notes.remove"), removeNotesInput,
                            {.clientId = QStringLiteral("batch-note-remove")});
        const auto notesAfterRemove =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        const auto undoNoteRemove = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("batch-note-remove-undo")});
        const auto notesAfterUndo =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        expect(removedNotes && notesAfterRemove && notesAfterRemove.get().isEmpty() &&
                   undoNoteRemove && notesAfterUndo && notesAfterUndo.get().size() == 2 &&
                   runtime.documentVersion().revision == noteRemoveBase.revision + 2,
               QStringLiteral("notes.remove batch must delete and restore both notes in one undo"));

        const auto scalarClipBeforeMove = clipSnapshot(runtime, fixture.scalarClipId);
        const auto noteClipBeforeMove = clipSnapshot(runtime, fixture.noteClipId);
        auto moveClipsInput = commandArguments(runtime.documentVersion());
        moveClipsInput.insert(
            QStringLiteral("moves"),
            QJsonArray{
                QJsonObject{{QStringLiteral("clip_id"), fixture.scalarClipId.value()},
                            {QStringLiteral("target_track_id"), fixture.batchTrackA.value()},
                            {QStringLiteral("start"), 10000}},
                QJsonObject{{QStringLiteral("clip_id"), fixture.noteClipId.value()},
                            {QStringLiteral("target_track_id"), fixture.batchTrackA.value()},
                            {QStringLiteral("start"), 12000}}
        });
        const auto clipMoveBase = runtime.documentVersion();
        const auto movedClips = registry.invoke(QStringLiteral("clips.move"), moveClipsInput,
                                                {.clientId = QStringLiteral("batch-clip-move")});
        const auto scalarClipMoved = clipSnapshot(runtime, fixture.scalarClipId);
        const auto noteClipMoved = clipSnapshot(runtime, fixture.noteClipId);
        const auto undoClipMove = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("batch-clip-move-undo")});
        const auto scalarClipRestored = clipSnapshot(runtime, fixture.scalarClipId);
        const auto noteClipRestored = clipSnapshot(runtime, fixture.noteClipId);
        expect(movedClips && scalarClipBeforeMove && noteClipBeforeMove && scalarClipMoved &&
                   noteClipMoved && undoClipMove && scalarClipRestored && noteClipRestored &&
                   runtime.documentVersion().revision == clipMoveBase.revision + 2 &&
                   scalarClipMoved->trackId == fixture.batchTrackA &&
                   noteClipMoved->trackId == fixture.batchTrackA &&
                   scalarClipRestored->trackId == scalarClipBeforeMove->trackId &&
                   noteClipRestored->data.properties.start ==
                       noteClipBeforeMove->data.properties.start,
               QStringLiteral("clips.move batch must move and restore both clips in one undo"));

        auto removeClipsInput = commandArguments(runtime.documentVersion());
        removeClipsInput.insert(QStringLiteral("clip_ids"), QJsonArray{fixture.scalarClipId.value(),
                                                                       fixture.noteClipId.value()});
        const auto clipRemoveBase = runtime.documentVersion();
        const auto removedClips =
            registry.invoke(QStringLiteral("clips.remove"), removeClipsInput,
                            {.clientId = QStringLiteral("batch-clip-remove")});
        const auto clipsAbsent = !clipSnapshot(runtime, fixture.scalarClipId) &&
                                 !clipSnapshot(runtime, fixture.noteClipId);
        const auto undoClipRemove = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("batch-clip-remove-undo")});
        const auto restoredNotes =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        expect(removedClips && clipsAbsent && undoClipRemove &&
                   clipSnapshot(runtime, fixture.scalarClipId) &&
                   clipSnapshot(runtime, fixture.noteClipId) && restoredNotes &&
                   restoredNotes.get().size() == 2 &&
                   runtime.documentVersion().revision == clipRemoveBase.revision + 2,
               QStringLiteral("clips.remove batch must restore complete clips in one undo"));

        return fixture;
    }

    void verifyDuplicateSplitAndFillBehavior(Automation::PublicAutomationRegistry &registry,
                                             Automation::CoreRuntime &runtime,
                                             const PublicEditingFixture &fixture) {
        auto duplicateNotes = commandArguments(runtime.documentVersion());
        duplicateNotes.insert(QStringLiteral("source_clip_id"), fixture.noteClipId.value());
        duplicateNotes.insert(
            QStringLiteral("note_ids"),
            QJsonArray{fixture.noteIds.at(0).value(), fixture.noteIds.at(1).value()});
        duplicateNotes.insert(QStringLiteral("target_clip_id"), fixture.noteClipId.value());
        duplicateNotes.insert(QStringLiteral("target_start"), 1440);
        const auto duplicateNotesBase = runtime.documentVersion();
        const auto duplicatedNotes =
            registry.invoke(QStringLiteral("notes.duplicate"), duplicateNotes,
                            {.clientId = QStringLiteral("duplicate-notes-created-objects")});
        const auto duplicatedNoteIds =
            duplicatedNotes ? createdObjectIds(duplicatedNotes.get(), QStringLiteral("note"))
                            : QList<int>{};
        const auto notesAfterDuplicate =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        const auto undoDuplicateNotes = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("duplicate-notes-undo")});
        const auto notesAfterDuplicateUndo =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        expect(duplicatedNotes && duplicatedNoteIds.size() == 2 && notesAfterDuplicate &&
                   notesAfterDuplicate.get().size() == 4 && undoDuplicateNotes &&
                   notesAfterDuplicateUndo && notesAfterDuplicateUndo.get().size() == 2 &&
                   runtime.documentVersion().revision == duplicateNotesBase.revision + 2,
               QStringLiteral(
                   "notes.duplicate must report every created note and undo the batch once"));

        auto splitNote = commandArguments(runtime.documentVersion());
        splitNote.insert(QStringLiteral("clip_id"), fixture.noteClipId.value());
        splitNote.insert(QStringLiteral("note_id"), fixture.noteIds.first().value());
        splitNote.insert(QStringLiteral("local_position"), 240);
        const auto splitBase = runtime.documentVersion();
        const auto split =
            registry.invoke(QStringLiteral("notes.split_at"), splitNote,
                            {.clientId = QStringLiteral("split-note-created-object")});
        const auto splitCreated =
            split ? createdObjectIds(split.get(), QStringLiteral("note")) : QList<int>{};
        const auto notesAfterSplit =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        bool hasSecondHalf = false;
        if (notesAfterSplit && splitCreated.size() == 1) {
            for (const auto &note : notesAfterSplit.get()) {
                if (note.id.value() == splitCreated.first()) {
                    hasSecondHalf = note.data.localStart == 240 && note.data.length == 240 &&
                                    note.data.lyric == QStringLiteral("-");
                }
            }
        }
        const auto undoSplit = registry.invoke(QStringLiteral("history.undo"),
                                               commandArguments(runtime.documentVersion()),
                                               {.clientId = QStringLiteral("split-note-undo")});
        expect(split && splitCreated.size() == 1 && hasSecondHalf && undoSplit &&
                   runtime.documentVersion().revision == splitBase.revision + 2,
               QStringLiteral("notes.split_at must report and atomically undo the new half"));

        auto fill = commandArguments(runtime.documentVersion());
        fill.insert(QStringLiteral("clip_id"), fixture.noteClipId.value());
        fill.insert(QStringLiteral("note_ids"),
                    QJsonArray{fixture.noteIds.first().value(), fixture.noteIds.last().value()});
        fill.insert(QStringLiteral("text"), QStringLiteral("AB"));
        fill.insert(QStringLiteral("options"),
                    QJsonObject{
                        {QStringLiteral("splitter_id"), QStringLiteral("character")},
                        {QStringLiteral("tagger_id"),   QStringLiteral("auto")     },
                        {QStringLiteral("language"),
                         QJsonObject{
                             {QStringLiteral("mode"), QStringLiteral("follow_singer")},
                         }                                                         },
        });
        const auto fillBase = runtime.documentVersion();
        const auto filled = registry.invoke(QStringLiteral("notes.fill_lyrics"), fill,
                                            {.clientId = QStringLiteral("fill-character")});
        if (!filled)
            QTextStream(stderr) << "DETAIL notes.fill_lyrics: "
                                << Automation::errorCodeName(filled.getError().code) << ", "
                                << filled.getError().fieldPath << ", " << filled.getError().message
                                << Qt::endl;
        const auto filledNotes =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        const auto undoFill = registry.invoke(QStringLiteral("history.undo"),
                                              commandArguments(runtime.documentVersion()),
                                              {.clientId = QStringLiteral("fill-character-undo")});
        const auto restoredNotes =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        expect(
            filled && filledNotes && filledNotes.get().size() == 2 &&
                filledNotes.get().at(0).data.lyric == QStringLiteral("A") &&
                filledNotes.get().at(1).data.lyric == QStringLiteral("B") &&
                filledNotes.get().at(0).data.language.isEmpty() && undoFill && restoredNotes &&
                restoredNotes.get().at(0).data.lyric == QStringLiteral("la") &&
                runtime.documentVersion().revision == fillBase.revision + 2,
            QStringLiteral("notes.fill_lyrics character mode must consume strategy and undo once"));

        auto duplicateFill = fill;
        duplicateFill.insert(
            QStringLiteral("note_ids"),
            QJsonArray{fixture.noteIds.first().value(), fixture.noteIds.first().value()});
        duplicateFill.insert(QStringLiteral("expected_revision"),
                             static_cast<qint64>(runtime.documentVersion().revision));
        const auto duplicateFillBase = runtime.documentVersion();
        const auto rejectedFill =
            registry.invoke(QStringLiteral("notes.fill_lyrics"), duplicateFill,
                            {.clientId = QStringLiteral("fill-duplicate-rejection")});
        expect(!rejectedFill &&
                   rejectedFill.getError().code ==
                       Automation::AutomationErrorCode::InvalidArgument &&
                   rejectedFill.getError().fieldPath == QStringLiteral("note_ids") &&
                   runtime.documentVersion() == duplicateFillBase,
               QStringLiteral("notes.fill_lyrics must reject duplicate note IDs without mutation"));

        auto mixedOffsets = commandArguments(runtime.documentVersion());
        mixedOffsets.insert(QStringLiteral("clip_id"), fixture.noteClipId.value());
        mixedOffsets.insert(QStringLiteral("notes"),
                            QJsonArray{
                                QJsonObject{
                                            {QStringLiteral("client_ref"), QStringLiteral("mixed-offsets")},
                                            {QStringLiteral("local_start"), 2400},
                                            {QStringLiteral("length"), 480},
                                            {QStringLiteral("key_index"), 60},
                                            {QStringLiteral("phonemes"),
                                     QJsonArray{
                                         QJsonObject{
                                             {QStringLiteral("symbol"), QStringLiteral("a")},
                                             {QStringLiteral("offset"), 0},
                                         },
                                         QJsonObject{
                                             {QStringLiteral("symbol"), QStringLiteral("b")},
                                         },
                                     }},
                                            }
        });
        const auto mixedOffsetsBase = runtime.documentVersion();
        const auto rejectedMixedOffsets =
            registry.invoke(QStringLiteral("notes.insert"), mixedOffsets,
                            {.clientId = QStringLiteral("mixed-phoneme-offset-rejection")});
        expect(!rejectedMixedOffsets &&
                   rejectedMixedOffsets.getError().code ==
                       Automation::AutomationErrorCode::InvalidArgument &&
                   rejectedMixedOffsets.getError().fieldPath ==
                       QStringLiteral("notes.0.phonemes") &&
                   runtime.documentVersion() == mixedOffsetsBase,
               QStringLiteral("notes.insert must reject partial phoneme offsets without mutation"));

        auto setLanguage = commandArguments(runtime.documentVersion());
        setLanguage.insert(QStringLiteral("clip_id"), fixture.noteClipId.value());
        setLanguage.insert(QStringLiteral("note_ids"), QJsonArray{fixture.noteIds.last().value()});
        setLanguage.insert(QStringLiteral("language"),
                           QJsonObject{
                               {QStringLiteral("mode"), QStringLiteral("follow_singer")}
        });
        const auto languageBase = runtime.documentVersion();
        const auto inheritedLanguage =
            registry.invoke(QStringLiteral("notes.set_language"), setLanguage,
                            {.clientId = QStringLiteral("note-language-follow-singer")});
        if (!inheritedLanguage) {
            const auto &failure = inheritedLanguage.getError();
            QTextStream(stderr) << "DETAIL notes.set_language: "
                                << Automation::errorCodeName(failure.code) << ", "
                                << failure.fieldPath << ", " << failure.message << Qt::endl;
        }
        const auto inheritedNotes =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        const auto undoInheritedLanguage = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()));
        const auto restoredLanguageNotes =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        expect(inheritedLanguage && inheritedNotes &&
                   inheritedNotes.get().last().data.language.isEmpty() && undoInheritedLanguage &&
                   restoredLanguageNotes &&
                   restoredLanguageNotes.get().last().data.language == QStringLiteral("en") &&
                   runtime.documentVersion().revision == languageBase.revision + 2,
               QStringLiteral(
                   "notes.set_language follow_singer must preserve inherited language semantics"));

        auto duplicateClip = commandArguments(runtime.documentVersion());
        duplicateClip.insert(QStringLiteral("clip_ids"), QJsonArray{fixture.noteClipId.value()});
        duplicateClip.insert(QStringLiteral("destination"),
                             QJsonObject{
                                 {QStringLiteral("target_track_id"), fixture.trackId.value()},
                                 {QStringLiteral("start"),           16000                  },
        });
        const auto duplicateClipBase = runtime.documentVersion();
        const auto duplicatedClip =
            registry.invoke(QStringLiteral("clips.duplicate"), duplicateClip,
                            {.clientId = QStringLiteral("duplicate-clip-created-object")});
        if (!duplicatedClip)
            QTextStream(stderr) << "DETAIL clips.duplicate: "
                                << Automation::errorCodeName(duplicatedClip.getError().code) << ", "
                                << duplicatedClip.getError().fieldPath << ", "
                                << duplicatedClip.getError().message << Qt::endl;
        const auto duplicatedClipIds =
            duplicatedClip ? createdObjectIds(duplicatedClip.get(), QStringLiteral("clip"))
                           : QList<int>{};
        const auto copied =
            duplicatedClipIds.size() == 1
                ? clipSnapshot(runtime, Automation::ClipId(duplicatedClipIds.first()))
                : std::nullopt;
        const auto undoDuplicateClip = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("duplicate-clip-undo")});
        expect(duplicatedClip && duplicatedClipIds.size() == 1 && copied &&
                   copied->data.properties.start == 16000 && copied->data.notes.size() == 2 &&
                   undoDuplicateClip &&
                   !clipSnapshot(runtime, Automation::ClipId(duplicatedClipIds.value(0))) &&
                   runtime.documentVersion().revision == duplicateClipBase.revision + 2,
               QStringLiteral("clips.duplicate must report a deep-copied clip and undo it once"));
    }

    void verifyFormatInspectionAndMidiPreview(Automation::PublicAutomationRegistry &registry,
                                              Automation::CoreRuntime &runtime, AppModel &model,
                                              const PublicEditingFixture &fixture,
                                              const QString &directory) {
        const auto *inspectContract =
            AutomationWire::findPublicTool(QStringLiteral("formats.inspect"));
        const auto *importContract =
            AutomationWire::findPublicTool(QStringLiteral("documents.import"));
        const auto *relocateContract =
            AutomationWire::findPublicTool(QStringLiteral("audio_clips.relocate"));
        const auto *confirmContract =
            AutomationWire::findPublicTool(QStringLiteral("audio_clips.confirm_path"));
        const auto inspectPurposes =
            inspectContract ? inspectContract->inputSchema.value(QStringLiteral("properties"))
                                  .toObject()
                                  .value(QStringLiteral("purpose"))
                                  .toObject()
                                  .value(QStringLiteral("enum"))
                                  .toArray()
                            : QJsonArray{};
        const auto importOptions =
            importContract ? importContract->inputSchema.value(QStringLiteral("properties"))
                                 .toObject()
                                 .value(QStringLiteral("options"))
                                 .toObject()
                                 .value(QStringLiteral("properties"))
                                 .toObject()
                           : QJsonObject{};
        expect(inspectPurposes == QJsonArray{QStringLiteral("open"), QStringLiteral("import")} &&
                   !importOptions.contains(QStringLiteral("import_lyrics")) && relocateContract &&
                   relocateContract->syncMode == AutomationWire::SyncMode::Synchronous &&
                   confirmContract &&
                   confirmContract->syncMode == AutomationWire::SyncMode::Synchronous,
               QStringLiteral("format and audio contracts must expose only implemented semantics"));

        const auto midiPath = QDir(directory).filePath(QStringLiteral("inspection.mid"));
        MidiConverter converter;
        QString exportError;
        const auto firstSave = converter.save(midiPath, &model, exportError);
        const QJsonObject inspectInput{
            {QStringLiteral("path"),    midiPath              },
            {QStringLiteral("purpose"), QStringLiteral("open")},
        };
        const auto firstPlan = registry.invoke(QStringLiteral("formats.inspect"), inspectInput);
        if (!firstPlan)
            QTextStream(stderr) << "DETAIL formats.inspect first: "
                                << Automation::errorCodeName(firstPlan.getError().code) << ", "
                                << firstPlan.getError().fieldPath << ", "
                                << firstPlan.getError().message << Qt::endl;

        auto setLyric = commandArguments(runtime.documentVersion());
        setLyric.insert(QStringLiteral("clip_id"), fixture.noteClipId.value());
        setLyric.insert(QStringLiteral("note_id"), fixture.noteIds.first().value());
        setLyric.insert(QStringLiteral("lyric"), QStringLiteral("digest-change"));
        const auto changedLyric = registry.invoke(QStringLiteral("notes.set_lyric"), setLyric);
        exportError.clear();
        const auto secondSave = converter.save(midiPath, &model, exportError);
        const auto secondPlan = registry.invoke(QStringLiteral("formats.inspect"), inspectInput);
        const auto undoLyric = registry.invoke(QStringLiteral("history.undo"),
                                               commandArguments(runtime.documentVersion()));
        const auto firstDigest =
            firstPlan ? firstPlan.get().value(QStringLiteral("plan_digest")).toString() : QString();
        const auto secondDigest =
            secondPlan ? secondPlan.get().value(QStringLiteral("plan_digest")).toString()
                       : QString();
        expect(firstSave && firstPlan &&
                   !firstPlan.get().value(QStringLiteral("sources")).toArray().isEmpty() &&
                   !firstPlan.get().value(QStringLiteral("encoding")).toString().isEmpty() &&
                   !firstPlan.get().value(QStringLiteral("lyrics_preview")).toArray().isEmpty() &&
                   changedLyric && secondSave && secondPlan && firstDigest != secondDigest &&
                   undoLyric,
               QStringLiteral(
                   "formats.inspect must parse MIDI content and bind the digest to file bytes"));

        auto staleOpen = QJsonObject{
            {QStringLiteral("current_document_id"),
             runtime.documentVersion().documentId.toString()                 },
            {QStringLiteral("expected_revision"),
             static_cast<qint64>(runtime.documentVersion().revision)         },
            {QStringLiteral("path"),                midiPath                 },
            {QStringLiteral("format_id"),           QStringLiteral("midi")   },
            {QStringLiteral("plan_digest"),         firstDigest              },
            {QStringLiteral("unsaved_policy"),      QStringLiteral("discard")},
        };
        const auto stale = registry.invoke(QStringLiteral("documents.open"), staleOpen);
        expect(!stale &&
                   stale.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
                   stale.getError().fieldPath == QStringLiteral("plan_digest"),
               QStringLiteral("documents.open must reject a stale content inspection digest"));

        const auto previewPath = QDir(directory).filePath(QStringLiteral("selected.mid"));
        const auto preview = registry.invoke(
            QStringLiteral("exports.midi.preview"),
            QJsonObject{
                {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
                {QStringLiteral("path"),        previewPath                                    },
                {QStringLiteral("options"),
                 QJsonObject{
                     {QStringLiteral("include_tempo"), false},
                     {QStringLiteral("include_time_signatures"), false},
                     {QStringLiteral("include_lyrics"), false},
                     {QStringLiteral("clip_ids"), QJsonArray{fixture.noteClipId.value()}},
                 }                                                                             },
        });
        if (!preview)
            QTextStream(stderr) << "DETAIL exports.midi.preview: "
                                << Automation::errorCodeName(preview.getError().code) << ", "
                                << preview.getError().fieldPath << ", "
                                << preview.getError().message << Qt::endl;
        const auto previewPlan =
            preview ? preview.get().value(QStringLiteral("plan")).toObject() : QJsonObject{};
        const auto invalidPreview = registry.invoke(
            Automation::OperationIds::exports::midi::preview,
            QJsonObject{
                {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()    },
                {QStringLiteral("path"),        QDir(directory).filePath(QStringLiteral("bad.wav"))},
                {QStringLiteral("options"),     QJsonObject{}                                      },
        });
        expect(preview && !QFileInfo::exists(previewPath) &&
                   !previewPlan.value(QStringLiteral("include_tempo")).toBool(true) &&
                   !previewPlan.value(QStringLiteral("include_time_signatures")).toBool(true) &&
                   !previewPlan.value(QStringLiteral("include_lyrics")).toBool(true) &&
                   previewPlan.value(QStringLiteral("clip_ids")).toArray() ==
                       QJsonArray{fixture.noteClipId.value()} &&
                   !previewPlan.value(QStringLiteral("track_ids")).toArray().isEmpty() &&
                   !invalidPreview &&
                   invalidPreview.getError().code ==
                       Automation::AutomationErrorCode::FormatUnsupported &&
                   invalidPreview.getError().operationId ==
                       Automation::OperationIds::exports::midi::preview,
               QStringLiteral(
                   "exports.midi.preview must remain a non-writing query with its own identity"));
    }

    void verifyCapabilityRouting(Automation::PublicAutomationRegistry &registry,
                                 Automation::CoreRuntime &runtime,
                                 const Automation::ClipId audioClipId,
                                 const Automation::ClipId singingClipId) {
        const auto current = runtime.documentVersion();
        const QJsonObject currentDocument{
            {QStringLiteral("document_id"), current.documentId.toString()},
        };
        const auto audioCapabilities =
            registry.invoke(QStringLiteral("exports.audio.get_capabilities"), currentDocument);
        const auto audioEnvelope =
            audioCapabilities ? audioCapabilities.get().value(QStringLiteral("document")).toObject()
                              : QJsonObject{};
        expect(
            audioCapabilities &&
                audioEnvelope.value(QStringLiteral("document_id")).toString() ==
                    current.documentId.toString() &&
                audioEnvelope.value(QStringLiteral("revision")).toInteger() ==
                    static_cast<qint64>(current.revision),
            QStringLiteral("audio capability envelope must use the validated document snapshot"));

        const QJsonObject commonAudioOptions{
            {QStringLiteral("format"),       QStringLiteral("wav")   },
            {QStringLiteral("sample_rate"),  44100                   },
            {QStringLiteral("channel_mode"), QStringLiteral("stereo")},
            {QStringLiteral("mixing_mode"),  QStringLiteral("mixed") },
        };
        const auto invalidAudioPreview = [&](QJsonObject options) {
            return registry.invoke(
                QStringLiteral("exports.audio.preview"),
                QJsonObject{
                    {QStringLiteral("document_id"), current.documentId.toString()},
                    {QStringLiteral("path"),        QStringLiteral("invalid.wav")},
                    {QStringLiteral("options"),     std::move(options)           },
            });
        };
        auto allWithIds = commonAudioOptions;
        allWithIds.insert(QStringLiteral("source"), QStringLiteral("all"));
        allWithIds.insert(QStringLiteral("source_ids"), QJsonArray{1});
        auto customWithoutIds = commonAudioOptions;
        customWithoutIds.insert(QStringLiteral("source"), QStringLiteral("custom"));
        auto allWithRange = commonAudioOptions;
        allWithRange.insert(QStringLiteral("source"), QStringLiteral("all"));
        allWithRange.insert(QStringLiteral("range"),
                            QJsonObject{
                                {QStringLiteral("start"), 0  },
                                {QStringLiteral("end"),   480}
        });
        const auto rejectedAllIds = invalidAudioPreview(allWithIds);
        const auto rejectedCustomMissing = invalidAudioPreview(customWithoutIds);
        const auto rejectedRange = invalidAudioPreview(allWithRange);
        expect(
            !rejectedAllIds && !rejectedCustomMissing && !rejectedRange &&
                rejectedAllIds.getError().code ==
                    Automation::AutomationErrorCode::InvalidArgument &&
                rejectedCustomMissing.getError().code ==
                    Automation::AutomationErrorCode::InvalidArgument &&
                rejectedRange.getError().code == Automation::AutomationErrorCode::InvalidArgument,
            QStringLiteral("audio export Registry must enforce conditional sources and no range"));

        const auto invalidUuid =
            registry.invoke(QStringLiteral("exports.audio.get_capabilities"),
                            QJsonObject{
                                {QStringLiteral("document_id"), QStringLiteral("not-a-uuid")}
        });
        const auto wrongGeneration = registry.invoke(
            QStringLiteral("exports.audio.get_capabilities"),
            QJsonObject{
                {QStringLiteral("document_id"), Automation::DocumentId::create().toString()}
        });
        expect(
            !invalidUuid &&
                invalidUuid.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
                !wrongGeneration &&
                wrongGeneration.getError().code == Automation::AutomationErrorCode::DocumentChanged,
            QStringLiteral("capability queries must reject malformed and stale document IDs"));

        const auto extractionArguments = [&](const Automation::ClipId clipId) {
            return QJsonObject{
                {QStringLiteral("document_id"),          current.documentId.toString()},
                {QStringLiteral("source_audio_clip_id"), clipId.value()               },
            };
        };
        const auto extractionCapabilities = registry.invoke(
            QStringLiteral("extract.get_capabilities"), extractionArguments(audioClipId));
        const auto extractionEnvelope =
            extractionCapabilities
                ? extractionCapabilities.get().value(QStringLiteral("document")).toObject()
                : QJsonObject{};
        const auto missingClip = registry.invoke(QStringLiteral("extract.get_capabilities"),
                                                 extractionArguments(Automation::ClipId(999999)));
        const auto wrongClip = registry.invoke(QStringLiteral("extract.get_capabilities"),
                                               extractionArguments(singingClipId));
        auto staleExtractionArguments = extractionArguments(audioClipId);
        staleExtractionArguments.insert(QStringLiteral("document_id"),
                                        Automation::DocumentId::create().toString());
        const auto staleExtraction =
            registry.invoke(QStringLiteral("extract.get_capabilities"), staleExtractionArguments);
        expect(extractionCapabilities &&
                   extractionEnvelope.value(QStringLiteral("document_id")).toString() ==
                       current.documentId.toString() &&
                   extractionEnvelope.value(QStringLiteral("revision")).toInteger() ==
                       static_cast<qint64>(current.revision) &&
                   !missingClip &&
                   missingClip.getError().code == Automation::AutomationErrorCode::NotFound &&
                   !wrongClip &&
                   wrongClip.getError().code == Automation::AutomationErrorCode::WrongObjectType &&
                   !staleExtraction &&
                   staleExtraction.getError().code ==
                       Automation::AutomationErrorCode::DocumentChanged,
               QStringLiteral(
                   "extraction capabilities must validate document, presence, and clip type"));

        const auto invalidMidiDestination = [&](const QJsonObject &destination) {
            auto arguments = commandArguments(runtime.documentVersion());
            arguments.insert(QStringLiteral("source_audio_clip_id"), audioClipId.value());
            arguments.insert(QStringLiteral("destination"), destination);
            arguments.insert(QStringLiteral("options"), QJsonObject{});
            return registry.invoke(QStringLiteral("extract.midi.start"), arguments);
        };
        const auto createWithTarget = invalidMidiDestination(QJsonObject{
            {QStringLiteral("target_track_id"), 1                            },
            {QStringLiteral("start"),           0                            },
            {QStringLiteral("mode"),            QStringLiteral("create_clip")},
            {QStringLiteral("target_clip_id"),  singingClipId.value()        },
        });
        const auto mergeWithoutTarget = invalidMidiDestination(QJsonObject{
            {QStringLiteral("target_track_id"), 1                                },
            {QStringLiteral("start"),           0                                },
            {QStringLiteral("mode"),            QStringLiteral("merge_into_clip")},
        });
        expect(
            !createWithTarget && !mergeWithoutTarget &&
                createWithTarget.getError().code ==
                    Automation::AutomationErrorCode::InvalidArgument &&
                mergeWithoutTarget.getError().code ==
                    Automation::AutomationErrorCode::InvalidArgument,
            QStringLiteral("MIDI extraction Registry must enforce its destination discriminator"));

        const auto inferenceCapabilities =
            registry.invoke(QStringLiteral("inference.get_capabilities"),
                            QJsonObject{
                                {QStringLiteral("document_id"), current.documentId.toString()     },
                                {QStringLiteral("scope"),
                                 QJsonObject{{QStringLiteral("kind"), QStringLiteral("document")}}},
        });
        expect(
            inferenceCapabilities && inferenceCapabilities.get()
                                         .value(QStringLiteral("capabilities"))
                                         .toObject()
                                         .value(QStringLiteral("providers"))
                                         .toArray()
                                         .isEmpty(),
            QStringLiteral("an unavailable inference provider must be omitted, not encoded empty"));

        const auto undecodedAudio =
            registry.invoke(QStringLiteral("audio_clips.get"),
                            QJsonObject{
                                {QStringLiteral("document_id"), current.documentId.toString()},
                                {QStringLiteral("clip_id"),     audioClipId.value()          },
        });
        const auto undecodedSnapshot =
            undecodedAudio ? undecodedAudio.get().value(QStringLiteral("snapshot")).toObject()
                           : QJsonObject{};
        expect(undecodedAudio &&
                   undecodedSnapshot.value(QStringLiteral("duration_seconds")).isNull() &&
                   undecodedSnapshot.value(QStringLiteral("sample_rate")).isNull() &&
                   undecodedSnapshot.value(QStringLiteral("channels")).isNull(),
               QStringLiteral("undecoded audio metadata must remain null instead of fabricated"));
    }

    QJsonObject voiceSelection(const SingerInfo &singer, const SpeakerInfo &speaker) {
        return {
            {QStringLiteral("singer"),
             QJsonObject{
                 {QStringLiteral("package_id"), singer.packageId()},
                 {QStringLiteral("package_version"), singer.packageVersion().toString()},
                 {QStringLiteral("singer_id"), singer.singerId()},
             }                                                                                   },
            {QStringLiteral("speaker"), QJsonObject{{QStringLiteral("speaker_id"), speaker.id()}}},
        };
    }

    QJsonObject fixedMix(const SingerInfo &singer, const SpeakerInfo &speakerA,
                         const SpeakerInfo &speakerB) {
        return {
            {QStringLiteral("singer"),
             QJsonObject{
                 {QStringLiteral("package_id"), singer.packageId()},
                 {QStringLiteral("package_version"), singer.packageVersion().toString()},
                 {QStringLiteral("singer_id"), singer.singerId()},
             }},
            {QStringLiteral("sources"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("speaker"),
                      QJsonObject{{QStringLiteral("speaker_id"), speakerA.id()}}},
                     {QStringLiteral("weight"), 0.25},
                 },
                 QJsonObject{
                     {QStringLiteral("speaker"),
                      QJsonObject{{QStringLiteral("speaker_id"), speakerB.id()}}},
                     {QStringLiteral("weight"), 0.75},
                 },
             }},
        };
    }

    bool invokeChangedOnce(Automation::PublicAutomationRegistry &registry,
                           Automation::CoreRuntime &runtime, const QString &operationId,
                           const QJsonObject &operationArguments, const QString &clientId,
                           const QString &label) {
        const auto before = runtime.documentVersion();
        const auto result = registry.invoke(
            operationId, mergeCommandArguments(before, operationArguments), {.clientId = clientId});
        const bool succeeded = result && result.get().value(QStringLiteral("changed")).toBool() &&
                               runtime.documentVersion().revision == before.revision + 1;
        expect(succeeded, label + QStringLiteral(" must succeed as exactly one revision"));
        return succeeded;
    }

    QJsonObject voiceContextSnapshot(Automation::PublicAutomationRegistry &registry,
                                     Automation::CoreRuntime &runtime, const QString &operationId,
                                     const QString &idField, const int id) {
        const auto result = registry.invoke(
            operationId,
            QJsonObject{
                {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
                {idField,                       id                                             },
        });
        return result ? result.get()
                            .value(QStringLiteral("snapshot"))
                            .toObject()
                            .value(QStringLiteral("voice_context"))
                            .toObject()
                      : QJsonObject{};
    }

    QJsonObject speakerMixSnapshot(Automation::PublicAutomationRegistry &registry,
                                   Automation::CoreRuntime &runtime, const QString &targetType,
                                   const int targetId) {
        const auto result = registry.invoke(
            QStringLiteral("speaker_mix.get"),
            QJsonObject{
                {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
                {QStringLiteral("target"),
                 QJsonObject{
                     {QStringLiteral("type"), targetType},
                     {QStringLiteral("id"), targetId},
                 }                                                                             },
        });
        return result ? result.get().value(QStringLiteral("snapshot")).toObject() : QJsonObject{};
    }

    QJsonObject speakerMixKeyframeAt(const QJsonObject &snapshot, const int position) {
        for (const auto &value : snapshot.value(QStringLiteral("keyframes")).toArray()) {
            const auto keyframe = value.toObject();
            if (keyframe.value(QStringLiteral("position")).toInt() == position)
                return keyframe;
        }
        return {};
    }

    void verifyPhonemeOriginalPreservation(Automation::PublicAutomationRegistry &registry,
                                           Automation::CoreRuntime &runtime,
                                           const PublicEditingFixture &fixture) {
        PhonemeName originalOnset;
        originalOnset.language = QStringLiteral("en");
        originalOnset.name = QStringLiteral("l");
        originalOnset.isOnset = true;
        PhonemeName originalVowel;
        originalVowel.language = QStringLiteral("en");
        originalVowel.name = QStringLiteral("a");
        PhonemeName editedOnset = originalOnset;
        editedOnset.name = QStringLiteral("r");
        PhonemeName editedVowel = originalVowel;
        editedVowel.name = QStringLiteral("ah");
        const QList<PhonemeName> originalNames{originalOnset, originalVowel};
        const QList<int> originalOffsets{-40, 80};
        Phonemes seeded;
        seeded.nameSeq.original = originalNames;
        seeded.nameSeq.edited = {editedOnset, editedVowel};
        seeded.offsetSeq.original = originalOffsets;
        seeded.offsetSeq.edited = {-20, 100};
        Automation::CommandContext setup;
        setup.expected = runtime.documentVersion();
        setup.source = Automation::InvocationSource::InternalAutomation;
        setup.clientId = QStringLiteral("phoneme-original-setup");
        const auto seededResult =
            runtime.notes().setPhonemes(setup, fixture.noteClipId, fixture.noteIds.first(), seeded);
        expect(seededResult && seededResult.get().changed,
               QStringLiteral("phoneme original-preservation fixture must be created"));

        invokeChangedOnce(
            registry, runtime, QStringLiteral("notes.set_phonemes"),
            QJsonObject{
                {QStringLiteral("clip_id"), fixture.noteClipId.value()                          },
                {QStringLiteral("note_id"), fixture.noteIds.first().value()                     },
                {QStringLiteral("names"),   QJsonArray{QStringLiteral("m"), QStringLiteral("i")}},
        },
            QStringLiteral("set-phonemes-preserve-original"), QStringLiteral("notes.set_phonemes"));
        const auto noteAfterSet =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        std::optional<Automation::NoteSnapshotDto> setSnapshot;
        if (noteAfterSet) {
            for (const auto &note : noteAfterSet.get()) {
                if (note.id == fixture.noteIds.first()) {
                    setSnapshot = note;
                    break;
                }
            }
        }
        expect(setSnapshot && setSnapshot->data.phonemes.nameSeq.original == originalNames &&
                   setSnapshot->data.phonemes.offsetSeq.original == originalOffsets &&
                   setSnapshot->data.phonemes.nameSeq.edited.size() == 2 &&
                   setSnapshot->data.phonemes.nameSeq.edited.at(0).name == QStringLiteral("m") &&
                   setSnapshot->data.phonemes.offsetSeq.edited.isEmpty(),
               QStringLiteral("notes.set_phonemes must preserve both original sequences"));

        invokeChangedOnce(registry, runtime, QStringLiteral("notes.set_phoneme_offsets"),
                          QJsonObject{
                              {QStringLiteral("clip_id"), fixture.noteClipId.value()     },
                              {QStringLiteral("note_id"), fixture.noteIds.first().value()},
                              {QStringLiteral("offsets"), QJsonArray{-10, 120}           },
        },
                          QStringLiteral("set-phoneme-offsets-before-reset"),
                          QStringLiteral("notes.set_phoneme_offsets"));
        invokeChangedOnce(registry, runtime, QStringLiteral("notes.reset_phonemes"),
                          QJsonObject{
                              {QStringLiteral("clip_id"), fixture.noteClipId.value()     },
                              {QStringLiteral("note_id"), fixture.noteIds.first().value()},
        },
                          QStringLiteral("reset-phonemes-preserve-original"),
                          QStringLiteral("notes.reset_phonemes"));
        const auto noteAfterReset =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        std::optional<Automation::NoteSnapshotDto> resetSnapshot;
        if (noteAfterReset) {
            for (const auto &note : noteAfterReset.get()) {
                if (note.id == fixture.noteIds.first()) {
                    resetSnapshot = note;
                    break;
                }
            }
        }
        expect(resetSnapshot && resetSnapshot->data.phonemes.nameSeq.original == originalNames &&
                   resetSnapshot->data.phonemes.offsetSeq.original == originalOffsets &&
                   resetSnapshot->data.phonemes.nameSeq.edited.isEmpty() &&
                   resetSnapshot->data.phonemes.offsetSeq.edited.isEmpty() &&
                   resetSnapshot->data.phonemes.nameSeq.result() == originalNames &&
                   resetSnapshot->data.phonemes.offsetSeq.result() == originalOffsets,
               QStringLiteral("notes.reset_phonemes must clear edits and fall back to originals"));

        Phonemes firstCascade;
        firstCascade.nameSeq.original = originalNames;
        firstCascade.offsetSeq.original = {-40, 850};
        firstCascade.offsetSeq.edited = {0, 700};
        Phonemes secondCascade;
        secondCascade.nameSeq.original = originalNames;
        secondCascade.offsetSeq.original = {0, 200};
        secondCascade.offsetSeq.edited = {-100, 200};
        setup.expected = runtime.documentVersion();
        setup.clientId = QStringLiteral("phoneme-cascade-first-setup");
        const auto firstCascadeSeed = runtime.notes().setPhonemes(
            setup, fixture.noteClipId, fixture.noteIds.first(), firstCascade);
        setup.expected = runtime.documentVersion();
        setup.clientId = QStringLiteral("phoneme-cascade-second-setup");
        const auto secondCascadeSeed = runtime.notes().setPhonemes(
            setup, fixture.noteClipId, fixture.noteIds.last(), secondCascade);
        expect(firstCascadeSeed && secondCascadeSeed,
               QStringLiteral("public phoneme reset cascade fixture must be created"));
        const auto seededCascade =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        expect(seededCascade && seededCascade.get().size() == 2 &&
                   seededCascade.get().first().data.phonemes.offsetSeq.edited ==
                       firstCascade.offsetSeq.edited,
               QStringLiteral("public phoneme reset first cascade fixture must remain valid"));
        expect(seededCascade && seededCascade.get().size() == 2 &&
                   seededCascade.get().last().data.phonemes.offsetSeq.edited ==
                       secondCascade.offsetSeq.edited,
               QStringLiteral("public phoneme reset second cascade fixture must remain valid"));

        const auto resetBase = runtime.documentVersion();
        const auto resetCascade = registry.invoke(
            QStringLiteral("notes.reset_phoneme_offsets"),
            mergeCommandArguments(
                resetBase,
                QJsonObject{
                    {QStringLiteral("clip_id"),  fixture.noteClipId.value()                 },
                    {QStringLiteral("note_ids"), QJsonArray{fixture.noteIds.first().value()}},
        }),
            {.clientId = QStringLiteral("reset-phoneme-offset-cascade")});
        const auto afterCascade =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        const auto undoCascade = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("reset-phoneme-offset-cascade-undo")});
        const auto afterCascadeUndo =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        expect(resetCascade && resetCascade.get().value(QStringLiteral("changed")).toBool(),
               QStringLiteral("notes.reset_phoneme_offsets must run without GUI confirmation"));
        expect(resetCascade &&
                   resetCascade.get().value(QStringLiteral("affected_objects")).toArray().size() ==
                       2,
               QStringLiteral("notes.reset_phoneme_offsets must report the complete cascade"));
        expect(afterCascade && afterCascade.get().size() == 2 &&
                   afterCascade.get().first().data.phonemes.offsetSeq.edited.isEmpty() &&
                   afterCascade.get().last().data.phonemes.offsetSeq.edited.isEmpty(),
               QStringLiteral("notes.reset_phoneme_offsets must reset the complete cascade"));
        expect(undoCascade && afterCascadeUndo &&
                   afterCascadeUndo.get().first().data.phonemes.offsetSeq.edited ==
                       firstCascade.offsetSeq.edited &&
                   afterCascadeUndo.get().last().data.phonemes.offsetSeq.edited ==
                       secondCascade.offsetSeq.edited &&
                   runtime.documentVersion().revision == resetBase.revision + 2,
               QStringLiteral("notes.reset_phoneme_offsets must undo the complete cascade once"));
    }

    void verifyPublicVoiceAndSpeakerMix(
        Automation::PublicAutomationRegistry &registry, Automation::CoreRuntime &runtime,
        const PublicEditingFixture &fixture, const SingerInfo &singer, const SpeakerInfo &speakerA,
        const SpeakerInfo &speakerB, const SingerInfo &speakerlessSinger,
        const SingerInfo &sameIdNewerSinger, const SpeakerInfo &sameIdNewerSpeaker,
        const SpeakerInfo &sameIdNewerSpeakerB) {
        const auto selectedA = voiceSelection(singer, speakerA);
        const auto selectedB = voiceSelection(singer, speakerB);
        const auto speakerId = [](const QJsonValue &voice) {
            return voice.toObject()
                .value(QStringLiteral("speaker"))
                .toObject()
                .value(QStringLiteral("speaker_id"))
                .toString();
        };

        const auto voices = invokeSchemaValid(registry, QStringLiteral("voices.list"), {},
                                              QStringLiteral("versioned voices.list"));
        QSet<QString> matchingVersions;
        if (voices) {
            for (const auto &value : voices->value(QStringLiteral("singers")).toArray()) {
                const auto candidate = value.toObject();
                if (candidate.value(QStringLiteral("package_id")) == singer.packageId() &&
                    candidate.value(QStringLiteral("singer_id")) == singer.singerId()) {
                    matchingVersions.insert(
                        candidate.value(QStringLiteral("package_version")).toString());
                }
            }
        }
        expect(matchingVersions == QSet<QString>{singer.packageVersion().toString(),
                                                 sameIdNewerSinger.packageVersion().toString()},
               QStringLiteral("voices.list must expose complete references for same-ID versions"));
        const auto newerVoiceDescription = invokeSchemaValid(
            registry, QStringLiteral("voices.describe"),
            QJsonObject{
                {QStringLiteral("singer"), voiceSelection(sameIdNewerSinger, sameIdNewerSpeaker)
                                               .value(QStringLiteral("singer"))
                                               .toObject()}
        },
            QStringLiteral("versioned voices.describe"));
        const auto newerVoiceSnapshot =
            newerVoiceDescription
                ? newerVoiceDescription->value(QStringLiteral("snapshot")).toObject()
                : QJsonObject{};
        expect(newerVoiceDescription &&
                   newerVoiceSnapshot.value(QStringLiteral("package_version")).toString() ==
                       sameIdNewerSinger.packageVersion().toString() &&
                   newerVoiceSnapshot.value(QStringLiteral("speakers"))
                           .toArray()
                           .first()
                           .toObject()
                           .value(QStringLiteral("speaker_id")) == sameIdNewerSpeaker.id(),
               QStringLiteral("voices.describe must resolve the requested package version"));

        invokeChangedOnce(
            registry, runtime, QStringLiteral("tracks.set_voice"),
            QJsonObject{
                {QStringLiteral("track_id"), fixture.trackId.value()},
                {QStringLiteral("voice"), voiceSelection(sameIdNewerSinger, sameIdNewerSpeaker)},
        },
            QStringLiteral("track-set-newer-voice"),
            QStringLiteral("tracks.set_voice with an exact package version"));
        const auto newerTrackVoice =
            voiceContextSnapshot(registry, runtime, QStringLiteral("tracks.get"),
                                 QStringLiteral("track_id"), fixture.trackId.value());
        const auto newerSingerRef = newerTrackVoice.value(QStringLiteral("own_voice"))
                                        .toObject()
                                        .value(QStringLiteral("singer"))
                                        .toObject();
        expect(newerSingerRef.value(QStringLiteral("package_version")).toString() ==
                       sameIdNewerSinger.packageVersion().toString() &&
                   speakerId(newerTrackVoice.value(QStringLiteral("own_voice"))) ==
                       sameIdNewerSpeaker.id(),
               QStringLiteral("voice selection must distinguish packages with identical IDs by "
                              "package version"));
        const QJsonObject newerMix{
            {QStringLiteral("singer"),  newerSingerRef},
            {QStringLiteral("sources"),
             QJsonArray{
                 QJsonObject{
                     {QStringLiteral("speaker"),
                      QJsonObject{{QStringLiteral("speaker_id"), sameIdNewerSpeaker.id()}}},
                     {QStringLiteral("weight"), 0.25},
                 },
                 QJsonObject{
                     {QStringLiteral("speaker"),
                      QJsonObject{{QStringLiteral("speaker_id"), sameIdNewerSpeakerB.id()}}},
                     {QStringLiteral("weight"), 0.75},
                 },
             }                                        },
        };
        invokeChangedOnce(registry, runtime, QStringLiteral("speaker_mix.set_fixed"),
                          QJsonObject{
                              {QStringLiteral("target"),
                               QJsonObject{
                                   {QStringLiteral("type"), QStringLiteral("track")},
                                   {QStringLiteral("id"), fixture.trackId.value()},
                               }                                 },
                              {QStringLiteral("mix"),    newerMix},
        },
                          QStringLiteral("track-set-newer-mix"),
                          QStringLiteral("speaker_mix.set_fixed with an exact package version"));
        const auto newerTrackMix =
            speakerMixSnapshot(registry, runtime, QStringLiteral("track"), fixture.trackId.value());
        const auto newerMixSnapshot = newerTrackMix.value(QStringLiteral("mix")).toObject();
        expect(newerMixSnapshot.value(QStringLiteral("singer"))
                           .toObject()
                           .value(QStringLiteral("package_version")) ==
                       sameIdNewerSinger.packageVersion().toString() &&
                   newerMixSnapshot.value(QStringLiteral("sources"))
                           .toArray()
                           .first()
                           .toObject()
                           .value(QStringLiteral("speaker"))
                           .toObject()
                           .value(QStringLiteral("speaker_id")) == sameIdNewerSpeaker.id(),
               QStringLiteral("Speaker Mix must resolve and return the requested package version"));

        invokeChangedOnce(registry, runtime, QStringLiteral("tracks.set_voice"),
                          QJsonObject{
                              {QStringLiteral("track_id"), fixture.trackId.value()},
                              {QStringLiteral("voice"),    selectedA              },
        },
                          QStringLiteral("track-set-voice"), QStringLiteral("tracks.set_voice"));
        const auto trackVoice =
            voiceContextSnapshot(registry, runtime, QStringLiteral("tracks.get"),
                                 QStringLiteral("track_id"), fixture.trackId.value());
        expect(trackVoice.value(QStringLiteral("available")).toBool() &&
                   !trackVoice.value(QStringLiteral("inherits_track")).toBool() &&
                   speakerId(trackVoice.value(QStringLiteral("own_voice"))) == speakerA.id() &&
                   speakerId(trackVoice.value(QStringLiteral("effective_voice"))) == speakerA.id(),
               QStringLiteral("track voice selection must be immediately queryable"));

        auto noteLanguage = commandArguments(runtime.documentVersion());
        noteLanguage.insert(QStringLiteral("clip_id"), fixture.noteClipId.value());
        noteLanguage.insert(QStringLiteral("note_ids"),
                            QJsonArray{fixture.noteIds.first().value()});
        noteLanguage.insert(QStringLiteral("language"),
                            QJsonObject{
                                {QStringLiteral("mode"),        QStringLiteral("explicit")},
                                {QStringLiteral("language_id"), QStringLiteral("zh")      },
        });
        const auto noteLanguageBase = runtime.documentVersion();
        const auto setNoteLanguage =
            registry.invoke(QStringLiteral("notes.set_language"), noteLanguage,
                            {.clientId = QStringLiteral("note-language-explicit-with-voice")});
        const auto explicitLanguageNotes =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        const auto undoNoteLanguage = registry.invoke(
            QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
            {.clientId = QStringLiteral("note-language-explicit-undo")});
        const auto restoredLanguageNotes =
            runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.noteClipId);
        expect(setNoteLanguage && explicitLanguageNotes &&
                   explicitLanguageNotes.get().first().data.language == QStringLiteral("zh") &&
                   undoNoteLanguage && restoredLanguageNotes &&
                   restoredLanguageNotes.get().first().data.language.isEmpty() &&
                   runtime.documentVersion().revision == noteLanguageBase.revision + 2,
               QStringLiteral("notes.set_language must validate against the effective voice and "
                              "remain one independently undoable edit"));

        const auto fillLanguageOptions = registry.invoke(
            QStringLiteral("automation.get_options"),
            QJsonObject{
                {QStringLiteral("operation_id"),      QStringLiteral("notes.fill_lyrics")            },
                {QStringLiteral("field_path"),        QStringLiteral("/options/language/language_id")},
                {QStringLiteral("partial_arguments"),
                 QJsonObject{
                     {QStringLiteral("document_id"),
                      runtime.documentVersion().documentId.toString()},
                     {QStringLiteral("clip_id"), fixture.noteClipId.value()},
                 }                                                                                   },
        });
        QSet<QString> fillLanguages;
        if (fillLanguageOptions) {
            for (const auto &value :
                 fillLanguageOptions.get().value(QStringLiteral("options")).toArray()) {
                fillLanguages.insert(value.toObject().value(QStringLiteral("value")).toString());
            }
        }
        auto invalidFillLanguage = commandArguments(runtime.documentVersion());
        invalidFillLanguage.insert(QStringLiteral("clip_id"), fixture.noteClipId.value());
        invalidFillLanguage.insert(QStringLiteral("note_ids"),
                                   QJsonArray{fixture.noteIds.first().value()});
        invalidFillLanguage.insert(QStringLiteral("text"), QStringLiteral("X"));
        invalidFillLanguage.insert(
            QStringLiteral("options"),
            QJsonObject{
                {QStringLiteral("language"),
                 QJsonObject{
                     {QStringLiteral("mode"), QStringLiteral("explicit")},
                     {QStringLiteral("language_id"), QStringLiteral("missing")},
                 }},
        });
        const auto fillLanguageBase = runtime.documentVersion();
        const auto rejectedFillLanguage =
            registry.invoke(QStringLiteral("notes.fill_lyrics"), invalidFillLanguage,
                            {.clientId = QStringLiteral("fill-language-validation")});
        expect(fillLanguageOptions &&
                   fillLanguages == QSet<QString>{QStringLiteral("en"), QStringLiteral("zh")} &&
                   !rejectedFillLanguage &&
                   rejectedFillLanguage.getError().code ==
                       Automation::AutomationErrorCode::InvalidArgument &&
                   rejectedFillLanguage.getError().fieldPath ==
                       QStringLiteral("/options/language/language_id") &&
                   runtime.documentVersion() == fillLanguageBase,
               QStringLiteral(
                   "fill-lyrics explicit language must be discovered and capability-validated"));
        verifyScalarEdit(
            registry, runtime, QStringLiteral("tracks.set_default_language"),
            QJsonObject{
                {QStringLiteral("track_id"), fixture.trackId.value()}
        },
            QStringLiteral("language_id"), QStringLiteral("zh"),
            QStringLiteral("default_language_id"),
            [&] { return publicTrackSnapshot(registry, runtime, fixture.trackId); },
            QStringLiteral("track default language"));

        invokeChangedOnce(registry, runtime, QStringLiteral("clips.set_voice"),
                          QJsonObject{
                              {QStringLiteral("clip_id"), fixture.mixClipId.value()},
                              {QStringLiteral("voice"),   selectedB                },
        },
                          QStringLiteral("clip-set-voice"), QStringLiteral("clips.set_voice"));
        const auto ownedClip =
            voiceContextSnapshot(registry, runtime, QStringLiteral("clips.get"),
                                 QStringLiteral("clip_id"), fixture.mixClipId.value());
        expect(ownedClip.value(QStringLiteral("available")).toBool() &&
                   !ownedClip.value(QStringLiteral("inherits_track")).toBool() &&
                   speakerId(ownedClip.value(QStringLiteral("own_voice"))) == speakerB.id() &&
                   speakerId(ownedClip.value(QStringLiteral("effective_voice"))) == speakerB.id(),
               QStringLiteral("clip voice selection must establish an owned voice context"));
        verifyScalarEdit(
            registry, runtime, QStringLiteral("clips.set_default_language"),
            QJsonObject{
                {QStringLiteral("clip_id"), fixture.mixClipId.value()}
        },
            QStringLiteral("language_id"), QStringLiteral("zh"),
            QStringLiteral("default_language_id"),
            [&] { return publicClipSnapshot(registry, runtime, fixture.mixClipId); },
            QStringLiteral("clip default language"));

        invokeChangedOnce(registry, runtime, QStringLiteral("clips.use_track_voice"),
                          QJsonObject{
                              {QStringLiteral("clip_id"), fixture.mixClipId.value()}
        },
                          QStringLiteral("clip-use-track-voice"),
                          QStringLiteral("clips.use_track_voice"));
        const auto inheritedClip =
            voiceContextSnapshot(registry, runtime, QStringLiteral("clips.get"),
                                 QStringLiteral("clip_id"), fixture.mixClipId.value());
        expect(inheritedClip.value(QStringLiteral("available")).toBool() &&
                   inheritedClip.value(QStringLiteral("inherits_track")).toBool() &&
                   speakerId(inheritedClip.value(QStringLiteral("own_voice"))) == speakerB.id() &&
                   speakerId(inheritedClip.value(QStringLiteral("effective_voice"))) ==
                       speakerA.id(),
               QStringLiteral(
                   "clip track inheritance must preserve own voice and expose track voice"));
        const auto undoInheritanceBase = runtime.documentVersion();
        const auto undoInheritance =
            registry.invoke(QStringLiteral("history.undo"), commandArguments(undoInheritanceBase),
                            {.clientId = QStringLiteral("clip-use-track-voice-undo")});
        const auto restoredOwnedClip =
            voiceContextSnapshot(registry, runtime, QStringLiteral("clips.get"),
                                 QStringLiteral("clip_id"), fixture.mixClipId.value());
        expect(undoInheritance && undoInheritance.get().value(QStringLiteral("changed")).toBool() &&
                   runtime.documentVersion().revision == undoInheritanceBase.revision + 1 &&
                   !restoredOwnedClip.value(QStringLiteral("inherits_track")).toBool() &&
                   speakerId(restoredOwnedClip.value(QStringLiteral("effective_voice"))) ==
                       speakerB.id(),
               QStringLiteral("clips.use_track_voice must be one independently undoable edit"));

        invokeChangedOnce(registry, runtime, QStringLiteral("clips.clear_voice"),
                          QJsonObject{
                              {QStringLiteral("clip_id"), fixture.mixClipId.value()}
        },
                          QStringLiteral("clip-clear-voice"), QStringLiteral("clips.clear_voice"));
        const auto clearedClip =
            voiceContextSnapshot(registry, runtime, QStringLiteral("clips.get"),
                                 QStringLiteral("clip_id"), fixture.mixClipId.value());
        expect(!clearedClip.value(QStringLiteral("available")).toBool() &&
                   !clearedClip.value(QStringLiteral("inherits_track")).toBool() &&
                   clearedClip.value(QStringLiteral("own_voice")).isNull() &&
                   clearedClip.value(QStringLiteral("effective_voice")).isNull(),
               QStringLiteral("clips.clear_voice must leave an explicit empty clip context"));
        const auto undoClearClipBase = runtime.documentVersion();
        const auto undoClearClip =
            registry.invoke(QStringLiteral("history.undo"), commandArguments(undoClearClipBase),
                            {.clientId = QStringLiteral("clip-clear-voice-undo")});
        const auto restoredAfterClear =
            voiceContextSnapshot(registry, runtime, QStringLiteral("clips.get"),
                                 QStringLiteral("clip_id"), fixture.mixClipId.value());
        expect(undoClearClip && undoClearClip.get().value(QStringLiteral("changed")).toBool() &&
                   runtime.documentVersion().revision == undoClearClipBase.revision + 1 &&
                   speakerId(restoredAfterClear.value(QStringLiteral("effective_voice"))) ==
                       speakerB.id(),
               QStringLiteral("clips.clear_voice must restore the owned voice in one undo"));

        const auto mix = fixedMix(singer, speakerA, speakerB);
        invokeChangedOnce(registry, runtime, QStringLiteral("speaker_mix.set_fixed"),
                          QJsonObject{
                              {QStringLiteral("target"),
                               QJsonObject{
                                   {QStringLiteral("type"), QStringLiteral("track")},
                                   {QStringLiteral("id"), fixture.trackId.value()},
                               }                            },
                              {QStringLiteral("mix"),    mix},
        },
                          QStringLiteral("track-fixed-mix"),
                          QStringLiteral("track speaker_mix.set_fixed"));
        const auto fixedTrack =
            speakerMixSnapshot(registry, runtime, QStringLiteral("track"), fixture.trackId.value());
        expect(!fixedTrack.value(QStringLiteral("dynamic_enabled")).toBool() &&
                   fixedTrack.value(QStringLiteral("mix"))
                           .toObject()
                           .value(QStringLiteral("sources"))
                           .toArray()
                           .size() == 2,
               QStringLiteral("track fixed Speaker Mix must round-trip both sources"));

        invokeChangedOnce(registry, runtime, QStringLiteral("speaker_mix.set_fixed"),
                          QJsonObject{
                              {QStringLiteral("target"),
                               QJsonObject{
                                   {QStringLiteral("type"), QStringLiteral("clip")},
                                   {QStringLiteral("id"), fixture.mixClipId.value()},
                               }                            },
                              {QStringLiteral("mix"),    mix},
        },
                          QStringLiteral("clip-fixed-mix"),
                          QStringLiteral("clip speaker_mix.set_fixed"));
        const auto fixedClip = speakerMixSnapshot(registry, runtime, QStringLiteral("clip"),
                                                  fixture.mixClipId.value());
        expect(!fixedClip.value(QStringLiteral("dynamic_enabled")).toBool() &&
                   fixedClip.value(QStringLiteral("mix"))
                           .toObject()
                           .value(QStringLiteral("sources"))
                           .toArray()
                           .size() == 2,
               QStringLiteral("clip fixed Speaker Mix must round-trip both sources"));

        auto presetDraft = mix;
        presetDraft.insert(QStringLiteral("name"), QStringLiteral("Registry Mix"));
        const auto beforePresetSave = runtime.documentVersion();
        const auto savedPreset =
            registry.invoke(QStringLiteral("speaker_mix.presets.save"),
                            QJsonObject{
                                {QStringLiteral("preset"), presetDraft}
        },
                            {.clientId = QStringLiteral("speaker-mix-preset-save")});
        reportFailure(QStringLiteral("speaker_mix.presets.save"), savedPreset);
        const auto savedPresetSnapshot =
            savedPreset ? savedPreset.get().value(QStringLiteral("preset")).toObject()
                        : QJsonObject{};
        const auto presetId = savedPresetSnapshot.value(QStringLiteral("preset_id")).toString();
        const auto listedPresets =
            registry.invoke(QStringLiteral("speaker_mix.presets.list"),
                            QJsonObject{
                                {QStringLiteral("singer"), mix.value(QStringLiteral("singer"))}
        },
                            {.clientId = QStringLiteral("speaker-mix-preset-list")});
        const auto newerVersionPresets =
            registry.invoke(QStringLiteral("speaker_mix.presets.list"),
                            QJsonObject{
                                {QStringLiteral("singer"), newerSingerRef}
        },
                            {.clientId = QStringLiteral("speaker-mix-preset-list-newer-version")});
        expect(
            savedPreset && !presetId.isEmpty() && listedPresets &&
                listedPresets.get().value(QStringLiteral("presets")).toArray().size() == 1 &&
                savedPresetSnapshot.value(QStringLiteral("singer"))
                        .toObject()
                        .value(QStringLiteral("package_version")) ==
                    singer.packageVersion().toString() &&
                !savedPresetSnapshot.contains(QStringLiteral("package_version")) &&
                newerVersionPresets &&
                newerVersionPresets.get().value(QStringLiteral("presets")).toArray().isEmpty() &&
                runtime.documentVersion() == beforePresetSave,
            QStringLiteral("Speaker Mix presets must preserve and filter the nested singer version "
                           "without changing a document"));

        const auto appliedPreset =
            invokeChangedOnce(registry, runtime, QStringLiteral("speaker_mix.presets.apply"),
                              QJsonObject{
                                  {QStringLiteral("target"),
                                   QJsonObject{
                                       {QStringLiteral("type"), QStringLiteral("clip")},
                                       {QStringLiteral("id"), fixture.mixClipId.value()},
                                   }                                    },
                                  {QStringLiteral("preset_id"), presetId},
        },
                              QStringLiteral("speaker-mix-preset-apply"),
                              QStringLiteral("speaker_mix.presets.apply"));
        const auto appliedPresetMix = speakerMixSnapshot(registry, runtime, QStringLiteral("clip"),
                                                         fixture.mixClipId.value());
        const auto sourcePreset =
            appliedPresetMix.value(QStringLiteral("source_preset")).toObject();
        expect(appliedPreset &&
                   sourcePreset.value(QStringLiteral("preset_id")).toString() == presetId &&
                   sourcePreset.value(QStringLiteral("name")).toString() ==
                       QStringLiteral("Registry Mix") &&
                   !sourcePreset.value(QStringLiteral("dirty")).toBool(),
               QStringLiteral("applying a Speaker Mix preset must expose clean source metadata"));

        auto renamedPresetDraft = presetDraft;
        renamedPresetDraft.insert(QStringLiteral("preset_id"), presetId);
        renamedPresetDraft.insert(QStringLiteral("name"), QStringLiteral("Registry Mix Renamed"));
        const auto beforePresetMaintenance = runtime.documentVersion();
        const auto renamedPreset =
            registry.invoke(QStringLiteral("speaker_mix.presets.save"),
                            QJsonObject{
                                {QStringLiteral("preset"), renamedPresetDraft}
        },
                            {.clientId = QStringLiteral("speaker-mix-preset-rename")});
        const auto deletedPreset =
            registry.invoke(QStringLiteral("speaker_mix.presets.delete"),
                            QJsonObject{
                                {QStringLiteral("preset_id"), presetId}
        },
                            {.clientId = QStringLiteral("speaker-mix-preset-delete")});
        const auto presetsAfterDelete =
            registry.invoke(QStringLiteral("speaker_mix.presets.list"), {});
        expect(renamedPreset &&
                   renamedPreset.get()
                           .value(QStringLiteral("preset"))
                           .toObject()
                           .value(QStringLiteral("name"))
                           .toString() == QStringLiteral("Registry Mix Renamed") &&
                   deletedPreset && deletedPreset.get().value(QStringLiteral("changed")).toBool() &&
                   presetsAfterDelete &&
                   presetsAfterDelete.get().value(QStringLiteral("presets")).toArray().isEmpty() &&
                   runtime.documentVersion() == beforePresetMaintenance,
               QStringLiteral("Speaker Mix preset update and delete must stay application-scoped"));

        invokeChangedOnce(registry, runtime, QStringLiteral("speaker_mix.enable_dynamic"),
                          QJsonObject{
                              {QStringLiteral("clip_id"), fixture.mixClipId.value()}
        },
                          QStringLiteral("clip-enable-dynamic"),
                          QStringLiteral("speaker_mix.enable_dynamic"));
        const auto enabledDynamic = speakerMixSnapshot(registry, runtime, QStringLiteral("clip"),
                                                       fixture.mixClipId.value());
        const auto dynamicPresetDirty = enabledDynamic.value(QStringLiteral("source_preset"))
                                            .toObject()
                                            .value(QStringLiteral("dirty"))
                                            .toBool();
        const auto validDynamicPreset =
            enabledDynamic.value(QStringLiteral("dynamic_enabled")).toBool() &&
            enabledDynamic.value(QStringLiteral("keyframes")).toArray().size() == 1 &&
            !speakerMixKeyframeAt(enabledDynamic, 0).isEmpty() && dynamicPresetDirty;
        if (!validDynamicPreset) {
            QTextStream(stderr) << "DETAIL speaker_mix.enable_dynamic snapshot: "
                                << QJsonDocument(enabledDynamic).toJson(QJsonDocument::Compact)
                                << Qt::endl;
        }
        expect(validDynamicPreset,
               QStringLiteral("dynamic Speaker Mix must create the immutable initial keyframe and "
                              "mark its preset source dirty"));

        invokeChangedOnce(registry, runtime, QStringLiteral("speaker_mix.keyframes.insert"),
                          QJsonObject{
                              {QStringLiteral("clip_id"),  fixture.mixClipId.value()},
                              {QStringLiteral("position"), 480                      },
        },
                          QStringLiteral("mix-keyframe-insert-interpolated"),
                          QStringLiteral("keyframe insert without weights"));
        invokeChangedOnce(registry, runtime, QStringLiteral("speaker_mix.keyframes.insert"),
                          QJsonObject{
                              {QStringLiteral("clip_id"),  fixture.mixClipId.value()},
                              {QStringLiteral("position"), 960                      },
                              {QStringLiteral("weights"),  QJsonArray{0.4, 0.6}     },
        },
                          QStringLiteral("mix-keyframe-insert-explicit"),
                          QStringLiteral("keyframe insert with explicit weights"));
        const auto insertedKeyframes = speakerMixSnapshot(registry, runtime, QStringLiteral("clip"),
                                                          fixture.mixClipId.value());
        const auto keyframe480 = speakerMixKeyframeAt(insertedKeyframes, 480);
        const auto keyframe960 = speakerMixKeyframeAt(insertedKeyframes, 960);
        const auto keyframe480Id = keyframe480.value(QStringLiteral("keyframe_id")).toInt(-1);
        const auto keyframe960Id = keyframe960.value(QStringLiteral("keyframe_id")).toInt(-1);
        expect(keyframe480Id > 0 && keyframe960Id > 0 &&
                   keyframe480.value(QStringLiteral("weights")).toArray().size() == 2 &&
                   keyframe960.value(QStringLiteral("weights")).toArray().size() == 2,
               QStringLiteral("Speaker Mix keyframes must expose stable IDs and full weights"));
        if (keyframe480Id <= 0 || keyframe960Id <= 0)
            return;

        invokeChangedOnce(registry, runtime, QStringLiteral("speaker_mix.keyframes.move"),
                          QJsonObject{
                              {QStringLiteral("clip_id"), fixture.mixClipId.value()},
                              {QStringLiteral("moves"),
                               QJsonArray{
                                   QJsonObject{
                                       {QStringLiteral("keyframe_id"), keyframe480Id},
                                       {QStringLiteral("position"), 600},
                                   },
                                   QJsonObject{
                                       {QStringLiteral("keyframe_id"), keyframe960Id},
                                       {QStringLiteral("position"), 1200},
                                   },
                               }                                                   },
        },
                          QStringLiteral("mix-keyframes-move"),
                          QStringLiteral("speaker_mix.keyframes.move batch"));
        const auto movedKeyframes = speakerMixSnapshot(registry, runtime, QStringLiteral("clip"),
                                                       fixture.mixClipId.value());
        const auto undoMoveBase = runtime.documentVersion();
        const auto undoMove =
            registry.invoke(QStringLiteral("history.undo"), commandArguments(undoMoveBase),
                            {.clientId = QStringLiteral("mix-keyframes-move-undo")});
        const auto restoredKeyframes = speakerMixSnapshot(registry, runtime, QStringLiteral("clip"),
                                                          fixture.mixClipId.value());
        expect(!speakerMixKeyframeAt(movedKeyframes, 600).isEmpty() &&
                   !speakerMixKeyframeAt(movedKeyframes, 1200).isEmpty() && undoMove &&
                   undoMove.get().value(QStringLiteral("changed")).toBool() &&
                   runtime.documentVersion().revision == undoMoveBase.revision + 1 &&
                   !speakerMixKeyframeAt(restoredKeyframes, 480).isEmpty() &&
                   !speakerMixKeyframeAt(restoredKeyframes, 960).isEmpty(),
               QStringLiteral("keyframe batch move must restore both keys with one undo"));

        invokeChangedOnce(registry, runtime, QStringLiteral("speaker_mix.keyframes.set_weights"),
                          QJsonObject{
                              {QStringLiteral("clip_id"),     fixture.mixClipId.value()},
                              {QStringLiteral("keyframe_id"), keyframe480Id            },
                              {QStringLiteral("weights"),     QJsonArray{0.3, 0.7}     },
        },
                          QStringLiteral("mix-keyframe-set-weights"),
                          QStringLiteral("speaker_mix.keyframes.set_weights"));
        const auto weightedKeyframes = speakerMixSnapshot(registry, runtime, QStringLiteral("clip"),
                                                          fixture.mixClipId.value());
        const auto roundTrippedWeights =
            speakerMixKeyframeAt(weightedKeyframes, 480).value(QStringLiteral("weights")).toArray();
        expect(roundTrippedWeights.size() == 2 && roundTrippedWeights.at(0).toDouble() > 0.29 &&
                   roundTrippedWeights.at(0).toDouble() < 0.31 &&
                   roundTrippedWeights.at(1).toDouble() > 0.69 &&
                   roundTrippedWeights.at(1).toDouble() < 0.71,
               QStringLiteral("Speaker Mix keyframe weights must round-trip"));

        invokeChangedOnce(
            registry, runtime, QStringLiteral("speaker_mix.keyframes.remove"),
            QJsonObject{
                {QStringLiteral("clip_id"),      fixture.mixClipId.value()               },
                {QStringLiteral("keyframe_ids"), QJsonArray{keyframe480Id, keyframe960Id}},
        },
            QStringLiteral("mix-keyframes-remove"),
            QStringLiteral("speaker_mix.keyframes.remove batch"));
        const auto removedKeyframes = speakerMixSnapshot(registry, runtime, QStringLiteral("clip"),
                                                         fixture.mixClipId.value());
        const auto undoRemoveBase = runtime.documentVersion();
        const auto undoRemove =
            registry.invoke(QStringLiteral("history.undo"), commandArguments(undoRemoveBase),
                            {.clientId = QStringLiteral("mix-keyframes-remove-undo")});
        const auto restoredRemovedKeyframes = speakerMixSnapshot(
            registry, runtime, QStringLiteral("clip"), fixture.mixClipId.value());
        expect(removedKeyframes.value(QStringLiteral("keyframes")).toArray().size() == 1 &&
                   undoRemove && undoRemove.get().value(QStringLiteral("changed")).toBool() &&
                   runtime.documentVersion().revision == undoRemoveBase.revision + 1 &&
                   restoredRemovedKeyframes.value(QStringLiteral("keyframes")).toArray().size() ==
                       3,
               QStringLiteral("keyframe batch remove must restore both keys with one undo"));

        invokeChangedOnce(registry, runtime, QStringLiteral("speaker_mix.set_dynamic_bypass"),
                          QJsonObject{
                              {QStringLiteral("clip_id"),  fixture.mixClipId.value()},
                              {QStringLiteral("bypassed"), true                     },
        },
                          QStringLiteral("mix-dynamic-bypass"),
                          QStringLiteral("speaker_mix.set_dynamic_bypass"));
        const auto bypassed = speakerMixSnapshot(registry, runtime, QStringLiteral("clip"),
                                                 fixture.mixClipId.value());
        expect(bypassed.value(QStringLiteral("dynamic_enabled")).toBool() &&
                   bypassed.value(QStringLiteral("bypassed")).toBool(),
               QStringLiteral("dynamic Speaker Mix bypass must round-trip"));

        invokeChangedOnce(registry, runtime, QStringLiteral("speaker_mix.disable_dynamic"),
                          QJsonObject{
                              {QStringLiteral("clip_id"), fixture.mixClipId.value()}
        },
                          QStringLiteral("clip-disable-dynamic"),
                          QStringLiteral("speaker_mix.disable_dynamic"));
        const auto disabledDynamic = speakerMixSnapshot(registry, runtime, QStringLiteral("clip"),
                                                        fixture.mixClipId.value());
        expect(!disabledDynamic.value(QStringLiteral("dynamic_enabled")).toBool() &&
                   !disabledDynamic.value(QStringLiteral("bypassed")).toBool() &&
                   disabledDynamic.value(QStringLiteral("keyframes")).toArray().isEmpty(),
               QStringLiteral("disabling dynamic Speaker Mix must retain a fixed mix"));

        invokeChangedOnce(registry, runtime, QStringLiteral("tracks.clear_voice"),
                          QJsonObject{
                              {QStringLiteral("track_id"), fixture.trackId.value()}
        },
                          QStringLiteral("track-clear-voice"),
                          QStringLiteral("tracks.clear_voice"));
        const auto clearedTrack =
            voiceContextSnapshot(registry, runtime, QStringLiteral("tracks.get"),
                                 QStringLiteral("track_id"), fixture.trackId.value());
        expect(!clearedTrack.value(QStringLiteral("available")).toBool() &&
                   clearedTrack.value(QStringLiteral("own_voice")).isNull(),
               QStringLiteral("tracks.clear_voice must clear the complete track context"));
        const auto undoTrackClearBase = runtime.documentVersion();
        const auto undoTrackClear =
            registry.invoke(QStringLiteral("history.undo"), commandArguments(undoTrackClearBase),
                            {.clientId = QStringLiteral("track-clear-voice-undo")});
        const auto restoredTrack =
            voiceContextSnapshot(registry, runtime, QStringLiteral("tracks.get"),
                                 QStringLiteral("track_id"), fixture.trackId.value());
        expect(undoTrackClear && undoTrackClear.get().value(QStringLiteral("changed")).toBool() &&
                   runtime.documentVersion().revision == undoTrackClearBase.revision + 1 &&
                   restoredTrack.value(QStringLiteral("available")).toBool(),
               QStringLiteral("tracks.clear_voice must restore the complete context in one undo"));

        const QJsonObject speakerlessSelection{
            {QStringLiteral("singer"),
             QJsonObject{
                 {QStringLiteral("package_id"), speakerlessSinger.packageId()},
                 {QStringLiteral("package_version"), speakerlessSinger.packageVersion().toString()},
                 {QStringLiteral("singer_id"), speakerlessSinger.singerId()},
             }},
        };
        const auto speakerlessOptions = registry.invoke(
            QStringLiteral("automation.get_options"),
            QJsonObject{
                {QStringLiteral("operation_id"),      QStringLiteral("tracks.set_voice")},
                {QStringLiteral("field_path"),        QStringLiteral("/voice/speaker")  },
                {QStringLiteral("partial_arguments"),
                 QJsonObject{{QStringLiteral("voice"), speakerlessSelection}}           },
        },
            {.clientId = QStringLiteral("speakerless-options")});
        invokeChangedOnce(registry, runtime, QStringLiteral("tracks.set_voice"),
                          QJsonObject{
                              {QStringLiteral("track_id"), fixture.trackId.value()},
                              {QStringLiteral("voice"),    speakerlessSelection   },
        },
                          QStringLiteral("track-set-speakerless-voice"),
                          QStringLiteral("tracks.set_voice without speaker"));
        const auto speakerlessContext =
            voiceContextSnapshot(registry, runtime, QStringLiteral("tracks.get"),
                                 QStringLiteral("track_id"), fixture.trackId.value());
        const auto effectiveVoice =
            speakerlessContext.value(QStringLiteral("effective_voice")).toObject();
        expect(
            speakerlessOptions &&
                speakerlessOptions.get().value(QStringLiteral("options")).toArray().isEmpty() &&
                speakerlessContext.value(QStringLiteral("available")).toBool() &&
                effectiveVoice.value(QStringLiteral("speaker")).isNull() &&
                effectiveVoice.value(QStringLiteral("singer"))
                        .toObject()
                        .value(QStringLiteral("singer_id")) == speakerlessSinger.singerId(),
            QStringLiteral("speakerless voices must be discoverable, assignable, and queryable"));
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    Automation::DocumentRuntimeServices documentServices;
    documentServices.saveProject = [](const QString &, AppModel *, QString &) { return true; };
    auto capturedAudioConfig = std::make_shared<Automation::AudioExportConfigDto>();
    Automation::AudioExportRuntimeServices audioExportServices;
    audioExportServices.createJob =
        [capturedAudioConfig](AppModel *, const QString &,
                              const Automation::AudioExportConfigDto &config)
        -> Automation::AutomationResult<std::shared_ptr<Automation::IAudioExportJob>> {
        *capturedAudioConfig = config;
        return std::shared_ptr<Automation::IAudioExportJob>(
            std::make_shared<PreviewAudioExportJob>(config));
    };
    Automation::FileRuntimeServices fileServices;
    fileServices.listProjectFormats = [] {
        const auto *midiExport =
            AutomationWire::findPublicTool(QStringLiteral("exports.midi.start"));
        const auto exportOptions = midiExport->inputSchema.value(QStringLiteral("properties"))
                                       .toObject()
                                       .value(QStringLiteral("options"))
                                       .toObject();
        auto root = AutomationWire::JsonSchema::oneOf(QJsonArray{
            AutomationWire::JsonSchema::object(
                {
                                      {QStringLiteral("operation_id"),
                     AutomationWire::JsonSchema::constant(QStringLiteral("documents.open"))},
                                      {QStringLiteral("options"), AutomationWire::JsonSchema::object()},
                                      },
                {QStringLiteral("operation_id"), QStringLiteral("options")}
                ),
            AutomationWire::JsonSchema::object(
                {
                                      {QStringLiteral("operation_id"),
                     AutomationWire::JsonSchema::constant(QStringLiteral("exports.midi.start"))},
                                      {QStringLiteral("options"), exportOptions},
                                      },
                {QStringLiteral("operation_id"), QStringLiteral("options")}
                ),
        });
        root.insert(QStringLiteral("type"), QStringLiteral("object"));
        return QList<Automation::ProjectFormatDto>{
            {
             .id = QStringLiteral("midi"),
             .displayName = QStringLiteral("MIDI"),
             .extensions = {QStringLiteral("mid"), QStringLiteral("midi")},
             .canOpen = true,
             .canImport = false,
             .canExport = true,
             .optionSchema = AutomationWire::JsonSchema::document(std::move(root)),
             },
        };
    };
    fileServices.exportMidi = [](AppModel *, const QString &,
                                 const Automation::MidiExportOptionsDto &,
                                 QString &) { return true; };
    auto registrySpeakerA = SpeakerInfo(QStringLiteral("speaker-a"), QStringLiteral("Speaker A"));
    auto registrySpeakerB = SpeakerInfo(QStringLiteral("speaker-b"), QStringLiteral("Speaker B"));
    auto registrySpeakerV2 =
        SpeakerInfo(QStringLiteral("speaker-v2"), QStringLiteral("Speaker V2"));
    auto registrySpeakerV2B =
        SpeakerInfo(QStringLiteral("speaker-v2-b"), QStringLiteral("Speaker V2 B"));
    registrySpeakerA.setMixable(true);
    registrySpeakerB.setMixable(true);
    registrySpeakerV2.setMixable(true);
    registrySpeakerV2B.setMixable(true);
    const SingerInfo registrySinger({QStringLiteral("registry-singer"),
                                     QStringLiteral("registry-package"), QVersionNumber(1, 0)},
                                    QStringLiteral("Registry Singer"),
                                    {registrySpeakerA, registrySpeakerB},
                                    {LanguageInfo(QStringLiteral("en"), QStringLiteral("English"),
                                                  QStringLiteral("registry-g2p")),
                                     LanguageInfo(QStringLiteral("zh"), QStringLiteral("Chinese"),
                                                  QStringLiteral("registry-g2p"))},
                                    QStringLiteral("en"));
    const SingerInfo registrySpeakerlessSinger(
        {QStringLiteral("registry-speakerless"), QStringLiteral("registry-package"),
         QVersionNumber(1, 0)},
        QStringLiteral("Registry Speakerless Singer"), {},
        {LanguageInfo(QStringLiteral("zh"), QStringLiteral("Chinese"),
                      QStringLiteral("registry-g2p"))},
        QStringLiteral("zh"));
    const SingerInfo registrySingerV2(
        {QStringLiteral("registry-singer"), QStringLiteral("registry-package"),
         QVersionNumber(2, 0)},
        QStringLiteral("Registry Singer V2"), {registrySpeakerV2, registrySpeakerV2B},
        {LanguageInfo(QStringLiteral("ja"), QStringLiteral("Japanese"),
                      QStringLiteral("registry-g2p"))},
        QStringLiteral("ja"));
    QTemporaryDir privatePackageDirectory;
    expect(privatePackageDirectory.isValid(),
           QStringLiteral("private package fixture directory must be available"));
    Automation::PackageDto registryPackage;
    registryPackage.id = QStringLiteral("registry-package");
    registryPackage.version = QVersionNumber(1, 0);
    registryPackage.vendor = QStringLiteral("Registry Vendor");
    registryPackage.path = privatePackageDirectory.path();
    registryPackage.singers.append({
        .singerId = QStringLiteral("registry-singer"),
        .packageId = registryPackage.id,
        .packageVersion = registryPackage.version,
        .name = QStringLiteral("Registry Singer"),
        .info = registrySinger,
    });
    registryPackage.singers.append({
        .singerId = QStringLiteral("registry-speakerless"),
        .packageId = registryPackage.id,
        .packageVersion = registryPackage.version,
        .name = QStringLiteral("Registry Speakerless Singer"),
        .info = registrySpeakerlessSinger,
    });
    Automation::PackageDto registryPackageV2;
    registryPackageV2.id = QStringLiteral("registry-package");
    registryPackageV2.version = QVersionNumber(2, 0);
    registryPackageV2.vendor = QStringLiteral("Registry Vendor");
    registryPackageV2.path = privatePackageDirectory.path();
    registryPackageV2.singers.append({
        .singerId = QStringLiteral("registry-singer"),
        .packageId = registryPackageV2.id,
        .packageVersion = registryPackageV2.version,
        .name = QStringLiteral("Registry Singer V2"),
        .info = registrySingerV2,
    });
    Automation::PackageRuntimeServices packageServices;
    packageServices.installedPackages = [registryPackage, registryPackageV2] {
        return QList<Automation::PackageDto>{registryPackage, registryPackageV2};
    };
    auto packageRefreshControl = std::make_shared<PackageRefreshTestControl>();
    packageRefreshControl->privatePath = privatePackageDirectory.path();
    packageServices.refreshPackages =
        [packageRefreshControl](Automation::PackageRefreshCommitGate commitGate,
                                Automation::PackageRefreshCompletion completion) {
            ++packageRefreshControl->starts;
            if (packageRefreshControl->deferNext) {
                packageRefreshControl->deferNext = false;
                packageRefreshControl->pendingCommitGate = std::move(commitGate);
                packageRefreshControl->pendingCompletion = std::move(completion);
                return Automation::AutomationResult<Automation::AutomationUnit>(
                    Automation::AutomationUnit{});
            }
            if (packageRefreshControl->failNext) {
                packageRefreshControl->failNext = false;
                Automation::AutomationError error;
                error.code = Automation::AutomationErrorCode::IoError;
                error.message =
                    QStringLiteral("Unable to scan %1").arg(packageRefreshControl->privatePath);
                completion(std::move(error));
                return Automation::AutomationResult<Automation::AutomationUnit>(
                    Automation::AutomationUnit{});
            }
            if (commitGate && !commitGate()) {
                return Automation::AutomationResult<Automation::AutomationUnit>(
                    Automation::AutomationUnit{});
            }
            completion(Automation::PackageRefreshResultDto{
                .packages = 1,
                .added = {QStringLiteral("registry-package")},
                .failures = {{packageRefreshControl->privatePath,
                              QStringLiteral("Unable to inspect %1/manifest.json")
                                  .arg(packageRefreshControl->privatePath)}},
            });
            return Automation::AutomationResult<Automation::AutomationUnit>(
                Automation::AutomationUnit{});
        };
    auto playbackState = std::make_shared<Automation::PlaybackHostSnapshot>();
    Automation::PlaybackRuntimeServices playbackServices;
    playbackServices.snapshot = [playbackState] { return *playbackState; };
    playbackServices.canStart = [] { return true; };
    playbackServices.play = [playbackState] {
        playbackState->state = Automation::PlaybackState::Playing;
        return true;
    };
    playbackServices.pause = [playbackState] {
        playbackState->state = Automation::PlaybackState::Paused;
    };
    playbackServices.stop = [playbackState] {
        playbackState->state = Automation::PlaybackState::Stopped;
    };
    playbackServices.setPosition = [playbackState](const double position) {
        playbackState->position = position;
    };
    playbackServices.setLastPosition = [playbackState](const double position) {
        playbackState->lastPosition = position;
    };
    playbackServices.setLoop = [playbackState](const LoopSettings &loop) {
        playbackState->loop = loop;
    };
    Automation::ApplicationRuntimeServices applicationServices;
    applicationServices.info = [] {
        return Automation::ApplicationInfoDto{
            .name = QStringLiteral("DS Editor Lite"),
            .version = QStringLiteral("registry-version"),
            .platform = QStringLiteral("registry-platform"),
            .buildId = QStringLiteral("registry-build-id"),
        };
    };
    auto settingsSnapshot = std::make_shared<Automation::SettingsSnapshotDto>();
    settingsSnapshot->general.uiLanguage = QStringLiteral("en_US");
    settingsSnapshot->general.defaultSingingLanguage = QStringLiteral("unknown");
    settingsSnapshot->general.defaultLyrics.insert(QStringLiteral("en"), QStringLiteral("la"));
    settingsSnapshot->appearance.themeId = QStringLiteral("light");
    settingsSnapshot->audio.driverName = QStringLiteral("test-driver");
    settingsSnapshot->audio.deviceName = QStringLiteral("test-device");
    settingsSnapshot->audio.adoptedBufferSize = 512;
    settingsSnapshot->audio.adoptedSampleRate = 44100.0;
    settingsSnapshot->inference.executionProvider = QStringLiteral("CPU");
    const QDir temporaryDirectory(QDir::tempPath());
    settingsSnapshot->general.recentProjectFiles = {
        temporaryDirectory.filePath(QStringLiteral("registry-recent-a.dspx")),
        temporaryDirectory.filePath(QStringLiteral("registry-recent-b.dspx")),
    };
    Automation::SettingsRuntimeServices settingsServices;
    settingsServices.snapshot = [settingsSnapshot] { return *settingsSnapshot; };
    settingsServices.publicSnapshot = [settingsSnapshot] {
        Automation::PublicSettingsSnapshotDto result;
        result.uiLanguage = Automation::UiLanguagePublicSettingsDto{
            .configured = settingsSnapshot->general.uiLanguage,
            .effective = settingsSnapshot->general.uiLanguage,
            .candidates =
                {
                             {QStringLiteral("en_US"), QStringLiteral("English"), true, {}},
                             {QStringLiteral("zh_CN"), QStringLiteral("简体中文"), true, {}},
                             },
        };
        result.singing = Automation::SingingPublicSettingsDto{
            .configuredDefaultLanguage = settingsSnapshot->general.defaultSingingLanguage,
            .effectiveDefaultLanguage = settingsSnapshot->general.defaultSingingLanguage,
            .configuredDefaultLyrics = settingsSnapshot->general.defaultLyrics,
            .effectiveDefaultLyrics = settingsSnapshot->general.defaultLyrics,
            .languageCandidates =
                {
                                     {QStringLiteral("unknown"), QStringLiteral("Unknown"), true, {}},
                                     {QStringLiteral("en"), QStringLiteral("English"), true, {}},
                                     {QStringLiteral("zh"), QStringLiteral("Chinese"), true, {}},
                                     },
        };
        result.theme = Automation::ThemePublicSettingsDto{
            .configured = settingsSnapshot->appearance.themeId,
            .effective = settingsSnapshot->appearance.themeId,
            .candidates =
                {
                             {QStringLiteral("light"), QStringLiteral("Light"), true, {}},
                             {QStringLiteral("dark"), QStringLiteral("Dark"), true, {}},
                             },
        };
        Automation::AudioDevicePublicSettingsDto audio;
        audio.configured = {
            .driverName = settingsSnapshot->audio.driverName,
            .deviceName = settingsSnapshot->audio.deviceName,
            .bufferSize = settingsSnapshot->audio.adoptedBufferSize,
            .sampleRate = settingsSnapshot->audio.adoptedSampleRate,
            .hotPlugNotificationMode = settingsSnapshot->audio.hotPlugNotificationMode,
            .gain = settingsSnapshot->audio.deviceGain,
            .pan = settingsSnapshot->audio.devicePan,
        };
        audio.effective = audio.configured;
        audio.drivers = {
            {
             .id = QStringLiteral("test-driver"),
             .displayName = QStringLiteral("Test Driver"),
             .devices =
                    {
                        {
                            .id = {},
                            .displayName = QStringLiteral("Default Device"),
                        },
                        {
                            .id = QStringLiteral("test-device"),
                            .displayName = QStringLiteral("Test Device"),
                            .bufferSizes = {256, 512},
                            .sampleRates = {44100.0, 48000.0},
                        },
                    }, }
        };
        result.audioDevice = audio;
        result.playbackBehavior = Automation::PlaybackBehaviorPublicSettingsDto{
            .configured = settingsSnapshot->audio.playheadBehavior,
            .effective = settingsSnapshot->audio.playheadBehavior,
            .candidates = {0, 1, 2},
        };
        result.computeDevice = Automation::ComputeDevicePublicSettingsDto{
            .configured = {settingsSnapshot->inference.executionProvider,
                           settingsSnapshot->inference.selectedGpuIndex,
                           settingsSnapshot->inference.selectedGpuId},
            .effective = {settingsSnapshot->inference.executionProvider,
                           settingsSnapshot->inference.selectedGpuIndex,
                           settingsSnapshot->inference.selectedGpuId},
            .providerCandidates =
                {
                           {QStringLiteral("CPU"), QStringLiteral("CPU"), true, {}},
                           {QStringLiteral("DML"), QStringLiteral("DirectML"), true, {}},
                           },
            .gpuCandidates = {{0, QStringLiteral("gpu-0"), QStringLiteral("GPU 0"), true, {}}},
        };
        const Automation::RenderPublicValueDto renderValue{
            settingsSnapshot->inference.samplingSteps,
            settingsSnapshot->inference.depth,
            settingsSnapshot->inference.runVocoderOnCpu,
            settingsSnapshot->inference.autoStartInference,
            settingsSnapshot->inference.playbackLookaheadSeconds,
            settingsSnapshot->inference.pitchSmoothKernelSize,
        };
        result.render = Automation::RenderPublicSettingsDto{
            .configured = renderValue,
            .effective = renderValue,
            .samplingStepsRange = {1,   100,   1   },
            .depthRange = {0.0, 1.0,   0.01},
            .playbackLookaheadRange = {1.0, 120.0, 1.0 },
            .pitchSmoothKernelRange = {0,   21,    2   },
        };
        const Automation::SingerSessionRetentionPublicValueDto retentionValue{
            settingsSnapshot->inference.singerSessionCacheCapacity,
            settingsSnapshot->inference.singerSessionIdleTimeoutSeconds,
        };
        result.singerSessionRetention = Automation::SingerSessionRetentionPublicSettingsDto{
            .configured = retentionValue,
            .effective = retentionValue,
            .capacityCandidates = {1,  4,  8  },
            .idleTimeoutCandidates = {30, 60, 120},
        };
        result.packageSearchPaths = Automation::PackageSearchPathsPublicSettingsDto{
            .configured = settingsSnapshot->general.packageSearchPaths,
            .effective = settingsSnapshot->general.packageSearchPaths,
        };
        return result;
    };
    settingsServices.applyGeneral =
        [settingsSnapshot](const Automation::GeneralSettingsDto &value) {
            settingsSnapshot->general = value;
            return true;
        };
    settingsServices.applyAppearance =
        [settingsSnapshot](const Automation::AppearanceSettingsDto &value) {
            settingsSnapshot->appearance = value;
            return true;
        };
    settingsServices.applyInference =
        [settingsSnapshot](const Automation::InferenceSettingsDto &value) {
            settingsSnapshot->inference = value;
            return true;
        };
    settingsServices.applyAudio = [settingsSnapshot](const Automation::AudioSettingsDto &value) {
        settingsSnapshot->audio = value;
        return true;
    };
    settingsServices.applyUiLanguage =
        [settingsSnapshot](const Automation::GeneralSettingsDto &value) {
            settingsSnapshot->general = value;
            return Automation::AutomationResult<Automation::AutomationUnit>(
                Automation::AutomationUnit{});
        };
    settingsServices.applyTheme =
        [settingsSnapshot](const Automation::AppearanceSettingsDto &value) {
            settingsSnapshot->appearance = value;
            return Automation::AutomationResult<Automation::AutomationUnit>(
                Automation::AutomationUnit{});
        };
    settingsServices.applyAudioDevice =
        [settingsSnapshot](const Automation::AudioSettingsDto &value,
                           const Automation::AudioDeviceSettingsPatchDto &) {
            settingsSnapshot->audio = value;
            return Automation::AutomationResult<Automation::AutomationUnit>(
                Automation::AutomationUnit{});
        };
    settingsServices.applyFillLyric =
        [settingsSnapshot](const Automation::FillLyricSettingsDto &value) {
            settingsSnapshot->fillLyric = value;
            return true;
        };
    const auto builtinLyricRuleId = QStringLiteral("builtin-splitter-1234567890abcdef12345678");
    const auto customLyricRuleId = QStringLiteral("11111111-1111-4111-8111-111111111111");
    settingsSnapshot->fillLyric.builtinSplitterEnabled.insert(QStringLiteral("Character"), true);
    settingsSnapshot->fillLyric.customSplitterRules.append({
        .ruleId = customLyricRuleId,
        .name = QStringLiteral("Registry Custom"),
        .regexes = {QStringLiteral("\\s+")},
        .enabled = true,
        .order = 1,
    });
    settingsSnapshot->fillLyric.splitterOrder = {QStringLiteral("Character"),
                                                 QStringLiteral("Registry Custom")};
    auto lyricRules =
        std::make_shared<QList<Automation::LyricRuleDto>>(QList<Automation::LyricRuleDto>{
            Automation::LyricRuleDto{
                                     .ruleId = builtinLyricRuleId,
                                     .kind = Automation::LyricRuleKind::Splitter,
                                     .builtin = true,
                                     .name = QStringLiteral("Character"),
                                     .regexes = {QStringLiteral(".")},
                                     .enabled = true,
                                     .order = 0,
                                     .engineOrderKey = QStringLiteral("Character"),
                                     },
            Automation::LyricRuleDto{
                                     .ruleId = customLyricRuleId,
                                     .kind = Automation::LyricRuleKind::Splitter,
                                     .builtin = false,
                                     .name = QStringLiteral("Registry Custom"),
                                     .regexes = {QStringLiteral("\\s+")},
                                     .enabled = true,
                                     .order = 1,
                                     .engineOrderKey = QStringLiteral("Registry Custom"),
                                     },
    });
    settingsServices.lyricRules = [lyricRules] { return *lyricRules; };
    settingsServices.testLyricRules = [](const QString &text) {
        return Automation::AutomationResult<Automation::LyricRuleTestResultDto>(
            Automation::LyricRuleTestResultDto{
                .splitTokens = {text},
                .taggedTokens = {{text, QStringLiteral("en"), QStringLiteral("word"), false}},
            });
    };
    auto presetStore = std::make_shared<QList<Automation::SpeakerMixPresetDto>>();
    Automation::PresetRuntimeServices presetServices;
    presetServices.speakerMixPresets = [presetStore] { return *presetStore; };
    presetServices.applySpeakerMixPresets =
        [presetStore](const QList<Automation::SpeakerMixPresetDto> &presets) {
            *presetStore = presets;
            return true;
        };

    auto editorViewState = std::make_shared<EditorViewState>();
    auto editorStableState = std::make_shared<Automation::EditorStableState>();
    Automation::EditorRuntimeServices editorServices;
    editorServices.captureView = [editorViewState] {
        return std::optional<EditorViewState>(*editorViewState);
    };
    editorServices.captureStableState = [editorStableState] { return *editorStableState; };
    editorServices.restoreView = [editorViewState](const EditorViewState &state) {
        *editorViewState = state;
        return true;
    };
    editorServices.setTrackPanelViewport = [editorViewState](const TrackPanelViewState &state) {
        editorViewState->trackPanel = state;
        return true;
    };
    editorServices.setPanelVisibility = [editorViewState](const bool trackVisible,
                                                          const bool clipVisible) {
        if (!trackVisible && !clipVisible)
            return false;
        editorViewState->layout.trackPanelVisible = trackVisible;
        editorViewState->layout.bottomPanelVisible = clipVisible;
        return true;
    };
    const auto focusRegion = [editorViewState](const EditorViewGlobal::Region region) {
        if (region == EditorViewGlobal::Region::TrackPanel) {
            editorViewState->layout.trackPanelVisible = true;
        } else if (region == EditorViewGlobal::Region::PianoRoll ||
                   region == EditorViewGlobal::Region::Parameters) {
            editorViewState->layout.bottomPanelVisible = true;
            editorViewState->layout.bottomPanelPageId = QStringLiteral("ClipEditor");
            if (region == EditorViewGlobal::Region::PianoRoll)
                editorViewState->layout.pianoRollVisible = true;
            else
                editorViewState->layout.parametersVisible = true;
        } else {
            return false;
        }
        editorViewState->layout.activeRegion = region;
        editorViewState->layout.focusedRegion = region;
        return true;
    };
    editorServices.showRegion = focusRegion;
    editorServices.focusRegion = focusRegion;
    editorServices.setClipEditorTimeViewport = [editorViewState](const double centerTick,
                                                                 const double horizontalScale) {
        editorViewState->pianoRoll.centerTick = centerTick;
        editorViewState->pianoRoll.horizontalScale = horizontalScale;
        return true;
    };
    editorServices.setPianoRollPitchViewport = [editorViewState](const double centerKeyIndex,
                                                                 const double verticalScale) {
        editorViewState->pianoRoll.centerKeyIndex = centerKeyIndex;
        editorViewState->pianoRoll.verticalScale = verticalScale;
        return true;
    };
    editorServices.setPianoRollEditMode =
        [editorViewState](const EditorViewGlobal::PianoRollEditMode mode) {
            editorViewState->pianoRoll.editMode = mode;
            return true;
        };
    editorServices.setParameterForeground = [editorViewState](const ParamInfo::Name name) {
        editorViewState->parameters.foreground = name;
        editorViewState->layout.bottomPanelVisible = true;
        editorViewState->layout.activeRegion = EditorViewGlobal::Region::Parameters;
        editorViewState->layout.focusedRegion = EditorViewGlobal::Region::Parameters;
        return true;
    };
    editorServices.setParameterBackground = [editorViewState](const ParamInfo::Name name) {
        editorViewState->parameters.background = name;
        editorViewState->layout.bottomPanelVisible = true;
        editorViewState->layout.activeRegion = EditorViewGlobal::Region::Parameters;
        editorViewState->layout.focusedRegion = EditorViewGlobal::Region::Parameters;
        return true;
    };
    editorServices.swapParameters = [editorViewState] {
        std::swap(editorViewState->parameters.foreground, editorViewState->parameters.background);
        editorViewState->layout.bottomPanelVisible = true;
        editorViewState->layout.activeRegion = EditorViewGlobal::Region::Parameters;
        editorViewState->layout.focusedRegion = EditorViewGlobal::Region::Parameters;
        return true;
    };
    editorServices.setParameterEditMode =
        [editorViewState](const EditorViewGlobal::ParameterEditMode mode) {
            editorViewState->parameters.editMode = mode;
            editorViewState->layout.bottomPanelVisible = true;
            editorViewState->layout.activeRegion = EditorViewGlobal::Region::Parameters;
            editorViewState->layout.focusedRegion = EditorViewGlobal::Region::Parameters;
            return true;
        };
    editorServices.setParameterValueViewport = [editorViewState](const double centerRatio,
                                                                 const double verticalScale) {
        editorViewState->parameters.centerRatio = centerRatio;
        editorViewState->parameters.verticalScale = verticalScale;
        editorViewState->layout.bottomPanelVisible = true;
        editorViewState->layout.activeRegion = EditorViewGlobal::Region::Parameters;
        editorViewState->layout.focusedRegion = EditorViewGlobal::Region::Parameters;
        return true;
    };
    editorServices.setActiveClip = [editorStableState](const int clipId) {
        editorStableState->activeClipId = clipId;
    };
    editorServices.setSelectedTrackIndex = [editorStableState](const int trackIndex) {
        editorStableState->selectedTrackIndex = trackIndex;
    };
    editorServices.setSelectedClips = [editorStableState](const QList<int> &clipIds,
                                                          const int primaryClipId) {
        editorStableState->selectedClipIds = clipIds;
        editorStableState->primaryClipId = primaryClipId;
    };
    editorServices.setSelectedNotes =
        [editorStableState](const int clipId, const QList<int> &noteIds, const int primaryNoteId) {
            editorStableState->activeClipId = clipId;
            editorStableState->selectedNoteIds = noteIds;
            editorStableState->primaryNoteId = primaryNoteId;
        };
    editorServices.setPianoRollQuantize = [editorStableState](const int quantize,
                                                              const bool enabled) {
        editorStableState->pianoRollQuantize = quantize;
        editorStableState->pianoRollQuantizeEnabled = enabled;
    };
    editorServices.setAutoPageTurn =
        [editorStableState](const Automation::EditorAutoPageTarget target, const bool enabled) {
            if (target == Automation::EditorAutoPageTarget::TrackPanel)
                editorStableState->trackAutoPageTurnEnabled = enabled;
            else
                editorStableState->pianoRollAutoPageTurnEnabled = enabled;
        };
    editorServices.focusVisibility = [](const HistoryFocus &) {
        return HistoryFocusVisibility::ScrollRequired;
    };
    editorServices.revealFocus = [editorViewState, editorStableState](const HistoryFocus &focus,
                                                                      const bool) {
        if (focus.kind == HistoryFocusKind::TrackClips) {
            editorViewState->layout.trackPanelVisible = true;
            editorViewState->layout.activeRegion = EditorViewGlobal::Region::TrackPanel;
            editorViewState->layout.focusedRegion = EditorViewGlobal::Region::TrackPanel;
            editorViewState->trackPanel.centerTick = (focus.tickStart + focus.tickEnd) * 0.5;
        } else {
            editorViewState->layout.bottomPanelVisible = true;
            editorViewState->layout.bottomPanelPageId = QStringLiteral("ClipEditor");
            editorViewState->layout.pianoRollVisible = true;
            editorViewState->layout.activeRegion = EditorViewGlobal::Region::PianoRoll;
            editorViewState->layout.focusedRegion = EditorViewGlobal::Region::PianoRoll;
            editorViewState->pianoRoll.centerTick = (focus.tickStart + focus.tickEnd) * 0.5;
            editorStableState->activeClipId = focus.containerId;
        }
        return true;
    };

    AutomationTestSupport::TestRuntime fixture(
        std::move(editorServices), std::move(documentServices), std::move(fileServices),
        std::move(audioExportServices), std::move(packageServices), std::move(playbackServices),
        std::move(applicationServices), std::move(settingsServices), std::move(presetServices));
    fixture.model().newProject();
    auto &runtime = fixture.runtime();

    Automation::CommandContext startupLifecycleContext;
    startupLifecycleContext.expected = runtime.documentVersion();
    startupLifecycleContext.source = Automation::InvocationSource::InternalAutomation;
    const auto startupReplacement = runtime.documents().commitNewDocument(
        startupLifecycleContext, Automation::DocumentAutomationFacade::newDocumentDraft(false));
    expect(bool(startupReplacement),
           QStringLiteral("empty audio-export lifecycle must survive initial generation replace"));
    runtime.audioExports().discardDocumentGeneration(runtime.documentVersion().documentId);

    QTemporaryDir directory;
    QFile readable(directory.filePath(QStringLiteral("project.dspx")));
    expect(directory.isValid() && readable.open(QIODevice::WriteOnly),
           QStringLiteral("test file root must be available"));
    readable.close();
    QFile backgroundOpenFile(directory.filePath(QStringLiteral("background.mid")));
    expect(backgroundOpenFile.open(QIODevice::WriteOnly),
           QStringLiteral("async document-open fixture must use a registered format"));
    backgroundOpenFile.close();

    Automation::AutomationAccessPolicy access(AutomationWire::AutomationProfile::L3);
    Automation::AutomationFileGuard fileGuard;
    expect(bool(fileGuard.setConfiguredRoots({directory.path()}, {directory.path()})),
           QStringLiteral("file guard roots must configure"));
    Automation::AdmissionLimits limits;
    limits.maximumGlobalInFlight = 8;
    limits.maximumBackgroundTasks = 1;
    Automation::AdmissionController admission(limits);
    auto services = hostServices(runtime);
    int audioPathPreparationCount = 0;
    bool failAudioPathPreparation = false;
    bool revokeAudioPathAccessDuringPreparation = false;
    QString lastPreparedAudioPath;
    services.prepareAudioPath = [&](const QString &canonicalPath)
        -> Automation::AutomationResult<Automation::PublicPreparedAudioPath> {
        ++audioPathPreparationCount;
        lastPreparedAudioPath = canonicalPath;
        if (failAudioPathPreparation) {
            Automation::AutomationError failure;
            failure.code = Automation::AutomationErrorCode::IoError;
            failure.fieldPath = QStringLiteral("path");
            failure.message = QStringLiteral("Controlled audio decoding failure");
            return failure;
        }
        if (revokeAudioPathAccessDuringPreparation)
            fileGuard.setConfiguredRoots({}, {});
        return Automation::PublicPreparedAudioPath{
            QStringLiteral("prepared-sha512-%1").arg(audioPathPreparationCount),
            QJsonObject{
                        {QStringLiteral("entryClassName"), QStringLiteral("PreparedFormatEntry")},
                        {QStringLiteral("userData"), QString::number(audioPathPreparationCount)},
                        },
        };
    };
    Automation::PublicAutomationRegistry registry(runtime, access, fileGuard, admission,
                                                  std::move(services));

    const auto settingsBeforeAdvancedBindings = *settingsSnapshot;
    const auto lyricRulesBeforeAdvancedBindings = *lyricRules;
    verifyAdvancedApplicationBindings(registry, runtime, directory.path(), builtinLyricRuleId,
                                      customLyricRuleId, *packageRefreshControl);
    *settingsSnapshot = settingsBeforeAdvancedBindings;
    *lyricRules = lyricRulesBeforeAdvancedBindings;
    verifyPackageRefreshLifetime(runtime, access, fileGuard, admission, *packageRefreshControl);

    const auto applicationInfo = registry.invoke(QStringLiteral("application.get_info"), {});
    expect(applicationInfo &&
               applicationInfo.get().value(QStringLiteral("build_id")).toString() ==
                   QStringLiteral("registry-build-id") &&
               applicationInfo.get().value(QStringLiteral("build_id")).toString() !=
                   applicationInfo.get().value(QStringLiteral("version")).toString(),
           QStringLiteral("application.get_info must expose the host build identifier"));

    const auto recentDocuments = registry.invoke(QStringLiteral("documents.list_recent"), {});
    const auto recentProjects =
        recentDocuments ? recentDocuments.get().value(QStringLiteral("projects")).toArray()
                        : QJsonArray{};
    expect(
        recentDocuments && recentProjects.size() == 2 &&
            recentProjects.first().toObject().value(QStringLiteral("file_name")).toString() ==
                QStringLiteral("registry-recent-a.dspx") &&
            !recentProjects.first().toObject().value(QStringLiteral("exists")).toBool(),
        QStringLiteral("documents.list_recent must expose configured paths without opening them"));

    const QJsonObject currentDocumentQuery{
        {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
    };
    const auto initialDocument =
        registry.invoke(QStringLiteral("documents.get"), currentDocumentQuery);
    const auto initialStatistics = initialDocument ? initialDocument.get()
                                                         .value(QStringLiteral("snapshot"))
                                                         .toObject()
                                                         .value(QStringLiteral("statistics"))
                                                         .toObject()
                                                   : QJsonObject{};
    expect(initialDocument &&
               initialStatistics.value(QStringLiteral("length_ticks")).toInt(-1) == 0 &&
               initialStatistics.value(QStringLiteral("track_count")).toInt(-1) == 0 &&
               initialStatistics.value(QStringLiteral("clip_count")).toInt(-1) == 0,
           QStringLiteral("documents.get must embed zeroed statistics for an empty project"));

    auto expectedIds = PublicAutomationToolsetExpectations::editorToolIds();
    auto bindingIds = registry.bindingIds();
    std::sort(expectedIds.begin(), expectedIds.end());
    expect(expectedIds.size() == 177 && bindingIds == expectedIds && registry.isComplete(),
           QStringLiteral("all 177 editor contracts must have exact bindings"));

    const auto lifecycleSchemaBase = runtime.documentVersion();
    for (const auto &operationId :
         {QStringLiteral("documents.new"), QStringLiteral("documents.open")}) {
        QJsonObject arguments{
            {QStringLiteral("current_document_id"), lifecycleSchemaBase.documentId.toString()  },
            {QStringLiteral("expected_revision"),
             static_cast<qint64>(lifecycleSchemaBase.revision)                                 },
            {QStringLiteral("unsaved_policy"),      QStringLiteral("discard")                  },
            {QStringLiteral("idempotency_key"),     QStringLiteral("unsupported-lifecycle-key")},
        };
        if (operationId == QStringLiteral("documents.open"))
            arguments.insert(QStringLiteral("path"), backgroundOpenFile.fileName());
        const auto rejected = registry.invoke(
            operationId, arguments,
            {.clientId = QStringLiteral("lifecycle-idempotency-schema-") + operationId});
        expect(!rejected &&
                   rejected.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
                   runtime.documentVersion() == lifecycleSchemaBase,
               operationId +
                   QStringLiteral(" must reject unsupported idempotency_key without replacement"));
    }
    expect(admission.snapshot().backgroundTasks == 0,
           QStringLiteral("rejected lifecycle idempotency input must not create a task"));

    const auto initiallyEmptyProject =
        runtime.project().getProject(runtime.documentVersion().documentId);
    expect(initiallyEmptyProject && initiallyEmptyProject.get().tracks.isEmpty(),
           QStringLiteral("empty-project track default fixture must start without tracks"));
    const auto emptyTrackBase = runtime.documentVersion();
    auto emptyTrackInput = commandArguments(emptyTrackBase);
    emptyTrackInput.insert(QStringLiteral("index"), 0);
    emptyTrackInput.insert(QStringLiteral("tracks"),
                           QJsonArray{QJsonObject{{QStringLiteral("client_ref"),
                                                   QStringLiteral("empty-project-track")}}});
    const auto emptyTrackCreated =
        registry.invoke(QStringLiteral("tracks.insert"), emptyTrackInput,
                        {.clientId = QStringLiteral("empty-project-track-default")});
    const auto emptyTrackIds =
        emptyTrackCreated ? createdObjectIds(emptyTrackCreated.get(), QStringLiteral("track"))
                          : QList<int>{};
    const auto emptyProjectTrack =
        emptyTrackIds.size() == 1
            ? trackSnapshot(runtime, Automation::TrackId(emptyTrackIds.constFirst()))
            : std::nullopt;
    expect(bool(emptyTrackCreated), QStringLiteral("empty-project tracks.insert must succeed"));
    expect(emptyTrackIds.size() == 1,
           QStringLiteral("empty-project tracks.insert must return one track id"));
    expect(emptyProjectTrack.has_value(),
           QStringLiteral("empty-project tracks.insert result must remain queryable"));
    expect(emptyProjectTrack && emptyProjectTrack->data.colorIndex == 0,
           QStringLiteral("omitted color in an empty project must apply palette index zero"));
    expect(emptyTrackCreated &&
               resolvedValue(emptyTrackCreated.get(), QStringLiteral("/tracks/0/color_index"))
                       .toInt() == 0,
           QStringLiteral("omitted color in an empty project must report palette index zero"));
    expect(runtime.documentVersion().revision == emptyTrackBase.revision + 1,
           QStringLiteral("empty-project tracks.insert must advance one revision"));
    const auto documentWithEmptyTrack =
        registry.invoke(QStringLiteral("documents.get"), currentDocumentQuery);
    const auto emptyTrackStatistics = documentWithEmptyTrack
                                          ? documentWithEmptyTrack.get()
                                                .value(QStringLiteral("snapshot"))
                                                .toObject()
                                                .value(QStringLiteral("statistics"))
                                                .toObject()
                                          : QJsonObject{};
    expect(documentWithEmptyTrack &&
               emptyTrackStatistics.value(QStringLiteral("track_count")).toInt(-1) == 1 &&
               emptyTrackStatistics.value(QStringLiteral("empty_track_count")).toInt(-1) == 1 &&
               emptyTrackStatistics.value(QStringLiteral("clip_count")).toInt(-1) == 0 &&
               emptyTrackStatistics.value(QStringLiteral("length_ticks")).toInt(-1) == 0,
           QStringLiteral("documents.get must classify an empty track without a project snapshot"));
    const auto undoEmptyTrack =
        registry.invoke(QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
                        {.clientId = QStringLiteral("empty-project-track-default-undo")});
    expect(bool(undoEmptyTrack),
           QStringLiteral("empty-project track insertion must expose one History undo"));
    expect(emptyTrackIds.size() == 1 &&
               !trackSnapshot(runtime, Automation::TrackId(emptyTrackIds.constFirst())),
           QStringLiteral("empty-project track insertion undo must remove the created track"));
    expect(runtime.documentVersion().revision == emptyTrackBase.revision + 2,
           QStringLiteral("empty-project track insertion undo must advance one revision"));

    const QJsonObject playbackQuery{
        {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
    };
    const auto initialPlayback = registry.invoke(QStringLiteral("playback.get"), playbackQuery);
    const auto initialStateVersion = initialPlayback ? initialPlayback.get()
                                                           .value(QStringLiteral("snapshot"))
                                                           .toObject()
                                                           .value(QStringLiteral("state_version"))
                                                           .toInteger()
                                                     : 0;
    playbackState->position = 4.0;
    auto staleSeekInput = playbackQuery;
    staleSeekInput.insert(QStringLiteral("position"), 8.0);
    staleSeekInput.insert(QStringLiteral("expected_state_version"), initialStateVersion);
    const auto staleSeek = registry.invoke(QStringLiteral("playback.seek"), staleSeekInput);
    expect(!staleSeek &&
               staleSeek.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
               staleSeek.getError().fieldPath == QStringLiteral("expected_state_version") &&
               playbackState->position == 4.0,
           QStringLiteral("playback mutations must reject stale state without host side effects"));
    const auto refreshedPlayback = registry.invoke(QStringLiteral("playback.get"), playbackQuery);
    auto currentSeekInput = staleSeekInput;
    currentSeekInput.insert(QStringLiteral("expected_state_version"),
                            refreshedPlayback ? refreshedPlayback.get()
                                                    .value(QStringLiteral("snapshot"))
                                                    .toObject()
                                                    .value(QStringLiteral("state_version"))
                                                    .toInteger()
                                              : 0);
    const auto currentSeek = registry.invoke(QStringLiteral("playback.seek"), currentSeekInput);
    expect(currentSeek && playbackState->position == 8.0,
           QStringLiteral("playback mutations must accept the current state version"));

    const auto playbackBeforeWrongDocument = *playbackState;
    const auto wrongPlaybackDocument = Automation::DocumentId::create().toString();
    const QJsonObject wrongPlaybackContext{
        {QStringLiteral("document_id"), wrongPlaybackDocument},
    };
    const auto wrongPlay = registry.invoke(QStringLiteral("playback.play"), wrongPlaybackContext);
    const auto wrongPause = registry.invoke(QStringLiteral("playback.pause"), wrongPlaybackContext);
    const auto wrongStop = registry.invoke(QStringLiteral("playback.stop"), wrongPlaybackContext);
    auto wrongSeekContext = wrongPlaybackContext;
    wrongSeekContext.insert(QStringLiteral("position"), 99.0);
    const auto wrongSeek = registry.invoke(QStringLiteral("playback.seek"), wrongSeekContext);
    expect(!wrongPlay &&
               wrongPlay.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
               wrongPlay.getError().operationId == Automation::OperationIds::playback::play,
           QStringLiteral("playback.play must reject a wrong document without a state token"));
    expect(!wrongPause &&
               wrongPause.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
               wrongPause.getError().operationId == Automation::OperationIds::playback::pause,
           QStringLiteral("playback.pause must reject a wrong document without a state token"));
    expect(!wrongStop &&
               wrongStop.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
               wrongStop.getError().operationId == Automation::OperationIds::playback::stop,
           QStringLiteral("playback.stop must reject a wrong document without a state token"));
    expect(!wrongSeek &&
               wrongSeek.getError().code == Automation::AutomationErrorCode::DocumentChanged &&
               wrongSeek.getError().operationId == Automation::OperationIds::playback::seek,
           QStringLiteral("playback.seek must reject a wrong document without a state token"));
    expect(playbackState->state == playbackBeforeWrongDocument.state &&
               playbackState->position == playbackBeforeWrongDocument.position &&
               playbackState->lastPosition == playbackBeforeWrongDocument.lastPosition &&
               playbackState->loop == playbackBeforeWrongDocument.loop,
           QStringLiteral("wrong-document playback calls must not reach the host"));

    const auto loopBase = runtime.documentVersion();
    auto fractionalLoopInput = commandArguments(loopBase);
    fractionalLoopInput.insert(QStringLiteral("start"), 120.5);
    fractionalLoopInput.insert(QStringLiteral("end"), 960);
    fractionalLoopInput.insert(
        QStringLiteral("expected_state_version"),
        currentSeek ? currentSeek.get().value(QStringLiteral("state_version")).toInteger() : 0);
    const auto fractionalLoop =
        registry.invoke(QStringLiteral("playback.set_loop"), fractionalLoopInput,
                        {.clientId = QStringLiteral("fractional-loop")});
    expect(!fractionalLoop &&
               fractionalLoop.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
               runtime.documentVersion() == loopBase && playbackState->loop == LoopSettings(),
           QStringLiteral("playback.set_loop must reject fractional ticks without truncation"));

    auto setLoopInput = commandArguments(loopBase);
    setLoopInput.insert(QStringLiteral("start"), 120);
    setLoopInput.insert(QStringLiteral("end"), 960);
    setLoopInput.insert(
        QStringLiteral("expected_state_version"),
        currentSeek ? currentSeek.get().value(QStringLiteral("state_version")).toInteger() : 0);
    const auto setLoop = registry.invoke(QStringLiteral("playback.set_loop"), setLoopInput,
                                         {.clientId = QStringLiteral("persistent-loop-set")});
    const auto *setLoopContract =
        AutomationWire::findPublicTool(QStringLiteral("playback.set_loop"));
    const auto setLoopPrevious =
        setLoop ? setLoop.get().value(QStringLiteral("previous")).toObject() : QJsonObject{};
    const auto setLoopCurrent =
        setLoop ? setLoop.get().value(QStringLiteral("current")).toObject() : QJsonObject{};
    const auto setLoopPlayback =
        setLoop ? setLoop.get().value(QStringLiteral("playback")).toObject() : QJsonObject{};
    expect(setLoop && setLoopContract &&
               AutomationWire::validateJsonValue(setLoop.get(), setLoopContract->outputSchema)
                   .valid() &&
               setLoopPrevious.value(QStringLiteral("revision")).toInteger() ==
                   static_cast<qint64>(loopBase.revision) &&
               setLoopCurrent.value(QStringLiteral("revision")).toInteger() ==
                   static_cast<qint64>(loopBase.revision + 1) &&
               setLoopPlayback.value(QStringLiteral("loop"))
                       .toObject()
                       .value(QStringLiteral("start"))
                       .toInt() == 120 &&
               setLoopPlayback.value(QStringLiteral("loop"))
                       .toObject()
                       .value(QStringLiteral("end"))
                       .toInt() == 960,
           QStringLiteral(
               "loop edits must return both standard document Mutation and playback snapshots"));

    const auto undoSetLoop =
        registry.invoke(QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
                        {.clientId = QStringLiteral("persistent-loop-set-undo")});
    const bool loopSetUndone = playbackState->loop == LoopSettings();
    const auto redoSetLoop =
        registry.invoke(QStringLiteral("history.redo"), commandArguments(runtime.documentVersion()),
                        {.clientId = QStringLiteral("persistent-loop-set-redo")});
    const auto playbackAfterRedo = registry.invoke(QStringLiteral("playback.get"), playbackQuery);
    const auto playbackAfterRedoSnapshot =
        playbackAfterRedo ? playbackAfterRedo.get().value(QStringLiteral("snapshot")).toObject()
                          : QJsonObject{};
    const auto stateVersionAfterRedo =
        playbackAfterRedoSnapshot.value(QStringLiteral("state_version")).toInteger();

    auto staleLoopEnable = commandArguments(loopBase);
    staleLoopEnable.insert(QStringLiteral("enabled"), false);
    staleLoopEnable.insert(QStringLiteral("expected_state_version"), stateVersionAfterRedo);
    const auto rejectedStaleLoopEnable =
        registry.invoke(QStringLiteral("playback.set_loop_enabled"), staleLoopEnable,
                        {.clientId = QStringLiteral("persistent-loop-enable-stale")});

    auto disableLoop = commandArguments(runtime.documentVersion());
    disableLoop.insert(QStringLiteral("enabled"), false);
    disableLoop.insert(QStringLiteral("expected_state_version"), stateVersionAfterRedo);
    const auto disabledLoop =
        registry.invoke(QStringLiteral("playback.set_loop_enabled"), disableLoop,
                        {.clientId = QStringLiteral("persistent-loop-disable")});
    auto clearLoop = commandArguments(runtime.documentVersion());
    clearLoop.insert(
        QStringLiteral("expected_state_version"),
        disabledLoop ? disabledLoop.get().value(QStringLiteral("state_version")).toInteger() : 0);
    const auto clearedLoop = registry.invoke(QStringLiteral("playback.clear_loop"), clearLoop,
                                             {.clientId = QStringLiteral("persistent-loop-clear")});
    const auto undoClearLoop =
        registry.invoke(QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
                        {.clientId = QStringLiteral("persistent-loop-clear-undo")});
    const bool clearRestoredDisabledRange = !playbackState->loop.enabled &&
                                            playbackState->loop.start == 120 &&
                                            playbackState->loop.end() == 960;
    const auto undoDisableLoop =
        registry.invoke(QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
                        {.clientId = QStringLiteral("persistent-loop-disable-undo")});
    const bool disableRestoredEnabledRange = playbackState->loop.enabled &&
                                             playbackState->loop.start == 120 &&
                                             playbackState->loop.end() == 960;
    const auto undoInitialLoop =
        registry.invoke(QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
                        {.clientId = QStringLiteral("persistent-loop-initial-undo")});
    expect(undoSetLoop && loopSetUndone && redoSetLoop && playbackAfterRedo &&
               playbackState->position == 8.0 && !rejectedStaleLoopEnable &&
               rejectedStaleLoopEnable.getError().code ==
                   Automation::AutomationErrorCode::RevisionConflict &&
               disabledLoop && clearedLoop && undoClearLoop && clearRestoredDisabledRange &&
               undoDisableLoop && disableRestoredEnabledRange && undoInitialLoop &&
               playbackState->loop == LoopSettings() &&
               runtime.documentVersion().revision == loopBase.revision + 8,
           QStringLiteral(
               "loop range, enabled flag, and clear must each use one undoable document revision"));
    const auto status = registry.invoke(QStringLiteral("automation.get_status"), {},
                                        {.clientId = QStringLiteral("status")});
    expect(status && status.get().value(QStringLiteral("documents")).toArray().size() == 1 &&
               status.get().value(QStringLiteral("windows")).toArray().size() == 1,
           QStringLiteral("single-document host status must truncate documents and windows"));
    access.update(AutomationWire::AutomationProfile::L1);
    const auto denied = registry.invoke(QStringLiteral("formats.list"), {},
                                        {.clientId = QStringLiteral("permission")});
    expect(!denied && denied.getError().code == Automation::AutomationErrorCode::PermissionDenied,
           QStringLiteral("L1 must deny an L2 tool"));
    const auto hiddenOptions =
        registry.invoke(QStringLiteral("automation.get_options"),
                        QJsonObject{
                            {QStringLiteral("operation_id"),      QStringLiteral("exports.audio.start")},
                            {QStringLiteral("field_path"),        QStringLiteral("/options/format")    },
                            {QStringLiteral("partial_arguments"), QJsonObject{}                        },
    },
                        {.clientId = QStringLiteral("permission-options")});
    expect(!hiddenOptions &&
               hiddenOptions.getError().code == Automation::AutomationErrorCode::PermissionDenied,
           QStringLiteral("get_options must not reveal a profile-blocked target"));
    access.update(AutomationWire::AutomationProfile::Custom, {QStringLiteral("documents.get")});
    expect(access.isAllowed(QStringLiteral("automation.get_status")) &&
               access.isAllowed(QStringLiteral("documents.get")) &&
               !access.isAllowed(QStringLiteral("tracks.set_color")),
           QStringLiteral("custom profile must retain Meta and only explicit business tools"));
    access.update(AutomationWire::AutomationProfile::L3);
    const auto staticOptions =
        registry.invoke(QStringLiteral("automation.get_options"),
                        QJsonObject{
                            {QStringLiteral("operation_id"),      QStringLiteral("documents.new")},
                            {QStringLiteral("field_path"),        QStringLiteral("/template")    },
                            {QStringLiteral("partial_arguments"), QJsonObject{}                  },
    },
                        {.clientId = QStringLiteral("static-options")});
    expect(!staticOptions &&
               staticOptions.getError().code == Automation::AutomationErrorCode::InvalidArgument,
           QStringLiteral("get_options must reject fixed enums already declared by input schema"));

    const QJsonObject registrySingerRef{
        {QStringLiteral("package_id"),      QStringLiteral("registry-package")},
        {QStringLiteral("package_version"), QStringLiteral("1.0")             },
        {QStringLiteral("singer_id"),       QStringLiteral("registry-singer") },
    };
    const QJsonObject registrySingerV2Ref{
        {QStringLiteral("package_id"),      QStringLiteral("registry-package")},
        {QStringLiteral("package_version"), QStringLiteral("2.0")             },
        {QStringLiteral("singer_id"),       QStringLiteral("registry-singer") },
    };
    const auto singerOptions =
        registry.invoke(QStringLiteral("automation.get_options"),
                        QJsonObject{
                            {QStringLiteral("operation_id"),      QStringLiteral("tracks.set_voice")},
                            {QStringLiteral("field_path"),        QStringLiteral("/voice/singer")   },
                            {QStringLiteral("partial_arguments"), QJsonObject{}                     },
    },
                        {.clientId = QStringLiteral("singer-reference-options")});
    const auto speakerOptions =
        registry.invoke(QStringLiteral("automation.get_options"),
                        QJsonObject{
                            {QStringLiteral("operation_id"),      QStringLiteral("tracks.set_voice")},
                            {QStringLiteral("field_path"),        QStringLiteral("/voice/speaker")  },
                            {QStringLiteral("partial_arguments"),
                             QJsonObject{
                                 {QStringLiteral("voice"),
                                  QJsonObject{
                                      {QStringLiteral("singer"), registrySingerRef},
                                  }},
                             }                                                                      },
    },
                        {.clientId = QStringLiteral("speaker-reference-options")});
    const auto speakerV2Options =
        registry.invoke(QStringLiteral("automation.get_options"),
                        QJsonObject{
                            {QStringLiteral("operation_id"),      QStringLiteral("tracks.set_voice")},
                            {QStringLiteral("field_path"),        QStringLiteral("/voice/speaker")  },
                            {QStringLiteral("partial_arguments"),
                             QJsonObject{
                                 {QStringLiteral("voice"),
                                  QJsonObject{
                                      {QStringLiteral("singer"), registrySingerV2Ref},
                                  }},
                             }                                                                      },
    },
                        {.clientId = QStringLiteral("speaker-v2-reference-options")});
    const auto singerOptionValues =
        singerOptions ? singerOptions.get().value(QStringLiteral("options")).toArray()
                      : QJsonArray{};
    const auto speakerOptionValues =
        speakerOptions ? speakerOptions.get().value(QStringLiteral("options")).toArray()
                       : QJsonArray{};
    const auto speakerV2OptionValues =
        speakerV2Options ? speakerV2Options.get().value(QStringLiteral("options")).toArray()
                         : QJsonArray{};
    const auto hasOptionValue = [](const QJsonArray &options, const QJsonObject &expected) {
        return std::any_of(options.cbegin(), options.cend(), [&](const QJsonValue &value) {
            return value.toObject().value(QStringLiteral("value")).toObject() == expected;
        });
    };
    expect(singerOptions && singerOptionValues.size() == 3 &&
               hasOptionValue(singerOptionValues, registrySingerRef) &&
               hasOptionValue(singerOptionValues, registrySingerV2Ref) && speakerOptions &&
               speakerOptionValues.size() == 2 &&
               speakerOptionValues.first()
                   .toObject()
                   .value(QStringLiteral("value"))
                   .toObject()
                   .contains(QStringLiteral("speaker_id")) &&
               speakerV2Options && speakerV2OptionValues.size() == 2 &&
               speakerV2OptionValues.first()
                       .toObject()
                       .value(QStringLiteral("value"))
                       .toObject()
                       .value(QStringLiteral("speaker_id")) == QStringLiteral("speaker-v2"),
           QStringLiteral("get_options must distinguish same-ID singer versions and return their "
                          "own speakers"));

    const auto metadataTask = runtime.automationTasks().createTask(QStringLiteral("documents.open"),
                                                                   runtime.documentVersion());
    Automation::MutationResult metadataMutation;
    metadataMutation.previous = runtime.documentVersion();
    metadataMutation.current = runtime.documentVersion();
    metadataMutation.changed = true;
    metadataMutation.resolvedValues.append(
        {QStringLiteral("/tracks/0/name"), QStringLiteral("New Track")});
    metadataMutation.presentationEffects.append(QStringLiteral("active_clip_changed"));
    runtime.automationTasks().markRunning(metadataTask.taskId);
    runtime.automationTasks().beginCommitting(metadataTask.taskId);
    runtime.automationTasks().succeed(metadataTask.taskId, metadataMutation);
    const auto encodedMetadataTask = registry.invoke(
        QStringLiteral("tasks.get"),
        QJsonObject{
            {QStringLiteral("scope"),       QStringLiteral("document")                     },
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
            {QStringLiteral("task_id"),     metadataTask.taskId.toString()                 },
    });
    const auto encodedMetadataMutation =
        encodedMetadataTask ? encodedMetadataTask.get().value(QStringLiteral("result")).toObject()
                            : QJsonObject{};
    expect(
        encodedMetadataTask &&
            resolvedValue(encodedMetadataMutation, QStringLiteral("/tracks/0/name")) ==
                QStringLiteral("New Track") &&
            encodedMetadataMutation.value(QStringLiteral("presentation_effects")).toArray() ==
                QJsonArray{QStringLiteral("active_clip_changed")},
        QStringLiteral("task Mutation results must preserve resolved and presentation metadata"));
    runtime.automationTasks().discardDocumentGeneration(runtime.documentVersion().documentId);

    const auto formats = registry.invoke(QStringLiteral("formats.list"), {},
                                         {.clientId = QStringLiteral("format-schema")});
    const auto formatItems =
        formats ? formats.get().value(QStringLiteral("formats")).toArray() : QJsonArray{};
    const auto formatSchema =
        formatItems.isEmpty()
            ? QJsonObject{}
            : formatItems.first().toObject().value(QStringLiteral("option_schema")).toObject();
    const auto explicitEmptyOpen = QJsonObject{
        {QStringLiteral("operation_id"), QStringLiteral("documents.open")},
        {QStringLiteral("options"),      QJsonObject{}                   },
    };
    const auto midiOptions = QJsonObject{
        {QStringLiteral("operation_id"), QStringLiteral("exports.midi.start")},
        {QStringLiteral("options"),
         QJsonObject{{QStringLiteral("include_tempo"), true},
                     {QStringLiteral("include_time_signatures"), false}}     },
    };
    auto invalidOpenOptions = explicitEmptyOpen;
    invalidOpenOptions.insert(QStringLiteral("options"), QJsonObject{
                                                             {QStringLiteral("unexpected"), true}
    });
    expect(formats && formatItems.size() == 1 &&
               AutomationWire::checkJsonSchema(formatSchema).valid() &&
               AutomationWire::validateJsonValue(explicitEmptyOpen, formatSchema).valid() &&
               AutomationWire::validateJsonValue(midiOptions, formatSchema).valid() &&
               !AutomationWire::validateJsonValue(invalidOpenOptions, formatSchema).valid(),
           QStringLiteral("formats.list must expose strict operation-sourced schemas, including "
                          "explicit empty options"));

    auto saveCurrent = commandArguments(runtime.documentVersion());
    const auto saveWithoutCurrentPath =
        registry.invoke(QStringLiteral("documents.save"), saveCurrent,
                        {.clientId = QStringLiteral("save-current-no-path")});
    expect(!saveWithoutCurrentPath &&
               saveWithoutCurrentPath.getError().code ==
                   Automation::AutomationErrorCode::PathRequired &&
               saveWithoutCurrentPath.getError().fieldPath == QStringLiteral("path"),
           QStringLiteral("save-current must reject a document without an existing path"));

    const auto strictInput = registry.invoke(QStringLiteral("automation.get_file_access"),
                                             QJsonObject{
                                                 {QStringLiteral("unexpected"), true}
    },
                                             {.clientId = QStringLiteral("strict")});
    expect(!strictInput &&
               strictInput.getError().code == Automation::AutomationErrorCode::InvalidArgument,
           QStringLiteral("strict input schema must reject additional properties"));

    auto project = runtime.project().getProject(runtime.documentVersion().documentId);
    expect(bool(project), QStringLiteral("project query must succeed"));
    if (!project || project.get().tracks.isEmpty()) {
        Automation::TrackDraftDto track;
        track.name = QStringLiteral("Automation Track");
        track.defaultLanguage = QStringLiteral("en");
        const auto inserted = runtime.project().insertTrack(
            {.expected = runtime.documentVersion(),
             .source = Automation::InvocationSource::InternalAutomation},
            0, track);
        expect(bool(inserted), QStringLiteral("test track setup must succeed"));
        project = runtime.project().getProject(runtime.documentVersion().documentId);
    }
    const auto trackId = project.get().tracks.first().id;
    const auto mixedInferenceScope = registry.invoke(
        QStringLiteral("inference.get_capabilities"),
        QJsonObject{
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
            {QStringLiteral("scope"),
             QJsonObject{
                 {QStringLiteral("kind"), QStringLiteral("track")},
                 {QStringLiteral("track_ids"), QJsonArray{trackId.value(), 999999}},
             }                                                                             },
    });
    expect(!mixedInferenceScope &&
               mixedInferenceScope.getError().code == Automation::AutomationErrorCode::NotFound,
           QStringLiteral("inference scope must reject mixed valid and missing target IDs"));

    const auto makeReadableAudio = [&](const QString &name) {
        QFile file(directory.filePath(name));
        const auto ready = file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                           file.write("audio-fixture") == 13;
        file.close();
        expect(ready, QStringLiteral("audio path fixture %1 must be available").arg(name));
        return file.fileName();
    };
    const auto originalAudioPath = makeReadableAudio(QStringLiteral("original.wav"));
    const auto relocatedAudioPath = makeReadableAudio(QStringLiteral("relocated.wav"));
    const auto confirmedAudioPath = makeReadableAudio(QStringLiteral("confirmed.wav"));
    const auto deniedAudioPath = makeReadableAudio(QStringLiteral("denied.wav"));
    const auto projectPath = directory.filePath(QStringLiteral("audio-candidates.dspx"));
    const auto savedForAudioCandidates = runtime.documents().saveDocument(
        {.expected = runtime.documentVersion(),
         .source = Automation::InvocationSource::InternalAutomation},
        projectPath, true);
    expect(bool(savedForAudioCandidates),
           QStringLiteral("audio candidate test must establish a project directory"));
    Automation::ClipDraftDto audioDraft;
    audioDraft.clientRef = QStringLiteral("registry-audio-clip");
    audioDraft.type = Automation::ClipDraftDto::Type::Audio;
    audioDraft.properties.name = QStringLiteral("Registry Audio");
    audioDraft.properties.length = 960;
    audioDraft.properties.clipLen = 960;
    audioDraft.properties.gain = 1.0;
    audioDraft.audioPath = originalAudioPath;
    audioDraft.audioPathInfo.relativeDir = QStringLiteral("assets");
    audioDraft.audioPathInfo.sha512 = QStringLiteral("stale-sha512");
    audioDraft.workspace.insert(
        QStringLiteral("diffscope.audio.formatData"),
        QJsonObject{
            {QStringLiteral("entryClassName"), QStringLiteral("StaleFormatEntry")}
    });
    const auto insertedAudio = runtime.project().insertClips(
        {
            .expected = runtime.documentVersion(),
            .source = Automation::InvocationSource::InternalAutomation
    },
        {{trackId, audioDraft}});
    expect(insertedAudio && !insertedAudio.get().createdObjects.isEmpty(),
           QStringLiteral("public audio path test clip must be inserted"));
    const auto audioClipId =
        insertedAudio && !insertedAudio.get().createdObjects.isEmpty()
            ? Automation::ClipId(insertedAudio.get().createdObjects.first().object.value)
            : Automation::ClipId();
    auto *audioClip = dynamic_cast<AudioClip *>(fixture.model().findClipById(audioClipId.value()));
    expect(audioClip != nullptr, QStringLiteral("public audio path test clip must be addressable"));
    const auto audioSnapshot = registry.invoke(
        QStringLiteral("audio_clips.get"),
        QJsonObject{
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
            {QStringLiteral("clip_id"),     audioClipId.value()                            },
    });
    const auto candidatePaths = audioSnapshot ? audioSnapshot.get()
                                                    .value(QStringLiteral("snapshot"))
                                                    .toObject()
                                                    .value(QStringLiteral("candidate_paths"))
                                                    .toArray()
                                              : QJsonArray{};
    expect(audioSnapshot &&
               candidatePaths ==
                   QJsonArray{
                       QDir(directory.path()).filePath(QStringLiteral("assets/original.wav")),
                       originalAudioPath,
                   },
           QStringLiteral("audio clip candidates must be confirmable absolute file paths in GUI "
                          "resolution order"));
    const auto audioClipDetail = registry.invoke(
        QStringLiteral("clips.get"),
        QJsonObject{
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
            {QStringLiteral("clip_id"),     audioClipId.value()                            },
    });
    expect(audioClipDetail && audioClipDetail.get()
                                  .value(QStringLiteral("snapshot"))
                                  .toObject()
                                  .value(QStringLiteral("voice_context"))
                                  .isNull(),
           QStringLiteral("clips.get must mark voice context unavailable for audio clips"));

    const auto extractionLanguages = registry.invoke(
        QStringLiteral("automation.get_options"),
        QJsonObject{
            {QStringLiteral("operation_id"),      QStringLiteral("extract.midi.start")       },
            {QStringLiteral("field_path"),        QStringLiteral("/options/default_language")},
            {QStringLiteral("partial_arguments"),
             QJsonObject{
                 {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
                 {QStringLiteral("source_audio_clip_id"), audioClipId.value()},
             }                                                                               },
    },
        {.clientId = QStringLiteral("extraction-language-options")});
    const auto extractionLanguageOptions =
        extractionLanguages ? extractionLanguages.get().value(QStringLiteral("options")).toArray()
                            : QJsonArray{};
    expect(extractionLanguages && extractionLanguageOptions.size() == 1 &&
               extractionLanguageOptions.first().toObject().value(QStringLiteral("value")) ==
                   QStringLiteral("zh"),
           QStringLiteral(
               "MIDI extraction language options must resolve from extraction capabilities"));

    audioPathPreparationCount = 0;
    lastPreparedAudioPath.clear();
    auto relocatePreviewInput = commandArguments(runtime.documentVersion());
    relocatePreviewInput.insert(QStringLiteral("clip_id"), audioClipId.value());
    relocatePreviewInput.insert(QStringLiteral("path"), relocatedAudioPath);
    relocatePreviewInput.insert(QStringLiteral("validate_only"), true);
    const auto pathBeforePreview = audioClip ? audioClip->path() : QString();
    const auto revisionBeforePreview = runtime.documentVersion();
    const auto relocatePreview =
        registry.invoke(QStringLiteral("audio_clips.relocate"), relocatePreviewInput,
                        {.clientId = QStringLiteral("audio-relocate-preview")});
    expect(!relocatePreview &&
               relocatePreview.getError().code ==
                   Automation::AutomationErrorCode::InvalidArgument &&
               audioClip && audioClip->path() == pathBeforePreview &&
               runtime.documentVersion() == revisionBeforePreview,
           QStringLiteral("audio relocation must reject unsupported validate-only input"));

    auto relocateInput = commandArguments(runtime.documentVersion());
    relocateInput.insert(QStringLiteral("clip_id"), audioClipId.value());
    relocateInput.insert(QStringLiteral("path"), relocatedAudioPath);
    const auto relocateBase = runtime.documentVersion();
    const auto relocated = registry.invoke(QStringLiteral("audio_clips.relocate"), relocateInput,
                                           {.clientId = QStringLiteral("audio-relocate")});
    const auto relocateFormat =
        audioClip ? audioClip->workspace().value(QStringLiteral("diffscope.audio.formatData"))
                  : QJsonObject{};
    expect(
        relocated && audioClip && runtime.documentVersion().revision == relocateBase.revision + 1 &&
            audioClip->path() == QFileInfo(relocatedAudioPath).canonicalFilePath() &&
            audioClip->pathInfo().sha512 == QStringLiteral("prepared-sha512-1") &&
            relocateFormat.value(QStringLiteral("entryClassName")) ==
                QStringLiteral("PreparedFormatEntry") &&
            relocateFormat.value(QStringLiteral("userData")) == QStringLiteral("1"),
        QStringLiteral(
            "audio relocation must atomically commit freshly prepared hash and format metadata"));

    if (audioClip) {
        runtime.project().setAudioClipPathStatus(
            {.expected = runtime.documentVersion(),
             .source = Automation::InvocationSource::InternalAutomation},
            audioClipId, Automation::audioAssetSnapshotDto(*audioClip),
            AudioClip::PathStatus::Unconfirmed);
    }
    auto confirmInput = commandArguments(runtime.documentVersion());
    confirmInput.insert(QStringLiteral("clip_id"), audioClipId.value());
    confirmInput.insert(QStringLiteral("path"), confirmedAudioPath);
    const auto confirmBase = runtime.documentVersion();
    const auto confirmed = registry.invoke(QStringLiteral("audio_clips.confirm_path"), confirmInput,
                                           {.clientId = QStringLiteral("audio-confirm")});
    const auto confirmFormat =
        audioClip ? audioClip->workspace().value(QStringLiteral("diffscope.audio.formatData"))
                  : QJsonObject{};
    expect(
        confirmed && audioClip && runtime.documentVersion().revision == confirmBase.revision + 1 &&
            audioClip->path() == QFileInfo(confirmedAudioPath).canonicalFilePath() &&
            audioClip->pathInfo().sha512 == QStringLiteral("prepared-sha512-2") &&
            confirmFormat.value(QStringLiteral("userData")) == QStringLiteral("2") &&
            audioClip->pathStatus() == AudioClip::PathStatus::Normal,
        QStringLiteral("audio path confirmation must perform one prepared atomic History commit"));

    if (audioClip) {
        runtime.project().setAudioClipPathStatus(
            {.expected = runtime.documentVersion(),
             .source = Automation::InvocationSource::InternalAutomation},
            audioClipId, Automation::audioAssetSnapshotDto(*audioClip),
            AudioClip::PathStatus::Unconfirmed);
    }
    auto confirmCurrentInput = commandArguments(runtime.documentVersion());
    confirmCurrentInput.insert(QStringLiteral("clip_id"), audioClipId.value());
    const auto confirmCurrentBase = runtime.documentVersion();
    const auto confirmedCurrent =
        registry.invoke(QStringLiteral("audio_clips.confirm_path"), confirmCurrentInput,
                        {.clientId = QStringLiteral("audio-confirm-current")});
    const auto confirmedCurrentApplied =
        audioClip && audioClip->pathStatus() == AudioClip::PathStatus::Normal &&
        audioClip->pathInfo().sha512 == QStringLiteral("prepared-sha512-3");
    const auto undoConfirmCurrent =
        registry.invoke(QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
                        {.clientId = QStringLiteral("audio-confirm-current-undo")});
    expect(confirmedCurrent && confirmedCurrent.get().value(QStringLiteral("changed")).toBool() &&
               confirmedCurrentApplied && audioClip &&
               audioClip->path() == QFileInfo(confirmedAudioPath).canonicalFilePath() &&
               audioClip->pathInfo().sha512 == QStringLiteral("prepared-sha512-2") &&
               audioPathPreparationCount == 3 &&
               lastPreparedAudioPath == QFileInfo(confirmedAudioPath).canonicalFilePath() &&
               undoConfirmCurrent &&
               runtime.documentVersion().revision == confirmCurrentBase.revision + 2 &&
               audioClip->pathStatus() == AudioClip::PathStatus::Unconfirmed,
           QStringLiteral("confirming the current candidate must prepare and undo the real path"));

    failAudioPathPreparation = true;
    auto failedPrepareInput = commandArguments(runtime.documentVersion());
    failedPrepareInput.insert(QStringLiteral("clip_id"), audioClipId.value());
    failedPrepareInput.insert(QStringLiteral("path"), relocatedAudioPath);
    const auto beforeFailedPrepare = Automation::audioAssetSnapshotDto(*audioClip);
    const auto failedPrepare =
        registry.invoke(QStringLiteral("audio_clips.relocate"), failedPrepareInput,
                        {.clientId = QStringLiteral("audio-prepare-failure")});
    expect(!failedPrepare &&
               failedPrepare.getError().code == Automation::AutomationErrorCode::IoError &&
               failedPrepare.getError().fieldPath == QStringLiteral("path") && audioClip &&
               Automation::audioAssetSnapshotDto(*audioClip) == beforeFailedPrepare,
           QStringLiteral("audio decoding failure must be stable and leave the clip unchanged"));
    failAudioPathPreparation = false;

    revokeAudioPathAccessDuringPreparation = true;
    auto revokedInput = commandArguments(runtime.documentVersion());
    revokedInput.insert(QStringLiteral("clip_id"), audioClipId.value());
    revokedInput.insert(QStringLiteral("path"), deniedAudioPath);
    const auto beforeRevoked = runtime.documentVersion();
    const auto revoked = registry.invoke(QStringLiteral("audio_clips.confirm_path"), revokedInput,
                                         {.clientId = QStringLiteral("audio-revoked")});
    expect(!revoked &&
               revoked.getError().code == Automation::AutomationErrorCode::PermissionDenied &&
               runtime.documentVersion() == beforeRevoked && audioClip &&
               audioClip->path() == QFileInfo(confirmedAudioPath).canonicalFilePath(),
           QStringLiteral(
               "audio path commit must reauthorize after preparation and reject revoked access"));
    revokeAudioPathAccessDuringPreparation = false;
    expect(bool(fileGuard.setConfiguredRoots({directory.path()}, {directory.path()})),
           QStringLiteral("file guard roots must restore after revocation test"));

    const QJsonObject audioExportOptions{
        {QStringLiteral("format"),       QStringLiteral("wav")      },
        {QStringLiteral("sample_rate"),  44100                      },
        {QStringLiteral("channel_mode"), QStringLiteral("stereo")   },
        {QStringLiteral("mixing_mode"),  QStringLiteral("mixed")    },
        {QStringLiteral("source"),       QStringLiteral("custom")   },
        {QStringLiteral("source_ids"),   QJsonArray{trackId.value()}},
    };
    const auto audioPreview = registry.invoke(
        QStringLiteral("exports.audio.preview"),
        QJsonObject{
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()  },
            {QStringLiteral("path"),        directory.filePath(QStringLiteral("preview.wav"))},
            {QStringLiteral("options"),     audioExportOptions                               },
    },
        {.clientId = QStringLiteral("audio-preview")});
    const auto audioPreviewPlan =
        audioPreview ? audioPreview.get().value(QStringLiteral("plan")).toObject() : QJsonObject{};
    const auto audioDiagnostics = audioPreviewPlan.value(QStringLiteral("diagnostics")).toArray();
    expect(audioPreview && capturedAudioConfig->fileName == QStringLiteral("preview.wav") &&
               capturedAudioConfig->sources == QList<int>{0} &&
               audioPreviewPlan.value(QStringLiteral("targets")).toArray().size() == 1 &&
               audioDiagnostics.size() == 2 &&
               audioDiagnostics.first().toObject().value(QStringLiteral("code")) ==
                   QStringLiteral("duplicate_paths") &&
               audioDiagnostics.last().toObject().value(QStringLiteral("code")) ==
                   QStringLiteral("lossy_format") &&
               !audioPreviewPlan.contains(QStringLiteral("warning_flags")),
           QStringLiteral("audio preview must expose a typed plan and diagnostics while mapping "
                          "TrackId to backend index"));
    QJsonObject audioStart{
        {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()}
    };
    audioStart.insert(QStringLiteral("path"),
                      directory.filePath(QStringLiteral("registry-export.wav")));
    audioStart.insert(QStringLiteral("options"), audioExportOptions);
    audioStart.insert(QStringLiteral("overwrite_policy"), QStringLiteral("reject"));
    const auto audioStartPreview =
        registry.invoke(QStringLiteral("exports.audio.start"), audioStart,
                        {.clientId = QStringLiteral("audio-start-preview")});
    expect(
        audioStartPreview &&
            capturedAudioConfig->fileName == QStringLiteral("registry-export.wav") &&
            capturedAudioConfig->sources == QList<int>{0},
        QStringLiteral(
            "audio start must preserve the extension and resolve stable TrackIds before backend"));

    auto mutationInput = commandArguments(runtime.documentVersion());
    mutationInput.insert(QStringLiteral("track_id"), trackId.value());
    mutationInput.insert(QStringLiteral("color_index"), 1);
    const auto revisionBeforeMutation = runtime.documentVersion().revision;
    const auto mutation = registry.invoke(QStringLiteral("tracks.set_color"), mutationInput,
                                          {.clientId = QStringLiteral("mutation")});
    const auto history = runtime.history().getState(runtime.documentVersion().documentId);
    expect(mutation && runtime.documentVersion().revision == revisionBeforeMutation + 1 &&
               history && history.get().canUndo,
           QStringLiteral("representative command must create one revision and History entry"));
    const auto stale = registry.invoke(QStringLiteral("tracks.set_color"), mutationInput,
                                       {.clientId = QStringLiteral("revision")});
    expect(!stale && stale.getError().code == Automation::AutomationErrorCode::RevisionConflict,
           QStringLiteral("stale public command must report revision_conflict"));

    Automation::ClipDraftDto singing;
    singing.clientRef = QStringLiteral("registry-parameter-clip");
    singing.type = Automation::ClipDraftDto::Type::Singing;
    singing.properties.name = QStringLiteral("Singing");
    singing.properties.length = 1920;
    singing.properties.clipLen = 1920;
    singing.properties.gain = 1.0;
    singing.defaultLanguage = QStringLiteral("unknown");
    const auto clipMutation = runtime.project().insertClips(
        {
            .expected = runtime.documentVersion(),
            .source = Automation::InvocationSource::InternalAutomation
    },
        {{trackId, singing}});
    expect(bool(clipMutation) && !clipMutation.get().createdObjects.isEmpty(),
           QStringLiteral("singing clip setup must succeed"));
    const auto clipId = Automation::ClipId(clipMutation.get().createdObjects.first().object.value);

    const auto capabilities = registry.invoke(
        QStringLiteral("parameters.get_capabilities"),
        QJsonObject{
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
            {QStringLiteral("clip_id"),     clipId.value()                                 }
    },
        {.clientId = QStringLiteral("range-capabilities")});
    QJsonObject pitchRange;
    if (!capabilities) {
        QTextStream(stderr) << "DETAIL parameters.get_capabilities: "
                            << Automation::errorCodeName(capabilities.getError().code) << ", "
                            << capabilities.getError().fieldPath << ", "
                            << capabilities.getError().message << Qt::endl;
    }
    if (capabilities) {
        for (const auto &value : capabilities.get()
                                     .value(QStringLiteral("capabilities"))
                                     .toObject()
                                     .value(QStringLiteral("parameters"))
                                     .toArray()) {
            const auto parameter = value.toObject();
            if (parameter.value(QStringLiteral("name")) == QStringLiteral("pitch")) {
                pitchRange = parameter.value(QStringLiteral("range")).toObject();
                break;
            }
        }
    }
    expect(capabilities && !pitchRange.isEmpty(),
           QStringLiteral("parameter capability must expose the pitch range"));
    const auto minimum = pitchRange.value(QStringLiteral("minimum")).toInt();
    const auto maximum = pitchRange.value(QStringLiteral("maximum")).toInt();
    auto boundaryInput = commandArguments(runtime.documentVersion());
    boundaryInput.insert(QStringLiteral("clip_id"), clipId.value());
    boundaryInput.insert(QStringLiteral("name"), QStringLiteral("pitch"));
    boundaryInput.insert(QStringLiteral("layer"), QStringLiteral("edited"));
    boundaryInput.insert(QStringLiteral("local_start"), 0);
    boundaryInput.insert(QStringLiteral("step"), 5);
    boundaryInput.insert(QStringLiteral("values"), QJsonArray{minimum, maximum});
    const auto beforeBoundary = runtime.documentVersion();
    const auto boundary = registry.invoke(QStringLiteral("parameters.draw"), boundaryInput,
                                          {.clientId = QStringLiteral("range-boundary")});
    expect(boundary && runtime.documentVersion().revision == beforeBoundary.revision + 1,
           QStringLiteral("capability-declared parameter minimum and maximum must be accepted"));

    auto offStepInput = commandArguments(runtime.documentVersion());
    offStepInput.insert(QStringLiteral("clip_id"), clipId.value());
    offStepInput.insert(QStringLiteral("name"), QStringLiteral("pitch"));
    offStepInput.insert(QStringLiteral("layer"), QStringLiteral("edited"));
    offStepInput.insert(QStringLiteral("local_start"), 10);
    offStepInput.insert(QStringLiteral("step"), 5);
    offStepInput.insert(QStringLiteral("values"), QJsonArray{minimum + 0.5});
    const auto beforeOffStep = runtime.documentVersion();
    const auto offStep = registry.invoke(QStringLiteral("parameters.draw"), offStepInput,
                                         {.clientId = QStringLiteral("range-off-step")});
    expect(!offStep &&
               offStep.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
               runtime.documentVersion() == beforeOffStep,
           QStringLiteral("off-step parameter values must fail before revision or History change"));

    const auto beforeRangeFailure = runtime.documentVersion();
    auto rangeInput = commandArguments(beforeRangeFailure);
    rangeInput.insert(QStringLiteral("clip_id"), clipId.value());
    rangeInput.insert(QStringLiteral("name"), QStringLiteral("pitch"));
    rangeInput.insert(QStringLiteral("layer"), QStringLiteral("edited"));
    rangeInput.insert(QStringLiteral("local_start"), 0);
    rangeInput.insert(QStringLiteral("step"), 5);
    rangeInput.insert(QStringLiteral("values"), QJsonArray{maximum + 1, maximum + 1});
    const auto rangeFailure = registry.invoke(QStringLiteral("parameters.draw"), rangeInput,
                                              {.clientId = QStringLiteral("range")});
    expect(!rangeFailure &&
               rangeFailure.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
               runtime.documentVersion() == beforeRangeFailure,
           QStringLiteral(
               "capability range gate must reject values without revision or History change"));

    auto createAnchorInput = commandArguments(runtime.documentVersion());
    createAnchorInput.insert(QStringLiteral("clip_id"), clipId.value());
    createAnchorInput.insert(QStringLiteral("name"), QStringLiteral("pitch"));
    createAnchorInput.insert(QStringLiteral("layer"), QStringLiteral("edited"));
    createAnchorInput.insert(QStringLiteral("client_ref"), QStringLiteral("anchor-curve-a"));
    createAnchorInput.insert(
        QStringLiteral("anchors"),
        QJsonArray{
            QJsonObject{{QStringLiteral("position"), 100},
                        {QStringLiteral("value"), minimum},
                        {QStringLiteral("interpolation"), QStringLiteral("linear")}},
            QJsonObject{{QStringLiteral("position"), 200},
                        {QStringLiteral("value"), minimum},
                        {QStringLiteral("interpolation"), QStringLiteral("step")}  },
    });
    const auto createdAnchorCurve =
        registry.invoke(QStringLiteral("parameters.create_anchor_curve"), createAnchorInput,
                        {.clientId = QStringLiteral("anchor-curve-create")});
    reportFailure(QStringLiteral("parameters.create_anchor_curve"), createdAnchorCurve);
    const auto createdCurveIds =
        createdAnchorCurve ? createdObjectIds(createdAnchorCurve.get(), QStringLiteral("curve"))
                           : QList<int>{};
    expect(createdAnchorCurve && createdCurveIds.size() == 1,
           QStringLiteral("anchor curve creation must return one stable curve ID"));
    const auto anchorCurveId = createdCurveIds.value(0, -1);
    auto insertAnchorInput = commandArguments(runtime.documentVersion());
    insertAnchorInput.insert(QStringLiteral("clip_id"), clipId.value());
    insertAnchorInput.insert(QStringLiteral("name"), QStringLiteral("pitch"));
    insertAnchorInput.insert(QStringLiteral("layer"), QStringLiteral("edited"));
    insertAnchorInput.insert(QStringLiteral("curve_id"), anchorCurveId);
    insertAnchorInput.insert(
        QStringLiteral("anchors"),
        QJsonArray{
            QJsonObject{{QStringLiteral("position"), 150},
                        {QStringLiteral("value"), minimum},
                        {QStringLiteral("interpolation"), QStringLiteral("linear")}}
    });
    const auto insertedAnchor =
        registry.invoke(QStringLiteral("parameters.insert_anchors"), insertAnchorInput,
                        {.clientId = QStringLiteral("anchor-insert-explicit")});
    reportFailure(QStringLiteral("parameters.insert_anchors"), insertedAnchor);
    expect(insertedAnchor && insertedAnchor.get().value(QStringLiteral("changed")).toBool(),
           QStringLiteral("anchor insertion must target an explicit existing curve"));
    const auto anchorSnapshot = registry.invoke(
        QStringLiteral("parameters.get"),
        QJsonObject{
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
            {QStringLiteral("clip_id"),     clipId.value()                                 },
            {QStringLiteral("name"),        QStringLiteral("pitch")                        },
            {QStringLiteral("layer"),       QStringLiteral("edited")                       },
    },
        {.clientId = QStringLiteral("anchor-snapshot")});
    int movingAnchorId = -1;
    if (anchorSnapshot) {
        for (const auto &curveValue : anchorSnapshot.get()
                                          .value(QStringLiteral("snapshot"))
                                          .toObject()
                                          .value(QStringLiteral("curves"))
                                          .toArray()) {
            const auto curve = curveValue.toObject();
            if (curve.value(QStringLiteral("type")) != QStringLiteral("anchor"))
                continue;
            const auto nodes = curve.value(QStringLiteral("nodes")).toArray();
            if (nodes.size() >= 2)
                movingAnchorId =
                    nodes.first().toObject().value(QStringLiteral("anchor_id")).toInt();
        }
    }
    expect(movingAnchorId >= 0,
           QStringLiteral("anchor overlap setup must expose stable anchor IDs"));
    auto moveOverlapInput = commandArguments(runtime.documentVersion());
    moveOverlapInput.insert(QStringLiteral("clip_id"), clipId.value());
    moveOverlapInput.insert(QStringLiteral("name"), QStringLiteral("pitch"));
    moveOverlapInput.insert(QStringLiteral("layer"), QStringLiteral("edited"));
    moveOverlapInput.insert(QStringLiteral("moves"),
                            QJsonArray{
                                QJsonObject{{QStringLiteral("anchor_id"), movingAnchorId},
                                            {QStringLiteral("position"), 200},
                                            {QStringLiteral("value"), minimum}}
    });
    const auto beforeOverlap = runtime.documentVersion();
    const auto historyBeforeOverlap = runtime.history().getState(beforeOverlap.documentId);
    const auto moveOverlap =
        registry.invoke(QStringLiteral("parameters.move_anchors"), moveOverlapInput,
                        {.clientId = QStringLiteral("anchor-overlap")});
    const auto historyAfterOverlap =
        runtime.history().getState(runtime.documentVersion().documentId);
    expect(
        !moveOverlap &&
            moveOverlap.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
            moveOverlap.getError().fieldPath == QStringLiteral("moves.position") &&
            runtime.documentVersion() == beforeOverlap && historyBeforeOverlap &&
            historyAfterOverlap &&
            historyBeforeOverlap.get().undoName == historyAfterOverlap.get().undoName &&
            historyBeforeOverlap.get().redoName == historyAfterOverlap.get().redoName,
        QStringLiteral("moving an anchor onto another anchor must not change revision or History"));

    auto denseDrawInput = commandArguments(runtime.documentVersion());
    denseDrawInput.insert(QStringLiteral("clip_id"), clipId.value());
    denseDrawInput.insert(QStringLiteral("name"), QStringLiteral("pitch"));
    denseDrawInput.insert(QStringLiteral("layer"), QStringLiteral("edited"));
    denseDrawInput.insert(QStringLiteral("local_start"), 600);
    denseDrawInput.insert(QStringLiteral("step"), 5);
    denseDrawInput.insert(QStringLiteral("values"),
                          QJsonArray{minimum, minimum, minimum, minimum, minimum, minimum, minimum,
                                     minimum, minimum, minimum});
    const auto denseDraw = registry.invoke(QStringLiteral("parameters.draw"), denseDrawInput,
                                           {.clientId = QStringLiteral("bounded-draw-setup")});
    const auto boundedParameter = registry.invoke(
        QStringLiteral("parameters.get"),
        QJsonObject{
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
            {QStringLiteral("clip_id"),     clipId.value()                                 },
            {QStringLiteral("name"),        QStringLiteral("pitch")                        },
            {QStringLiteral("layer"),       QStringLiteral("edited")                       },
            {QStringLiteral("range"),
             QJsonObject{{QStringLiteral("start"), 600}, {QStringLiteral("end"), 650}}     },
            {QStringLiteral("max_points"),  3                                              },
    },
        {.clientId = QStringLiteral("bounded-parameter-query")});
    const auto boundedSnapshot =
        boundedParameter ? boundedParameter.get().value(QStringLiteral("snapshot")).toObject()
                         : QJsonObject{};
    expect(denseDraw && boundedParameter &&
               boundedSnapshot.value(QStringLiteral("source_point_count")).toInt() == 10 &&
               boundedSnapshot.value(QStringLiteral("returned_point_count")).toInt() <= 3 &&
               boundedSnapshot.value(QStringLiteral("downsampled")).toBool(),
           QStringLiteral("parameters.get must bound and report a ranged draw projection"));
    const auto undersizedAnchorQuery = registry.invoke(
        QStringLiteral("parameters.get"),
        QJsonObject{
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
            {QStringLiteral("clip_id"),     clipId.value()                                 },
            {QStringLiteral("name"),        QStringLiteral("pitch")                        },
            {QStringLiteral("layer"),       QStringLiteral("edited")                       },
            {QStringLiteral("range"),
             QJsonObject{{QStringLiteral("start"), 90}, {QStringLiteral("end"), 250}}      },
            {QStringLiteral("max_points"),  2                                              },
    },
        {.clientId = QStringLiteral("bounded-anchor-query")});
    expect(!undersizedAnchorQuery &&
               undersizedAnchorQuery.getError().code ==
                   Automation::AutomationErrorCode::InvalidArgument &&
               undersizedAnchorQuery.getError().fieldPath == QStringLiteral("max_points"),
           QStringLiteral("parameters.get must never discard stable anchor IDs to meet a bound"));

    auto secondCurveInput = commandArguments(runtime.documentVersion());
    secondCurveInput.insert(QStringLiteral("clip_id"), clipId.value());
    secondCurveInput.insert(QStringLiteral("name"), QStringLiteral("pitch"));
    secondCurveInput.insert(QStringLiteral("layer"), QStringLiteral("edited"));
    secondCurveInput.insert(QStringLiteral("client_ref"), QStringLiteral("anchor-curve-b"));
    secondCurveInput.insert(
        QStringLiteral("anchors"),
        QJsonArray{
            QJsonObject{{QStringLiteral("position"), 400},
                        {QStringLiteral("value"), minimum},
                        {QStringLiteral("interpolation"), QStringLiteral("linear")}},
            QJsonObject{{QStringLiteral("position"), 500},
                        {QStringLiteral("value"), minimum},
                        {QStringLiteral("interpolation"), QStringLiteral("step")}  },
    });
    const auto secondCurve =
        registry.invoke(QStringLiteral("parameters.create_anchor_curve"), secondCurveInput,
                        {.clientId = QStringLiteral("anchor-curve-second")});
    reportFailure(QStringLiteral("parameters.create_anchor_curve(second)"), secondCurve);
    const auto secondCurveIds =
        secondCurve ? createdObjectIds(secondCurve.get(), QStringLiteral("curve")) : QList<int>{};
    auto crossCurveMoveInput = commandArguments(runtime.documentVersion());
    crossCurveMoveInput.insert(QStringLiteral("clip_id"), clipId.value());
    crossCurveMoveInput.insert(QStringLiteral("name"), QStringLiteral("pitch"));
    crossCurveMoveInput.insert(QStringLiteral("layer"), QStringLiteral("edited"));
    crossCurveMoveInput.insert(QStringLiteral("moves"),
                               QJsonArray{
                                   QJsonObject{{QStringLiteral("anchor_id"), movingAnchorId},
                                               {QStringLiteral("position"), 450},
                                               {QStringLiteral("value"), minimum}}
    });
    const auto beforeCrossCurveMove = runtime.documentVersion();
    const auto crossCurveMove =
        registry.invoke(QStringLiteral("parameters.move_anchors"), crossCurveMoveInput,
                        {.clientId = QStringLiteral("anchor-cross-curve-overlap")});
    expect(!crossCurveMove &&
               crossCurveMove.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
               crossCurveMove.getError().fieldPath == QStringLiteral("moves.position") &&
               runtime.documentVersion() == beforeCrossCurveMove,
           QStringLiteral("anchor movement must not make two anchor curves overlap"));
    auto mergeCurvesInput = commandArguments(runtime.documentVersion());
    mergeCurvesInput.insert(QStringLiteral("clip_id"), clipId.value());
    mergeCurvesInput.insert(QStringLiteral("name"), QStringLiteral("pitch"));
    mergeCurvesInput.insert(QStringLiteral("layer"), QStringLiteral("edited"));
    mergeCurvesInput.insert(QStringLiteral("target_curve_id"), anchorCurveId);
    mergeCurvesInput.insert(QStringLiteral("source_curve_id"), secondCurveIds.value(0, -1));
    const auto mergeBase = runtime.documentVersion();
    const auto mergedCurves =
        registry.invoke(QStringLiteral("parameters.merge_anchor_curves"), mergeCurvesInput,
                        {.clientId = QStringLiteral("anchor-curves-merge")});
    reportFailure(QStringLiteral("parameters.merge_anchor_curves"), mergedCurves);
    const auto undoMergedCurves =
        registry.invoke(QStringLiteral("history.undo"), commandArguments(runtime.documentVersion()),
                        {.clientId = QStringLiteral("anchor-curves-merge-undo")});
    expect(secondCurve && secondCurveIds.size() == 1 && mergedCurves &&
               mergedCurves.get().value(QStringLiteral("changed")).toBool() && undoMergedCurves &&
               runtime.documentVersion().revision == mergeBase.revision + 2,
           QStringLiteral("adjacent anchor curves must merge and undo as one History entry"));

    const auto publicEditingFixture = verifyPublicEditingBehavior(registry, runtime);
    if (publicEditingFixture) {
        verifyCapabilityRouting(registry, runtime, audioClipId, publicEditingFixture->noteClipId);
        verifyDuplicateSplitAndFillBehavior(registry, runtime, *publicEditingFixture);
        verifyFormatInspectionAndMidiPreview(registry, runtime, fixture.model(),
                                             *publicEditingFixture, directory.path());
        verifyPhonemeOriginalPreservation(registry, runtime, *publicEditingFixture);
        access.update(AutomationWire::AutomationProfile::L2);
        const auto deniedPackageLookup = registry.invoke(QStringLiteral("packages.list"), {});
        expect(!deniedPackageLookup && deniedPackageLookup.getError().code ==
                                           Automation::AutomationErrorCode::PermissionDenied,
               QStringLiteral("L2 voice workflows must not depend on the L3 packages domain"));
        verifyPublicVoiceAndSpeakerMix(registry, runtime, *publicEditingFixture, registrySinger,
                                       registrySpeakerA, registrySpeakerB,
                                       registrySpeakerlessSinger, registrySingerV2,
                                       registrySpeakerV2, registrySpeakerV2B);
        access.update(AutomationWire::AutomationProfile::L3);
        verifyAdvancedGuiBindings(registry, runtime, *publicEditingFixture);
        const auto projectForStatistics =
            runtime.project().getProject(runtime.documentVersion().documentId);
        const auto documentWithStatistics = registry.invoke(
            QStringLiteral("documents.get"),
            QJsonObject{
                {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()}
        });
        const auto actualStatistics = documentWithStatistics
                                          ? documentWithStatistics.get()
                                                .value(QStringLiteral("snapshot"))
                                                .toObject()
                                                .value(QStringLiteral("statistics"))
                                                .toObject()
                                          : QJsonObject{};
        expect(
            projectForStatistics && documentWithStatistics &&
                actualStatistics == expectedDocumentStatistics(projectForStatistics.get()),
            QStringLiteral("documents.get statistics must exactly classify every track and clip"));
    }

    runtime.automationTasks().discardDocumentGeneration(runtime.documentVersion().documentId);
    expect(
        admission.snapshot().backgroundTasks == 0,
        QStringLiteral("async task-domain fixture must start without a retained Admission lease"));
    QJsonObject openInput{
        {QStringLiteral("current_document_id"), runtime.documentVersion().documentId.toString()},
        {QStringLiteral("expected_revision"),
         static_cast<qint64>(runtime.documentVersion().revision)                               },
        {QStringLiteral("path"),                backgroundOpenFile.fileName()                  },
        {QStringLiteral("unsaved_policy"),      QStringLiteral("discard")                      },
    };
    const auto opened = registry.invoke(QStringLiteral("documents.open"), openInput,
                                        {.clientId = QStringLiteral("background")});
    if (!opened) {
        QTextStream(stderr) << "DETAIL async documents.open: "
                            << Automation::errorCodeName(opened.getError().code) << ", "
                            << opened.getError().fieldPath << ", " << opened.getError().message
                            << Qt::endl;
    }
    const auto taskId = opened ? Automation::TaskId::fromString(
                                     opened.get().value(QStringLiteral("task_id")).toString())
                               : Automation::TaskId{};
    expect(bool(opened), QStringLiteral("async document-open fixture must be accepted"));
    expect(!taskId.isNull(), QStringLiteral("async document-open must return a valid task ID"));
    expect(admission.snapshot().backgroundTasks == 1,
           QStringLiteral("async Admission lease must remain held after invoke returns"));
    const auto polled = registry.invoke(
        QStringLiteral("tasks.get"),
        QJsonObject{
            {QStringLiteral("scope"),       QStringLiteral("document")                     },
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
            {QStringLiteral("task_id"),     taskId.toString()                              },
    },
        {.clientId = QStringLiteral("task-poll")});
    if (!polled) {
        QTextStream(stderr) << "DETAIL tasks.get running document task: "
                            << Automation::errorCodeName(polled.getError().code) << ", "
                            << polled.getError().fieldPath << ", " << polled.getError().message
                            << Qt::endl;
    }
    expect(bool(polled),
           QStringLiteral("a running document task must remain observable through task domain"));
    expect(admission.snapshot().backgroundTasks == 1,
           QStringLiteral("polling must not release a running task's Admission lease"));
    runtime.automationTasks().discardDocumentGeneration(runtime.documentVersion().documentId);
    expect(admission.snapshot().backgroundTasks == 0,
           QStringLiteral("generation discard must release the retained async Admission lease"));

    const auto reopened = registry.invoke(QStringLiteral("documents.open"), openInput,
                                          {.clientId = QStringLiteral("background-reacquire")});
    if (!reopened) {
        QTextStream(stderr) << "DETAIL async documents.open reacquire: "
                            << Automation::errorCodeName(reopened.getError().code) << ", "
                            << reopened.getError().fieldPath << ", " << reopened.getError().message
                            << Qt::endl;
    }
    const auto reopenedTaskId =
        reopened ? Automation::TaskId::fromString(
                       reopened.get().value(QStringLiteral("task_id")).toString())
                 : Automation::TaskId{};
    expect(bool(reopened) && !reopenedTaskId.isNull(),
           QStringLiteral("generation discard must make the background slot reusable"));
    expect(admission.snapshot().backgroundTasks == 1,
           QStringLiteral("the reacquired background slot must retain its Admission lease"));
    const auto queuedDocumentTasks = registry.invoke(
        QStringLiteral("tasks.list"),
        QJsonObject{
            {QStringLiteral("scope"),       QStringLiteral("document")                     },
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
            {QStringLiteral("state"),       QStringLiteral("queued")                       },
            {QStringLiteral("kind"),        QStringLiteral("documents.open")               },
            {QStringLiteral("limit"),       10                                             },
    },
        {.clientId = QStringLiteral("task-filter")});
    const auto unrelatedTasks = registry.invoke(
        QStringLiteral("tasks.list"),
        QJsonObject{
            {QStringLiteral("scope"),       QStringLiteral("document")                     },
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
            {QStringLiteral("kind"),        QStringLiteral("exports.audio.start")          },
            {QStringLiteral("limit"),       10                                             },
    },
        {.clientId = QStringLiteral("task-filter-empty")});
    if (!queuedDocumentTasks || !unrelatedTasks) {
        const auto &failure =
            queuedDocumentTasks ? unrelatedTasks.getError() : queuedDocumentTasks.getError();
        QTextStream(stderr) << "DETAIL tasks.list filtering: "
                            << Automation::errorCodeName(failure.code) << ", " << failure.fieldPath
                            << ", " << failure.message << Qt::endl;
    }
    expect(queuedDocumentTasks &&
               queuedDocumentTasks.get().value(QStringLiteral("tasks")).toArray().size() == 1,
           QStringLiteral("tasks.list must retain the matching state and kind"));
    expect(unrelatedTasks &&
               unrelatedTasks.get().value(QStringLiteral("tasks")).toArray().isEmpty(),
           QStringLiteral("tasks.list must remove unrelated kinds before pagination"));
    const auto canceled = registry.invoke(
        QStringLiteral("tasks.cancel"),
        QJsonObject{
            {QStringLiteral("scope"),       QStringLiteral("document")                     },
            {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
            {QStringLiteral("task_id"),     reopenedTaskId.toString()                      },
    },
        {.clientId = QStringLiteral("task-cancel")});
    if (!canceled) {
        QTextStream(stderr) << "DETAIL tasks.cancel running document task: "
                            << Automation::errorCodeName(canceled.getError().code) << ", "
                            << canceled.getError().fieldPath << ", " << canceled.getError().message
                            << Qt::endl;
    }
    expect(bool(canceled),
           QStringLiteral("a running task must be cancelable through its independent task domain"));
    expect(admission.snapshot().backgroundTasks == 0,
           QStringLiteral("canceling a running task must release its Admission lease"));

    runtime.automationTasks().createTask(QStringLiteral("documents.open"),
                                         runtime.documentVersion());
    const auto internalTask = runtime.automationTasks().createTask(
        QStringLiteral("audio_clips.apply_decode_cache"), runtime.documentVersion());
    const QJsonObject allTasksInput{
        {QStringLiteral("scope"),       QStringLiteral("document")                     },
        {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
    };
    const auto publicTasks = registry.invoke(QStringLiteral("tasks.list"), allTasksInput);
    bool exposedInternalTask = false;
    if (publicTasks) {
        for (const auto &value : publicTasks.get().value(QStringLiteral("tasks")).toArray()) {
            exposedInternalTask |= value.toObject().value(QStringLiteral("task_id")).toString() ==
                                   internalTask.taskId.toString();
        }
    }
    QJsonObject internalTaskInput{
        {QStringLiteral("scope"),       QStringLiteral("document")                     },
        {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
        {QStringLiteral("task_id"),     internalTask.taskId.toString()                 },
    };
    const auto internalTaskGet = registry.invoke(QStringLiteral("tasks.get"), internalTaskInput);
    const auto internalTaskCancel =
        registry.invoke(QStringLiteral("tasks.cancel"), internalTaskInput,
                        {.clientId = QStringLiteral("internal-task")});
    expect(publicTasks && !exposedInternalTask && !internalTaskGet && !internalTaskCancel &&
               internalTaskGet.getError().code == Automation::AutomationErrorCode::NotFound &&
               internalTaskCancel.getError().code == Automation::AutomationErrorCode::NotFound,
           QStringLiteral("public task tools must hide GUI and internal automation tasks"));
    const QJsonObject taskPageInput{
        {QStringLiteral("scope"),       QStringLiteral("document")                     },
        {QStringLiteral("document_id"), runtime.documentVersion().documentId.toString()},
        {QStringLiteral("limit"),       1                                              },
    };
    const auto taskPageOne = registry.invoke(QStringLiteral("tasks.list"), taskPageInput);
    const auto taskCursor =
        taskPageOne ? taskPageOne.get().value(QStringLiteral("next_cursor")).toString() : QString();
    auto taskPageTwoInput = taskPageInput;
    taskPageTwoInput.insert(QStringLiteral("cursor"), taskCursor);
    const auto taskPageTwo = registry.invoke(QStringLiteral("tasks.list"), taskPageTwoInput);
    auto forgedTaskPageInput = taskPageInput;
    forgedTaskPageInput.insert(QStringLiteral("cursor"), QStringLiteral("1"));
    const auto forgedTaskPage = registry.invoke(QStringLiteral("tasks.list"), forgedTaskPageInput);
    expect(
        taskPageOne && !taskCursor.isEmpty() && taskCursor != QStringLiteral("1") && taskPageTwo &&
            taskPageTwo.get().value(QStringLiteral("tasks")).toArray().size() == 1 &&
            !forgedTaskPage &&
            forgedTaskPage.getError().code == Automation::AutomationErrorCode::InvalidArgument,
        QStringLiteral("tasks.list must round-trip opaque cursors and reject numeric forgeries"));

    const QJsonObject manifestPageInput{
        {QStringLiteral("limit"), 1}
    };
    const auto manifestPageOne =
        registry.invoke(QStringLiteral("automation.get_manifest"), manifestPageInput);
    const auto manifestCursor =
        manifestPageOne ? manifestPageOne.get().value(QStringLiteral("next_cursor")).toString()
                        : QString();
    auto manifestPageTwoInput = manifestPageInput;
    manifestPageTwoInput.insert(QStringLiteral("cursor"), manifestCursor);
    QElapsedTimer cachedManifestPageTimer;
    cachedManifestPageTimer.start();
    const auto manifestPageTwo =
        registry.invoke(QStringLiteral("automation.get_manifest"), manifestPageTwoInput);
    const auto cachedManifestPageElapsedMs = cachedManifestPageTimer.elapsed();
    auto forgedManifestInput = manifestPageInput;
    forgedManifestInput.insert(QStringLiteral("cursor"), QStringLiteral("1"));
    const auto forgedManifest =
        registry.invoke(QStringLiteral("automation.get_manifest"), forgedManifestInput);
    expect(
        manifestPageOne && !manifestCursor.isEmpty() && manifestCursor != QStringLiteral("1") &&
            manifestPageTwo &&
            manifestPageTwo.get().value(QStringLiteral("operations")).toArray().size() == 1 &&
            cachedManifestPageElapsedMs < 250 && !forgedManifest &&
            forgedManifest.getError().code == Automation::AutomationErrorCode::InvalidArgument,
        QStringLiteral(
            "automation.get_manifest must round-trip opaque cursors and reject numeric forgeries"));

    const auto direct = registry.invoke(QStringLiteral("automation.get_file_access"), {},
                                        {.clientId = QStringLiteral("adapter")});
    Automation::McpRequestDispatcher dispatcher(
        registry, {QStringLiteral("registry-test"), QStringLiteral("1.0"), {}, {}});
    Mcp::RequestEnvelope discover;
    discover.id = 1;
    discover.method = QString::fromLatin1(Mcp::DiscoverMethod);
    discover.protocolVersion = QString::fromLatin1(Mcp::ProtocolVersion);
    const auto discoverResponse = dispatcher.dispatch(discover, QStringLiteral("adapter"));
    expect(!discoverResponse.contains(QStringLiteral("error")),
           QStringLiteral("server/discover adapter request must succeed"));
    Mcp::RequestEnvelope list;
    list.id = 2;
    list.method = QString::fromLatin1(Mcp::ToolsListMethod);
    list.protocolVersion = QString::fromLatin1(Mcp::ProtocolVersion);
    const auto listResponse = dispatcher.dispatch(list, QStringLiteral("adapter"));
    const auto firstListResult = listResponse.value(QStringLiteral("result")).toObject();
    auto listedTools = firstListResult.value(QStringLiteral("tools")).toArray();
    const auto listCursor = firstListResult.value(QStringLiteral("nextCursor")).toString();
    auto nextList = list;
    nextList.id = 3;
    nextList.params.insert(QStringLiteral("cursor"), listCursor);
    QElapsedTimer cachedToolsPageTimer;
    cachedToolsPageTimer.start();
    const auto nextListResponse = dispatcher.dispatch(nextList, QStringLiteral("adapter"));
    const auto cachedToolsPageElapsedMs = cachedToolsPageTimer.elapsed();
    for (const auto &tool : nextListResponse.value(QStringLiteral("result"))
                                .toObject()
                                .value(QStringLiteral("tools"))
                                .toArray()) {
        listedTools.append(tool);
    }
    QSet<QString> listedIds;
    for (const auto &tool : listedTools)
        listedIds.insert(tool.toObject().value(QStringLiteral("name")).toString());
    expect(!listCursor.isEmpty() && !nextListResponse.contains(QStringLiteral("error")) &&
               cachedToolsPageElapsedMs < 250 && listedTools.size() == 177 &&
               listedIds == PublicAutomationToolsetExpectations::editorToolIdSet(),
           QStringLiteral("tools/list must expose the exact 177-tool editor surface"));
    access.update(AutomationWire::AutomationProfile::L1);
    const auto reducedListResponse = dispatcher.dispatch(list, QStringLiteral("adapter"));
    const auto reducedListResult = reducedListResponse.value(QStringLiteral("result")).toObject();
    expect(!reducedListResponse.contains(QStringLiteral("error")) &&
               reducedListResult.value(QStringLiteral("tools")).toArray().size() == 89 &&
               reducedListResult.value(QStringLiteral("nextCursor")).toString().isEmpty(),
           QStringLiteral("tools/list cache must invalidate when the access profile changes"));
    const auto staleListResponse = dispatcher.dispatch(nextList, QStringLiteral("adapter"));
    expect(staleListResponse.value(QStringLiteral("error"))
                   .toObject()
                   .value(QStringLiteral("code"))
                   .toInt() == Mcp::InvalidParams,
           QStringLiteral("tools/list cursors must be bound to the enabled tool snapshot"));
    access.update(AutomationWire::AutomationProfile::L3);
    auto forgedList = list;
    forgedList.params.insert(QStringLiteral("cursor"), QStringLiteral("1"));
    const auto forgedListResponse = dispatcher.dispatch(forgedList, QStringLiteral("client-a"));
    expect(forgedListResponse.value(QStringLiteral("error"))
                   .toObject()
                   .value(QStringLiteral("code"))
                   .toInt() == Mcp::InvalidParams,
           QStringLiteral("tools/list must reject forged numeric cursors"));
    Mcp::RequestEnvelope call;
    call.id = 4;
    call.method = QString::fromLatin1(Mcp::ToolsCallMethod);
    call.protocolVersion = QString::fromLatin1(Mcp::ProtocolVersion);
    call.name = QStringLiteral("automation.get_file_access");
    call.params = QJsonObject{
        {QStringLiteral("name"),      call.name    },
        {QStringLiteral("arguments"), QJsonObject{}}
    };
    const auto callResponse = dispatcher.dispatch(call, QStringLiteral("adapter"));
    expect(direct && callResponse.value(QStringLiteral("result"))
                             .toObject()
                             .value(QStringLiteral("structuredContent")) == direct.get(),
           QStringLiteral("dispatcher tools/call result must equal direct Registry invocation"));
    auto resumedCall = call;
    resumedCall.params.insert(QStringLiteral("requestState"), QStringLiteral("opaque-state"));
    resumedCall.params.insert(QStringLiteral("inputResponses"),
                              QJsonObject{
                                  {QStringLiteral("answer"), QJsonObject{}}
    });
    const auto resumedResponse = dispatcher.dispatch(resumedCall, QStringLiteral("adapter"));
    expect(resumedResponse.value(QStringLiteral("error"))
                       .toObject()
                       .value(QStringLiteral("code"))
                       .toInt() == Mcp::InvalidParams &&
               resumedResponse.value(QStringLiteral("error"))
                       .toObject()
                       .value(QStringLiteral("message"))
                       .toString() == QStringLiteral("No pending MRTR request"),
           QStringLiteral(
               "07-28 MRTR fields must be recognized and rejected as having no pending request"));
    resumedCall.params.insert(QStringLiteral("inputResponses"), QJsonArray{});
    const auto invalidResponses = dispatcher.dispatch(resumedCall, QStringLiteral("adapter"));
    expect(
        invalidResponses.value(QStringLiteral("error"))
            .toObject()
            .value(QStringLiteral("message"))
            .toString()
            .contains(QStringLiteral("inputResponses")),
        QStringLiteral("tools/call inputResponses must be type-checked before MRTR state lookup"));
    resumedCall = call;
    resumedCall.params.insert(QStringLiteral("requestState"), 42);
    const auto invalidRequestState = dispatcher.dispatch(resumedCall, QStringLiteral("adapter"));
    expect(invalidRequestState.value(QStringLiteral("error"))
               .toObject()
               .value(QStringLiteral("message"))
               .toString()
               .contains(QStringLiteral("requestState")),
           QStringLiteral("tools/call requestState must be type-checked before MRTR state lookup"));
    call.name = QStringLiteral("missing.tool");
    call.params.insert(QStringLiteral("name"), call.name);
    const auto unknown = dispatcher.dispatch(call, QStringLiteral("adapter"));
    expect(
        unknown.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt() ==
            Mcp::InvalidParams,
        QStringLiteral("unknown tool must be a JSON-RPC invalid-params error"));

    for (const auto &contract : registry.contracts()) {
        auto input = validInputSample(contract, runtime.documentVersion());
        const auto validatesOnly = contract.inputSchema.value(QStringLiteral("properties"))
                                       .toObject()
                                       .contains(QStringLiteral("validate_only"));
        if (validatesOnly)
            input.insert(QStringLiteral("validate_only"), true);
        expect(AutomationWire::validateJsonValue(input, contract.inputSchema).valid(),
               QStringLiteral("generated schema-valid smoke input failed for %1")
                   .arg(contract.operationId));
        if (contract.kind == AutomationWire::OperationKind::Command && !validatesOnly)
            continue;
        const auto smoke =
            registry.invoke(contract.operationId, input, {.clientId = QStringLiteral("smoke")});
        const auto stableFailure =
            smoke ||
            (smoke.getError().operationId == contract.operationId &&
             smoke.getError().code != Automation::AutomationErrorCode::InternalError &&
             smoke.getError().code != Automation::AutomationErrorCode::OperationUnavailable &&
             smoke.getError().code != Automation::AutomationErrorCode::ModuleNotReady);
        expect(stableFailure,
               QStringLiteral("binding smoke for %1 failed with unstable %2: %3")
                   .arg(contract.operationId,
                        smoke ? QStringLiteral("success")
                              : Automation::errorCodeName(smoke.getError().code),
                        smoke ? QString()
                              : QStringLiteral("%1 (%2)").arg(smoke.getError().message,
                                                              smoke.getError().fieldPath)));
    }

    Automation::CommandContext replaceContext;
    replaceContext.expected = runtime.documentVersion();
    replaceContext.source = Automation::InvocationSource::InternalAutomation;
    auto replaced = runtime.documents().commitOpenedDocument(
        replaceContext, Automation::DocumentAutomationFacade::newDocumentDraft(true),
        readable.fileName(), QStringLiteral("project.dspx"), true);
    expect(
        replaced && replaced.get().presentationEffects ==
                        QStringList{QStringLiteral("active_document_changed")},
        QStringLiteral("opening a document must report the active-document presentation effect"));
    Automation::CommandContext dirtyContext;
    dirtyContext.expected = runtime.documentVersion();
    dirtyContext.source = Automation::InvocationSource::InternalAutomation;
    const auto dirtied = runtime.timeline().setTempo(dirtyContext, 960, 141.0);
    const auto dirtyHistory = runtime.history().getState(runtime.documentVersion().documentId);
    expect(dirtied && dirtyHistory && !dirtyHistory.get().onSavePoint,
           QStringLiteral("save-current setup must create an unsaved History state"));
    saveCurrent = commandArguments(runtime.documentVersion());
    const auto saveExistingByDefault =
        registry.invoke(QStringLiteral("documents.save"), saveCurrent,
                        {.clientId = QStringLiteral("save-current-overwrite-default")});
    const auto savedHistory = runtime.history().getState(runtime.documentVersion().documentId);
    expect(saveExistingByDefault && savedHistory && savedHistory.get().onSavePoint,
           QStringLiteral("save-current must overwrite its own path and update the savepoint"));

    QFile explicitExisting(directory.filePath(QStringLiteral("save-as-existing.dspx")));
    expect(explicitExisting.open(QIODevice::WriteOnly),
           QStringLiteral("explicit Save As fixture must be available"));
    explicitExisting.close();
    auto saveAsExisting = commandArguments(runtime.documentVersion());
    saveAsExisting.insert(QStringLiteral("path"), explicitExisting.fileName());
    QString rejectExistingPolicy;
    const auto *saveAsContract =
        AutomationWire::findPublicTool(QStringLiteral("documents.save_as"));
    if (saveAsContract) {
        const auto policies = saveAsContract->inputSchema.value(QStringLiteral("properties"))
                                  .toObject()
                                  .value(QStringLiteral("overwrite_policy"))
                                  .toObject()
                                  .value(QStringLiteral("enum"))
                                  .toArray();
        for (const auto &policyValue : policies) {
            const auto policy = policyValue.toString();
            if (!policy.contains(QStringLiteral("overwrite"), Qt::CaseInsensitive) &&
                !policy.contains(QStringLiteral("replace"), Qt::CaseInsensitive)) {
                rejectExistingPolicy = policy;
                break;
            }
        }
    }
    expect(!rejectExistingPolicy.isEmpty(),
           QStringLiteral("Save As must expose an unattended reject-existing policy"));
    saveAsExisting.insert(QStringLiteral("overwrite_policy"), rejectExistingPolicy);
    const auto saveAsDenied =
        registry.invoke(QStringLiteral("documents.save_as"), saveAsExisting,
                        {.clientId = QStringLiteral("save-as-overwrite-default")});
    expect(!saveAsDenied &&
               saveAsDenied.getError().code == Automation::AutomationErrorCode::OverwriteDenied,
           QStringLiteral("Save As reject policy must refuse an existing target"));

    QTemporaryDir outsideDirectory;
    QFile outsideProject(outsideDirectory.filePath(QStringLiteral("outside.dspx")));
    expect(outsideDirectory.isValid() && outsideProject.open(QIODevice::WriteOnly),
           QStringLiteral("outside-root project fixture must be available"));
    outsideProject.close();
    replaceContext.expected = runtime.documentVersion();
    replaceContext.source = Automation::InvocationSource::InternalAutomation;
    replaced = runtime.documents().commitOpenedDocument(
        replaceContext, Automation::DocumentAutomationFacade::newDocumentDraft(true),
        outsideProject.fileName(), QStringLiteral("outside.dspx"), true);
    expect(bool(replaced), QStringLiteral("outside-root document setup must succeed"));
    saveCurrent = commandArguments(runtime.documentVersion());
    const auto saveOutsideRoot =
        registry.invoke(QStringLiteral("documents.save"), saveCurrent,
                        {.clientId = QStringLiteral("save-current-outside-root")});
    expect(!saveOutsideRoot &&
               saveOutsideRoot.getError().code ==
                   Automation::AutomationErrorCode::PermissionDenied &&
               saveOutsideRoot.getError().fieldPath == QStringLiteral("path"),
           QStringLiteral("save-current must authorize the resolved session path"));

    const auto previousDocument = runtime.documentVersion();
    const auto newDocument = registry.invoke(
        QStringLiteral("documents.new"),
        QJsonObject{
            {QStringLiteral("current_document_id"), previousDocument.documentId.toString()        },
            {QStringLiteral("expected_revision"),   static_cast<qint64>(previousDocument.revision)},
            {QStringLiteral("unsaved_policy"),      QStringLiteral("discard")                     },
            {QStringLiteral("template"),            QStringLiteral("empty")                       },
    },
        {.clientId = QStringLiteral("document-lifecycle-effects")});
    expect(newDocument &&
               newDocument.get()
                       .value(QStringLiteral("previous"))
                       .toObject()
                       .value(QStringLiteral("document_id")) ==
                   previousDocument.documentId.toString() &&
               newDocument.get()
                       .value(QStringLiteral("current"))
                       .toObject()
                       .value(QStringLiteral("document_id")) ==
                   runtime.documentVersion().documentId.toString() &&
               newDocument.get().value(QStringLiteral("presentation_effects")).toArray() ==
                   QJsonArray{QStringLiteral("active_document_changed")},
           QStringLiteral("documents.new must encode its active-document presentation effect"));

    return failures == 0 ? 0 : 1;
}
