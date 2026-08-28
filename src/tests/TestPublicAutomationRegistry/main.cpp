#include "Automation/Public/AdmissionController.h"
#include "Automation/Public/AutomationAccessPolicy.h"
#include "Automation/Public/AutomationFileGuard.h"
#include "Automation/Public/PublicAutomationRegistry.h"
#include "TestRuntime.h"

#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/PublicConstants.h>
#include <lite/AutomationWire/PublicToolContract.h>

#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>
#include <QVersionNumber>

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

namespace {
    int failures = 0;

    struct PackageRefreshTestControl {
        QString privatePath;
        int starts = 0;
        bool deferNext = false;
        bool failNext = false;
        Automation::PackageRefreshCommitGate pendingCommitGate;
        Automation::PackageRefreshCompletion pendingCompletion;
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

    Automation::PublicAutomationHostServices hostServices(Automation::CoreRuntime &runtime) {
        Automation::PublicAutomationHostServices services;
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

    QJsonObject mergeCommandArguments(const Automation::DocumentVersion &document,
                                      const QJsonObject &operationArguments) {
        auto result = commandArguments(document);
        for (auto it = operationArguments.constBegin(); it != operationArguments.constEnd(); ++it)
            result.insert(it.key(), it.value());
        return result;
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

    struct PublicEditingFixture {
        Automation::TrackId trackId;
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
        createPublicEditingFixture(Automation::PublicAutomationRegistry &registry,
                                   Automation::CoreRuntime &runtime) {
        const auto project = runtime.project().getProject(runtime.documentVersion().documentId);
        if (!project)
            return std::nullopt;

        auto trackInput = commandArguments(runtime.documentVersion());
        trackInput.insert(QStringLiteral("index"), project.get().tracks.size());
        trackInput.insert(QStringLiteral("tracks"),
                          QJsonArray{
                              QJsonObject{
                                          {QStringLiteral("client_ref"), QStringLiteral("registry-track")},
                                          {QStringLiteral("name"), QStringLiteral("Registry Track")},
                                          {QStringLiteral("color_index"), 0},
                                          }
        });
        const auto insertedTrack =
            invokeSchemaValid(registry, QStringLiteral("tracks.insert"), trackInput,
                              QStringLiteral("representative public track insertion"));
        const auto trackIds = insertedTrack
                                  ? createdObjectIds(*insertedTrack, QStringLiteral("track"))
                                  : QList<int>{};
        if (trackIds.size() != 1)
            return std::nullopt;

        PublicEditingFixture fixture;
        fixture.trackId = Automation::TrackId(trackIds.first());
        auto clipInput = commandArguments(runtime.documentVersion());
        clipInput.insert(QStringLiteral("clips"),
                         QJsonArray{
                             QJsonObject{
                                         {QStringLiteral("client_ref"), QStringLiteral("scalar-clip")},
                                         {QStringLiteral("track_id"), fixture.trackId.value()},
                                         {QStringLiteral("start"), 0},
                                         },
                             QJsonObject{
                                         {QStringLiteral("client_ref"), QStringLiteral("note-clip")},
                                         {QStringLiteral("track_id"), fixture.trackId.value()},
                                         {QStringLiteral("start"), 2400},
                                         {QStringLiteral("length"), 1920},
                                         {QStringLiteral("name"), QStringLiteral("Notes")},
                                         },
                             QJsonObject{
                                         {QStringLiteral("client_ref"), QStringLiteral("mix-clip")},
                                         {QStringLiteral("track_id"), fixture.trackId.value()},
                                         {QStringLiteral("start"), 4800},
                                         {QStringLiteral("length"), 1920},
                                         {QStringLiteral("name"), QStringLiteral("Mix")},
                                         },
        });
        const auto insertedClips =
            invokeSchemaValid(registry, QStringLiteral("clips.insert"), clipInput,
                              QStringLiteral("representative public clip insertion"));
        const auto clipIds =
            insertedClips ? createdObjectIds(*insertedClips, QStringLiteral("clip")) : QList<int>{};
        if (clipIds.size() != 3)
            return std::nullopt;
        fixture.scalarClipId = Automation::ClipId(clipIds.at(0));
        fixture.noteClipId = Automation::ClipId(clipIds.at(1));
        fixture.mixClipId = Automation::ClipId(clipIds.at(2));

        auto noteInput = commandArguments(runtime.documentVersion());
        noteInput.insert(QStringLiteral("clip_id"), fixture.noteClipId.value());
        noteInput.insert(QStringLiteral("notes"),
                         QJsonArray{
                             QJsonObject{
                                         {QStringLiteral("client_ref"), QStringLiteral("note-a")},
                                         {QStringLiteral("local_start"), 0},
                                         {QStringLiteral("length"), 480},
                                         {QStringLiteral("key_index"), 60},
                                         },
                             QJsonObject{
                                         {QStringLiteral("client_ref"), QStringLiteral("note-b")},
                                         {QStringLiteral("local_start"), 720},
                                         {QStringLiteral("length"), 480},
                                         {QStringLiteral("key_index"), 64},
                                         {QStringLiteral("lyric"), QStringLiteral("seed")},
                                         },
        });
        const auto insertedNotes =
            invokeSchemaValid(registry, QStringLiteral("notes.insert"), noteInput,
                              QStringLiteral("representative public note insertion"));
        const auto noteIds =
            insertedNotes ? createdObjectIds(*insertedNotes, QStringLiteral("note")) : QList<int>{};
        if (noteIds.size() != 2)
            return std::nullopt;
        fixture.noteIds = {Automation::NoteId(noteIds.at(0)), Automation::NoteId(noteIds.at(1))};
        return fixture;
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

    void verifyPublicVoiceAndSpeakerMix(Automation::PublicAutomationRegistry &registry,
                                        Automation::CoreRuntime &runtime,
                                        const PublicEditingFixture &fixture,
                                        const SingerInfo &singer,
                                        const SingerInfo &sameIdNewerSinger,
                                        const SpeakerInfo &sameIdNewerSpeaker,
                                        const SpeakerInfo &sameIdNewerSpeakerB) {
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

        const auto exactVoice = voiceSelection(sameIdNewerSinger, sameIdNewerSpeaker);
        const auto described = invokeSchemaValid(
            registry, QStringLiteral("voices.describe"),
            QJsonObject{
                {QStringLiteral("singer"), exactVoice.value(QStringLiteral("singer")).toObject()}
        },
            QStringLiteral("versioned voices.describe"));
        const auto describedSnapshot =
            described ? described->value(QStringLiteral("snapshot")).toObject() : QJsonObject{};
        expect(described &&
                   describedSnapshot.value(QStringLiteral("package_version")).toString() ==
                       sameIdNewerSinger.packageVersion().toString() &&
                   describedSnapshot.value(QStringLiteral("speakers"))
                           .toArray()
                           .first()
                           .toObject()
                           .value(QStringLiteral("speaker_id")) == sameIdNewerSpeaker.id(),
               QStringLiteral("voices.describe must resolve the requested package version"));

        invokeChangedOnce(registry, runtime, QStringLiteral("tracks.set_voice"),
                          QJsonObject{
                              {QStringLiteral("track_id"), fixture.trackId.value()},
                              {QStringLiteral("voice"),    exactVoice             }
        },
                          QStringLiteral("track-set-versioned-voice"),
                          QStringLiteral("tracks.set_voice with an exact package version"));
        const auto voiceContext =
            voiceContextSnapshot(registry, runtime, QStringLiteral("tracks.get"),
                                 QStringLiteral("track_id"), fixture.trackId.value());
        const auto singerRef = voiceContext.value(QStringLiteral("own_voice"))
                                   .toObject()
                                   .value(QStringLiteral("singer"))
                                   .toObject();
        const auto speakerRef = voiceContext.value(QStringLiteral("own_voice"))
                                    .toObject()
                                    .value(QStringLiteral("speaker"))
                                    .toObject();
        expect(singerRef.value(QStringLiteral("package_version")).toString() ==
                       sameIdNewerSinger.packageVersion().toString() &&
                   speakerRef.value(QStringLiteral("speaker_id")).toString() ==
                       sameIdNewerSpeaker.id(),
               QStringLiteral("track voice selection must distinguish packages by version"));

        const QJsonObject exactMix{
            {QStringLiteral("singer"),  singerRef},
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
             }                                   },
        };
        invokeChangedOnce(registry, runtime, QStringLiteral("speaker_mix.set_fixed"),
                          QJsonObject{
                              {QStringLiteral("target"),
                               QJsonObject{{QStringLiteral("type"), QStringLiteral("track")},
                                           {QStringLiteral("id"), fixture.trackId.value()}}},
                              {QStringLiteral("mix"),    exactMix                          },
        },
                          QStringLiteral("track-set-versioned-mix"),
                          QStringLiteral("speaker_mix.set_fixed with an exact package version"));
        const auto mix =
            speakerMixSnapshot(registry, runtime, QStringLiteral("track"), fixture.trackId.value())
                .value(QStringLiteral("mix"))
                .toObject();
        expect(mix.value(QStringLiteral("singer"))
                           .toObject()
                           .value(QStringLiteral("package_version")) ==
                       sameIdNewerSinger.packageVersion().toString() &&
                   mix.value(QStringLiteral("sources"))
                           .toArray()
                           .first()
                           .toObject()
                           .value(QStringLiteral("speaker"))
                           .toObject()
                           .value(QStringLiteral("speaker_id")) == sameIdNewerSpeaker.id(),
               QStringLiteral("Speaker Mix must preserve the requested package version"));
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
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
    Automation::ApplicationRuntimeServices applicationServices;
    applicationServices.info = [] {
        return Automation::ApplicationInfoDto{
            .name = QStringLiteral("DS Editor Lite"),
            .version = QStringLiteral("registry-version"),
            .platform = QStringLiteral("registry-platform"),
            .buildId = QStringLiteral("registry-build-id"),
        };
    };
    auto terminationResult = std::make_shared<Automation::ApplicationTerminationRequestResult>(
        Automation::ApplicationTerminationRequestResult::Accepted);
    auto terminationCalls = std::make_shared<int>(0);
    auto lastTerminationMode = std::make_shared<Automation::ApplicationTerminationMode>(
        Automation::ApplicationTerminationMode::Exit);
    auto lastTerminationSavePolicy = std::make_shared<Automation::ApplicationTerminationSavePolicy>(
        Automation::ApplicationTerminationSavePolicy::Prompt);
    applicationServices.requestTermination =
        [terminationResult, terminationCalls, lastTerminationMode,
         lastTerminationSavePolicy](const Automation::ApplicationTerminationMode mode,
                                    const Automation::ApplicationTerminationSavePolicy savePolicy) {
            ++*terminationCalls;
            *lastTerminationMode = mode;
            *lastTerminationSavePolicy = savePolicy;
            return *terminationResult;
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
        std::move(editorServices), {}, {}, {}, std::move(packageServices), {},
        std::move(applicationServices), std::move(settingsServices));
    fixture.model().newProject();
    auto &runtime = fixture.runtime();

    QTemporaryDir directory;
    expect(directory.isValid(), QStringLiteral("test file root must be available"));

    Automation::AutomationAccessPolicy access(AutomationWire::AutomationProfile::L3);
    Automation::AutomationFileGuard fileGuard;
    expect(bool(fileGuard.setConfiguredRoots({directory.path()}, {directory.path()})),
           QStringLiteral("file guard roots must configure"));
    Automation::AdmissionLimits limits;
    limits.maximumGlobalInFlight = 8;
    limits.maximumBackgroundTasks = 1;
    Automation::AdmissionController admission(limits);
    Automation::PublicAutomationRegistry registry(runtime, access, fileGuard, admission,
                                                  hostServices(runtime));

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

    const auto exitRequest = registry.invoke(QStringLiteral("application.request_exit"), {});
    expect(exitRequest && exitRequest.get().value(QStringLiteral("accepted")).toBool() &&
               exitRequest.get().value(QStringLiteral("action")).toString() ==
                   QStringLiteral("exit") &&
               !exitRequest.get().value(QStringLiteral("discard_changes")).toBool() &&
               *terminationCalls == 1 &&
               *lastTerminationMode == Automation::ApplicationTerminationMode::Exit &&
               *lastTerminationSavePolicy ==
                   Automation::ApplicationTerminationSavePolicy::RejectUnsaved,
           QStringLiteral("application.request_exit must request non-interactive graceful exit"));

    *terminationResult = Automation::ApplicationTerminationRequestResult::UnsavedChanges;
    const auto rejectedRestart = registry.invoke(QStringLiteral("application.request_restart"), {});
    expect(!rejectedRestart &&
               rejectedRestart.getError().code == Automation::AutomationErrorCode::Busy &&
               rejectedRestart.getError().fieldPath == QStringLiteral("discard_changes") &&
               *terminationCalls == 2 &&
               *lastTerminationSavePolicy ==
                   Automation::ApplicationTerminationSavePolicy::RejectUnsaved,
           QStringLiteral("lifecycle requests must reject unsaved changes without prompting"));

    *terminationResult = Automation::ApplicationTerminationRequestResult::Accepted;
    const auto restartRequest = registry.invoke(QStringLiteral("application.request_restart"),
                                                QJsonObject{
                                                    {QStringLiteral("discard_changes"), true}
    });
    expect(restartRequest &&
               restartRequest.get().value(QStringLiteral("action")).toString() ==
                   QStringLiteral("restart") &&
               restartRequest.get().value(QStringLiteral("discard_changes")).toBool() &&
               *terminationCalls == 3 &&
               *lastTerminationMode == Automation::ApplicationTerminationMode::Restart &&
               *lastTerminationSavePolicy == Automation::ApplicationTerminationSavePolicy::Discard,
           QStringLiteral("discard_changes must opt into a non-interactive graceful restart"));

    auto expectedIds = AutomationWire::publicToolIds();
    auto bindingIds = registry.bindingIds();
    std::sort(expectedIds.begin(), expectedIds.end());
    expect(bindingIds == expectedIds && registry.isComplete(),
           QStringLiteral("every declared editor contract must have exactly one binding"));

    const auto status = registry.invoke(QStringLiteral("application.get_status"), {},
                                        {.clientId = QStringLiteral("status")});
    expect(status && status.get().value(QStringLiteral("documents")).toArray().size() == 1 &&
               status.get().value(QStringLiteral("windows")).toArray().size() == 1,
           QStringLiteral("single-document host status must truncate documents and windows"));

    const auto strictInput = registry.invoke(QStringLiteral("application.get_file_access"),
                                             QJsonObject{
                                                 {QStringLiteral("unexpected"), true}
    },
                                             {.clientId = QStringLiteral("strict-schema")});
    expect(!strictInput &&
               strictInput.getError().code == Automation::AutomationErrorCode::InvalidArgument,
           QStringLiteral("strict input schemas must reject additional properties"));

    access.update(AutomationWire::AutomationProfile::L1);
    const auto denied = registry.invoke(QStringLiteral("formats.list"), {},
                                        {.clientId = QStringLiteral("permission")});
    expect(!denied && denied.getError().code == Automation::AutomationErrorCode::PermissionDenied,
           QStringLiteral("L1 must deny an L2 tool"));
    access.update(AutomationWire::AutomationProfile::Custom, {QStringLiteral("documents.get")});
    expect(access.isAllowed(QStringLiteral("application.get_status")) &&
               access.isAllowed(QStringLiteral("documents.get")) &&
               !access.isAllowed(QStringLiteral("tracks.set_color")),
           QStringLiteral("custom profile must retain L0 and only explicit business tools"));
    access.update(AutomationWire::AutomationProfile::L3);

    const auto publicEditingFixture = createPublicEditingFixture(registry, runtime);
    expect(publicEditingFixture.has_value(),
           QStringLiteral("representative public editing fixture must be created"));
    if (publicEditingFixture) {
        access.update(AutomationWire::AutomationProfile::L2);
        const auto deniedPackageLookup = registry.invoke(QStringLiteral("packages.list"), {});
        expect(!deniedPackageLookup && deniedPackageLookup.getError().code ==
                                           Automation::AutomationErrorCode::PermissionDenied,
               QStringLiteral("L2 voice workflows must not depend on the L3 packages domain"));
        verifyPublicVoiceAndSpeakerMix(registry, runtime, *publicEditingFixture, registrySinger,
                                       registrySingerV2, registrySpeakerV2, registrySpeakerV2B);
        access.update(AutomationWire::AutomationProfile::L3);
        verifyAdvancedGuiBindings(registry, runtime, *publicEditingFixture);
    }

    return failures == 0 ? 0 : 1;
}
