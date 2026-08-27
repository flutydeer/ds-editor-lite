#include "Automation/OperationIds.h"
#include "Automation/ProjectAutomationDtos.h"
#include "TestRuntime.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/SpeakerMixData.h>
#include <lite/ProjectModel/Voice/SingerInfo.h>
#include <lite/ProjectModel/Voice/SpeakerInfo.h>

#include <QCoreApplication>
#include <QHash>
#include <QProcess>
#include <QTextStream>
#include <QVersionNumber>
#include <QVector>

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <optional>

namespace {
    using Automation::AutomationErrorCode;
    using Automation::AutomationResult;
    using Automation::ClipId;
    using Automation::CommandContext;
    using Automation::CoreRuntime;
    using Automation::MutationResult;
    using Automation::NoteId;
    using Automation::OperationId;
    using Automation::TrackId;
    using AutomationTestSupport::TestRuntime;

    class Suite final {
    public:
        template <typename Function>
        void run(const OperationId &operationId, const QString &name, Function function) {
            m_current = operationId + QStringLiteral("/") + name;
            ++m_scenarios;
            ++m_operationScenarios[operationId];
            const auto failuresBefore = m_failures;
            function();
            if (failuresBefore == m_failures)
                ++m_passedScenarios;
        }

        void expect(const bool condition, const QString &message) {
            ++m_assertions;
            if (condition)
                return;
            ++m_failures;
            QTextStream(stderr) << "FAILED [" << m_current << "]: " << message << Qt::endl;
        }

        void requireOperations(const QList<OperationId> &operationIds) {
            run(QStringLiteral("coverage"), QStringLiteral("operation-manifest"), [&] {
                for (const auto &operationId : operationIds) {
                    expect(m_operationScenarios.value(operationId) > 0,
                           QStringLiteral("missing direct scenario for %1").arg(operationId));
                }
            });
        }

        int result() const {
            QTextStream(stdout) << "Automation editing domains: " << m_scenarios << " scenarios, "
                                << m_passedScenarios << " passed, " << m_assertions
                                << " assertions, " << m_failures << " failures" << Qt::endl;
            return m_failures == 0 ? 0 : 1;
        }

    private:
        QString m_current;
        QHash<OperationId, int> m_operationScenarios;
        int m_scenarios = 0;
        int m_passedScenarios = 0;
        int m_assertions = 0;
        int m_failures = 0;
    };

    CommandContext commandContext(const CoreRuntime &runtime, const bool validateOnly = false) {
        return {
            .expected = runtime.documentVersion(),
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
    }

    CommandContext wrongDocumentContext(const CoreRuntime &runtime) {
        auto context = commandContext(runtime);
        context.expected.documentId = Automation::DocumentId::create();
        context.expected.revision += 100;
        return context;
    }

    CommandContext staleContext(const CoreRuntime &runtime) {
        auto context = commandContext(runtime);
        context.expected.revision += 100;
        return context;
    }

    Automation::TrackDraftDto trackDraft(const QString &name, const QString &clientRef = {}) {
        Automation::TrackDraftDto result;
        result.clientRef = clientRef;
        result.name = name;
        result.colorIndex = 1;
        result.gain = 0.5;
        result.pan = -0.25;
        result.defaultLanguage = QStringLiteral("en");
        return result;
    }

    Automation::ClipDraftDto singingClipDraft(const QString &name, const QString &clientRef = {}) {
        Automation::ClipDraftDto result;
        result.clientRef = clientRef;
        result.type = Automation::ClipDraftDto::Type::Singing;
        result.properties.name = name;
        result.properties.start = 0;
        result.properties.length = 3840;
        result.properties.clipStart = 0;
        result.properties.clipLen = 3840;
        result.properties.gain = 1.0;
        result.defaultLanguage = QStringLiteral("en");
        return result;
    }

    Automation::ClipDraftDto audioClipDraft(const QString &name, const QString &clientRef = {}) {
        Automation::ClipDraftDto result;
        result.clientRef = clientRef;
        result.type = Automation::ClipDraftDto::Type::Audio;
        result.properties.name = name;
        result.properties.start = 0;
        result.properties.length = 100;
        result.properties.clipStart = 50;
        result.properties.clipLen = 100;
        result.properties.gain = 1.0;
        result.audioPath = QStringLiteral("fixture-audio.wav");
        return result;
    }

    bool sameClipTiming(const Automation::ClipPropertiesDto &left,
                        const Automation::ClipPropertiesDto &right) {
        return left.start == right.start && left.length == right.length &&
               left.clipStart == right.clipStart && left.clipLen == right.clipLen &&
               left.trimStartMs == right.trimStartMs && left.playLengthMs == right.playLengthMs &&
               left.materialLengthMs == right.materialLengthMs;
    }

    Automation::NoteDraftDto noteDraft(const int start, const int length, const int key,
                                       const QString &lyric, const QString &clientRef = {}) {
        Automation::NoteDraftDto result;
        result.clientRef = clientRef;
        result.localStart = start;
        result.length = length;
        result.keyIndex = key;
        result.lyric = lyric;
        result.language = QStringLiteral("en");
        return result;
    }

    TrackId insertedTrack(CoreRuntime &runtime, const QString &name, const qsizetype index = -1) {
        const auto project = runtime.project().getProject(runtime.documentVersion().documentId);
        const auto insertionIndex = index >= 0 ? index : project ? project.get().tracks.size() : 0;
        const auto result =
            runtime.project().insertTrack(commandContext(runtime), insertionIndex,
                                          trackDraft(name, QStringLiteral("fixture-%1").arg(name)));
        if (!result || result.get().affectedObjects.isEmpty())
            return {};
        return TrackId(result.get().affectedObjects.first().value);
    }

    ClipId insertedSingingClip(CoreRuntime &runtime, const TrackId trackId, const QString &name,
                               const int start = 0) {
        auto draft = singingClipDraft(name, QStringLiteral("fixture-%1").arg(name));
        draft.properties.start = start;
        const auto result = runtime.project().insertClips(commandContext(runtime),
                                                          {
                                                              {.trackId = trackId, .clip = draft}
        });
        if (!result || result.get().affectedObjects.isEmpty())
            return {};
        return ClipId(result.get().affectedObjects.first().value);
    }

    QList<NoteId> insertedNotes(CoreRuntime &runtime, const ClipId clipId,
                                QList<Automation::NoteDraftDto> drafts) {
        const auto result = runtime.notes().insertNotes(commandContext(runtime), clipId, drafts);
        QList<NoteId> ids;
        if (!result)
            return ids;
        for (const auto &object : result.get().affectedObjects)
            ids.append(NoteId(object.value));
        return ids;
    }

    std::optional<Automation::TrackSnapshotDto> trackSnapshot(CoreRuntime &runtime,
                                                              const TrackId trackId) {
        const auto project = runtime.project().getProject(runtime.documentVersion().documentId);
        if (!project)
            return std::nullopt;
        for (const auto &track : project.get().tracks) {
            if (track.id == trackId)
                return track;
        }
        return std::nullopt;
    }

    std::optional<Automation::ClipSnapshotDto> clipSnapshot(CoreRuntime &runtime,
                                                            const ClipId clipId) {
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

    std::optional<Automation::NoteSnapshotDto>
        noteSnapshot(CoreRuntime &runtime, const ClipId clipId, const NoteId noteId) {
        const auto notes = runtime.notes().getNotes(runtime.documentVersion().documentId, clipId);
        if (!notes)
            return std::nullopt;
        for (const auto &note : notes.get()) {
            if (note.id == noteId)
                return note;
        }
        return std::nullopt;
    }

    bool isError(const AutomationResult<MutationResult> &result, const AutomationErrorCode code,
                 const QString &fieldPath = {}) {
        return !result && result.getError().code == code &&
               (fieldPath.isEmpty() || result.getError().fieldPath == fieldPath);
    }

    void testProjectDomain(Suite &suite) {
        TestRuntime testRuntime;
        auto &runtime = testRuntime.runtime();

        TrackId first;
        TrackId second;
        TrackId third;
        suite.run(
            Automation::OperationIds::tracks::insert, QStringLiteral("validation-and-create"), [&] {
                const auto initial = runtime.documentVersion();
                const auto invalid = runtime.project().insertTrack(
                    commandContext(runtime), -1, trackDraft(QStringLiteral("invalid")));
                suite.expect(
                    isError(invalid, AutomationErrorCode::InvalidArgument, QStringLiteral("index")),
                    QStringLiteral("negative insertion index must be rejected"));
                const auto preview = runtime.project().insertTrack(
                    commandContext(runtime, true), 0,
                    trackDraft(QStringLiteral("First"), QStringLiteral("track-first")));
                suite.expect(preview && preview.get().validatedOnly && preview.get().changed &&
                                 preview.get().createdObjects.isEmpty() &&
                                 runtime.documentVersion() == initial,
                             QStringLiteral("validate-only must not allocate or mutate"));
                const auto insert = runtime.project().insertTrack(
                    commandContext(runtime), 0,
                    trackDraft(QStringLiteral("First"), QStringLiteral("track-first")));
                suite.expect(insert && insert.get().current.revision == initial.revision + 1 &&
                                 insert.get().createdObjects.size() == 1,
                             QStringLiteral("track insertion must be one revision with binding"));
                if (insert)
                    first = TrackId(insert.get().affectedObjects.first().value);
                second = insertedTrack(runtime, QStringLiteral("Second"));
                third = insertedTrack(runtime, QStringLiteral("Third"));
                suite.expect(first.isValid() && second.isValid() && third.isValid(),
                             QStringLiteral("fixture tracks must be created"));
            });

        suite.run(
            Automation::OperationIds::project::get, QStringLiteral("ordered-value-snapshot"), [&] {
                const auto result =
                    runtime.project().getProject(runtime.documentVersion().documentId);
                suite.expect(result && result.get().document == runtime.documentVersion() &&
                                 result.get().tracks.size() == 3 &&
                                 result.get().tracks.at(0).id == first &&
                                 result.get().tracks.at(1).id == second &&
                                 result.get().tracks.at(2).id == third,
                             QStringLiteral("project query must preserve ordered typed IDs"));
                const auto wrong = runtime.project().getProject(Automation::DocumentId::create());
                suite.expect(
                    !wrong && wrong.getError().code == AutomationErrorCode::DocumentChanged &&
                        wrong.getError().operationId == Automation::OperationIds::project::get,
                    QStringLiteral("query must reject an old document generation"));
            });

        suite.run(
            Automation::OperationIds::tracks::move, QStringLiteral("preview-noop-commit-undo"),
            [&] {
                testRuntime.history()->reset();
                const auto base = runtime.documentVersion();
                const auto invalid = runtime.project().moveTrack(commandContext(runtime), first, 4);
                suite.expect(isError(invalid, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("target_index")) &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("out-of-range target must not mutate"));
                const auto preview =
                    runtime.project().moveTrack(commandContext(runtime, true), first, 2);
                suite.expect(preview && preview.get().validatedOnly && preview.get().changed &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("move preview must predict without mutation"));
                const auto move = runtime.project().moveTrack(commandContext(runtime), first, 2);
                const auto moved =
                    runtime.project().getProject(runtime.documentVersion().documentId);
                suite.expect(move && move.get().current.revision == base.revision + 1 && moved &&
                                 moved.get().tracks.at(1).id == first,
                             QStringLiteral("track move must update order once"));
                const auto noOp = runtime.project().moveTrack(commandContext(runtime), first, 2);
                suite.expect(noOp && !noOp.get().changed &&
                                 runtime.documentVersion() == move.get().current,
                             QStringLiteral("moving to current index must be a no-op"));
                const auto undo = runtime.history().undo(commandContext(runtime));
                const auto restored =
                    runtime.project().getProject(runtime.documentVersion().documentId);
                suite.expect(undo && undo.get().changed && restored &&
                                 restored.get().tracks.first().id == first,
                             QStringLiteral("track move must undo as one entry"));

                if (!restored || restored.get().tracks.first().id != first)
                    runtime.history().undo(commandContext(runtime));
                const auto endBase = runtime.documentVersion();
                const auto endPreview =
                    runtime.project().moveTrack(commandContext(runtime, true), first, 3);
                suite.expect(
                    endPreview && endPreview.get().changed && endPreview.get().validatedOnly &&
                        runtime.documentVersion() == endBase,
                    QStringLiteral("end insertion preview must accept index equal to size"));
                const auto moveToEnd =
                    runtime.project().moveTrack(commandContext(runtime), first, 3);
                const auto atEnd =
                    runtime.project().getProject(runtime.documentVersion().documentId);
                suite.expect(moveToEnd && atEnd && atEnd.get().tracks.last().id == first,
                             QStringLiteral("track must support insertion after the last row"));
                if (moveToEnd)
                    runtime.history().undo(commandContext(runtime));
            });

        suite.run(
            Automation::OperationIds::tracks::set_properties, QStringLiteral("atomic-properties"),
            [&] {
                testRuntime.history()->reset();
                const auto base = runtime.documentVersion();
                Automation::TrackPropertiesDto edit{
                    .id = second,
                    .name = QStringLiteral("第二轨 ☃"),
                    .gain = 0.75,
                    .pan = 0.5,
                    .mute = true,
                    .solo = false,
                };
                auto invalid = edit;
                invalid.pan = std::numeric_limits<double>::quiet_NaN();
                const auto rejected =
                    runtime.project().setTrackProperties(commandContext(runtime), invalid);
                suite.expect(isError(rejected, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("properties.control")) &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("non-finite track control must be atomic failure"));
                const auto preview =
                    runtime.project().setTrackProperties(commandContext(runtime, true), edit);
                suite.expect(preview && preview.get().changed && preview.get().validatedOnly &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("property preview must be side-effect free"));
                const auto changed =
                    runtime.project().setTrackProperties(commandContext(runtime), edit);
                const auto snapshot = trackSnapshot(runtime, second);
                suite.expect(
                    changed && changed.get().current.revision == base.revision + 1 && snapshot &&
                        snapshot->data.name == edit.name && snapshot->data.gain == edit.gain &&
                        snapshot->data.pan == edit.pan && snapshot->data.mute &&
                        snapshot->data.colorIndex == 1,
                    QStringLiteral(
                        "track property edits must commit atomically without resetting color"));
                const auto noOp =
                    runtime.project().setTrackProperties(commandContext(runtime), edit);
                suite.expect(noOp && !noOp.get().changed &&
                                 runtime.documentVersion() == changed.get().current,
                             QStringLiteral("identical track properties must be a no-op"));
            });

        suite.run(
            Automation::OperationIds::tracks::set_color, QStringLiteral("history-state-and-noop"),
            [&] {
                testRuntime.history()->reset();
                const auto base = runtime.documentVersion();
                const auto invalid =
                    runtime.project().setTrackColor(commandContext(runtime), third, -1);
                suite.expect(isError(invalid, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("color_index")),
                             QStringLiteral("negative color must be rejected"));
                const auto changed =
                    runtime.project().setTrackColor(commandContext(runtime), third, 7);
                const auto state = runtime.history().getState(runtime.documentVersion().documentId);
                const auto snapshot = trackSnapshot(runtime, third);
                suite.expect(changed && changed.get().current.revision == base.revision + 1 &&
                                 snapshot && snapshot->data.colorIndex == 7 && state &&
                                 state.get().canUndo,
                             QStringLiteral("color must advance revision with one History entry"));
                const auto noOp =
                    runtime.project().setTrackColor(commandContext(runtime), third, 7);
                suite.expect(noOp && !noOp.get().changed &&
                                 runtime.documentVersion() == changed.get().current,
                             QStringLiteral("identical color must be a no-op"));
            });

        suite.run(Automation::OperationIds::tracks::set_default_language,
                  QStringLiteral("unicode-and-noop"), [&] {
                      testRuntime.history()->reset();
                      const auto base = runtime.documentVersion();
                      const auto empty = runtime.project().setTrackDefaultLanguage(
                          commandContext(runtime), third, QStringLiteral("  "));
                      suite.expect(isError(empty, AutomationErrorCode::InvalidArgument,
                                           QStringLiteral("language")),
                                   QStringLiteral("blank language must be rejected"));
                      const auto changed = runtime.project().setTrackDefaultLanguage(
                          commandContext(runtime), third, QStringLiteral("zh-汉字"));
                      const auto snapshot = trackSnapshot(runtime, third);
                      suite.expect(changed && changed.get().current.revision == base.revision + 1 &&
                                       snapshot &&
                                       snapshot->data.defaultLanguage == QStringLiteral("zh-汉字"),
                                   QStringLiteral("Unicode language ID must round-trip"));
                      const auto noOp = runtime.project().setTrackDefaultLanguage(
                          commandContext(runtime), third, QStringLiteral("zh-汉字"));
                      const auto state =
                          runtime.history().getState(runtime.documentVersion().documentId);
                      suite.expect(noOp && !noOp.get().changed && state && state.get().canUndo,
                                   QStringLiteral("language state change must create one History entry"));
                      const auto undo = runtime.history().undo(commandContext(runtime));
                      const auto restored = trackSnapshot(runtime, third);
                      suite.expect(undo && restored &&
                                       restored->data.defaultLanguage == QStringLiteral("en"),
                                   QStringLiteral("track language change must undo atomically"));
                  });

        ClipId clip;
        suite.run(
            Automation::OperationIds::clips::insert, QStringLiteral("empty-invalid-preview-create"),
            [&] {
                const auto base = runtime.documentVersion();
                const auto empty = runtime.project().insertClips(commandContext(runtime), {});
                suite.expect(empty && !empty.get().changed && runtime.documentVersion() == base,
                             QStringLiteral("empty insert must be a no-op"));
                auto invalidDraft = singingClipDraft(QStringLiteral("Invalid"));
                invalidDraft.properties.length = -1;
                const auto invalid = runtime.project().insertClips(
                    commandContext(runtime), {
                                                 {.trackId = second, .clip = invalidDraft}
                });
                suite.expect(isError(invalid, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("clip.properties")),
                             QStringLiteral("invalid clip geometry must be rejected"));
                auto visibleOverflowDraft = singingClipDraft(QStringLiteral("Visible Overflow"));
                visibleOverflowDraft.properties.start = std::numeric_limits<int>::max();
                visibleOverflowDraft.properties.clipStart = 0;
                visibleOverflowDraft.properties.clipLen = 1;
                const auto visibleOverflow = runtime.project().insertClips(
                    commandContext(runtime),
                    {
                        {.trackId = second, .clip = visibleOverflowDraft}
                });
                suite.expect(isError(visibleOverflow, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("clip.properties")),
                             QStringLiteral("visible clip end must fit the model tick type"));
                auto localOverflowDraft = singingClipDraft(QStringLiteral("Local Overflow"));
                localOverflowDraft.properties.start = -std::numeric_limits<int>::max();
                localOverflowDraft.properties.clipStart = std::numeric_limits<int>::max();
                localOverflowDraft.properties.clipLen = 1;
                const auto localOverflow = runtime.project().insertClips(
                    commandContext(runtime), {
                                                 {.trackId = second, .clip = localOverflowDraft}
                });
                suite.expect(isError(localOverflow, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("clip.properties")),
                             QStringLiteral("clip-local end must fit the model tick type"));
                const auto draft =
                    singingClipDraft(QStringLiteral("歌声 Clip"), QStringLiteral("clip-main"));
                const auto preview = runtime.project().insertClips(
                    commandContext(runtime, true), {
                                                       {.trackId = second, .clip = draft}
                });
                suite.expect(preview && preview.get().validatedOnly &&
                                 preview.get().createdObjects.isEmpty() &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("clip preview must not allocate IDs"));
                const auto insert = runtime.project().insertClips(
                    commandContext(runtime), {
                                                 {.trackId = second, .clip = draft}
                });
                suite.expect(insert && insert.get().createdObjects.size() == 1 &&
                                 insert.get().createdObjects.first().clientRef ==
                                     QStringLiteral("clip-main"),
                             QStringLiteral("clip create must bind client_ref"));
                if (insert)
                    clip = ClipId(insert.get().affectedObjects.first().value);
            });

        suite.run(
            Automation::OperationIds::clips::set_properties,
            QStringLiteral("legacy-range-move-and-edit-atomically"), [&] {
                testRuntime.history()->reset();
                const auto before = clipSnapshot(runtime, clip);
                suite.expect(before.has_value(), QStringLiteral("fixture clip must exist"));
                auto edit = before->data.properties;
                edit.id = clip;
                edit.name = QStringLiteral("Moved Clip");
                edit.start = 960;
                edit.length = 100;
                edit.clipStart = 50;
                edit.clipLen = 100;
                edit.gain = 0.8;
                edit.mute = true;
                auto invalid = edit;
                invalid.gain = std::numeric_limits<double>::infinity();
                const auto rejected =
                    runtime.project().setClipProperties(commandContext(runtime), invalid, third);
                suite.expect(isError(rejected, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("properties")),
                             QStringLiteral("invalid clip properties must not partially move"));
                auto visibleOverflow = edit;
                visibleOverflow.start = std::numeric_limits<int>::max();
                visibleOverflow.clipStart = 0;
                visibleOverflow.clipLen = 1;
                const auto rejectedVisibleOverflow = runtime.project().setClipProperties(
                    commandContext(runtime, true), visibleOverflow, third);
                suite.expect(
                    isError(rejectedVisibleOverflow, AutomationErrorCode::InvalidArgument,
                            QStringLiteral("properties")),
                    QStringLiteral("clip edit must reject an unrepresentable visible end"));
                auto localOverflow = edit;
                localOverflow.start = -std::numeric_limits<int>::max();
                localOverflow.clipStart = std::numeric_limits<int>::max();
                localOverflow.clipLen = 1;
                const auto rejectedLocalOverflow = runtime.project().setClipProperties(
                    commandContext(runtime, true), localOverflow, third);
                suite.expect(
                    isError(rejectedLocalOverflow, AutomationErrorCode::InvalidArgument,
                            QStringLiteral("properties")),
                    QStringLiteral("clip edit must reject an unrepresentable clip-local end"));
                const auto base = runtime.documentVersion();
                const auto preview =
                    runtime.project().setClipProperties(commandContext(runtime, true), edit, third);
                suite.expect(preview && preview.get().changed && preview.get().validatedOnly &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("clip edit preview must be side-effect free"));
                const auto changed =
                    runtime.project().setClipProperties(commandContext(runtime), edit, third);
                const auto after = clipSnapshot(runtime, clip);
                suite.expect(
                    changed && changed.get().current.revision == base.revision + 1 && after &&
                        after->trackId == third && after->data.properties.name == edit.name &&
                        after->data.properties.start == edit.start &&
                        after->data.properties.length == edit.length &&
                        after->data.properties.clipStart == edit.clipStart &&
                        after->data.properties.clipLen == edit.clipLen &&
                        after->data.properties.mute,
                    QStringLiteral(
                        "legacy clip range and move must commit once without normalization"));
                const auto noOp =
                    runtime.project().setClipProperties(commandContext(runtime), edit, third);
                suite.expect(noOp && !noOp.get().changed &&
                                 runtime.documentVersion() == changed.get().current,
                             QStringLiteral("identical clip edit must be a no-op"));
                const auto undo = runtime.history().undo(commandContext(runtime));
                const auto restored = clipSnapshot(runtime, clip);
                suite.expect(undo && restored && restored->trackId == second &&
                                 restored->data.properties.name == before->data.properties.name,
                             QStringLiteral("combined clip move/edit must undo atomically"));
            });

        ClipId legacyAudioClip;
        suite.run(
            Automation::OperationIds::clips::insert, QStringLiteral("legacy-audio-range-create"),
            [&] {
                const auto insert = runtime.project().insertClips(
                    commandContext(runtime),
                    {
                        {.trackId = second,
                         .clip = audioClipDraft(QStringLiteral("Legacy Audio"),
                         QStringLiteral("legacy-audio"))}
                });
                if (insert && !insert.get().affectedObjects.isEmpty())
                    legacyAudioClip = ClipId(insert.get().affectedObjects.first().value);
                const auto snapshot = clipSnapshot(runtime, legacyAudioClip);
                suite.expect(
                    insert && snapshot &&
                        snapshot->data.type == Automation::ClipDraftDto::Type::Audio &&
                        snapshot->data.properties.length == 100 &&
                        snapshot->data.properties.clipStart == 50 &&
                        snapshot->data.properties.clipLen == 100,
                    QStringLiteral("legacy audio geometry must be inserted without normalization"));
                if (!snapshot)
                    return;

                const auto reusable = Automation::validate(snapshot->data);
                suite.expect(static_cast<bool>(reusable),
                             QStringLiteral("a legacy audio snapshot must validate for reuse"));
                auto copiedDraft = snapshot->data;
                copiedDraft.clientRef = QStringLiteral("legacy-audio-copy");
                copiedDraft.properties.start = 480;
                const auto copied = runtime.project().insertClips(
                    commandContext(runtime), {
                                                 {.trackId = second, .clip = copiedDraft}
                });
                const auto copiedSnapshot =
                    copied && !copied.get().affectedObjects.isEmpty()
                        ? clipSnapshot(runtime, ClipId(copied.get().affectedObjects.first().value))
                        : std::nullopt;
                suite.expect(
                    copied && copied.get().createdObjects.size() == 1 && copiedSnapshot &&
                        sameClipTiming(copiedSnapshot->data.properties, copiedDraft.properties),
                    QStringLiteral(
                        "a pasted legacy audio snapshot must preserve its exact timing geometry"));

                auto conflictingDraft = copiedDraft;
                conflictingDraft.clientRef = QStringLiteral("conflicting-audio-anchor");
                conflictingDraft.properties.start = 960;
                conflictingDraft.properties.length = 480;
                conflictingDraft.properties.clipStart = 0;
                conflictingDraft.properties.clipLen = 480;
                conflictingDraft.properties.trimStartMs = 0.0;
                conflictingDraft.properties.playLengthMs = 1000.0;
                conflictingDraft.properties.materialLengthMs = 1000.0;
                const auto conflicting = runtime.project().insertClips(
                    commandContext(runtime), {
                                                 {.trackId = second, .clip = conflictingDraft}
                });
                const auto conflictingSnapshot =
                    conflicting && !conflicting.get().affectedObjects.isEmpty()
                        ? clipSnapshot(runtime,
                                       ClipId(conflicting.get().affectedObjects.first().value))
                        : std::nullopt;
                suite.expect(
                    conflicting && conflictingSnapshot &&
                        conflictingSnapshot->data.properties.start +
                                conflictingSnapshot->data.properties.clipStart ==
                            conflictingDraft.properties.start +
                                conflictingDraft.properties.clipStart &&
                        conflictingSnapshot->data.properties.clipLen !=
                            conflictingDraft.properties.clipLen &&
                        conflictingSnapshot->data.properties.length >=
                            conflictingSnapshot->data.properties.clipStart +
                                conflictingSnapshot->data.properties.clipLen &&
                        conflictingSnapshot->data.properties.playLengthMs ==
                            conflictingDraft.properties.playLengthMs,
                    QStringLiteral(
                        "an inconsistent anchored draft must reconcile ticks to realtime truth"));

                auto oversizedDraft = conflictingDraft;
                oversizedDraft.clientRef = QStringLiteral("oversized-audio-material");
                oversizedDraft.properties.start = 1920;
                oversizedDraft.properties.length = 9600;
                oversizedDraft.properties.clipLen = 480;
                oversizedDraft.properties.playLengthMs = 500.0;
                oversizedDraft.properties.materialLengthMs = 500.0;
                const auto oversized = runtime.project().insertClips(
                    commandContext(runtime), {
                                                 {.trackId = second, .clip = oversizedDraft}
                });
                const auto oversizedSnapshot =
                    oversized && !oversized.get().affectedObjects.isEmpty()
                        ? clipSnapshot(runtime,
                                       ClipId(oversized.get().affectedObjects.first().value))
                        : std::nullopt;
                suite.expect(
                    oversized && oversizedSnapshot &&
                        oversizedSnapshot->data.properties.start ==
                            oversizedDraft.properties.start &&
                        oversizedSnapshot->data.properties.clipStart ==
                            oversizedDraft.properties.clipStart &&
                        oversizedSnapshot->data.properties.clipLen ==
                            oversizedDraft.properties.clipLen &&
                        oversizedSnapshot->data.properties.length !=
                            oversizedDraft.properties.length &&
                        oversizedSnapshot->data.properties.length ==
                            oversizedSnapshot->data.properties.clipStart +
                                oversizedSnapshot->data.properties.clipLen,
                    QStringLiteral(
                        "an oversized anchored length must reconcile to its material duration"));
            });

        suite.run(
            Automation::OperationIds::clips::set_properties,
            QStringLiteral("legacy-audio-metadata-edit-preserves-range"), [&] {
                testRuntime.history()->reset();
                const auto before = clipSnapshot(runtime, legacyAudioClip);
                suite.expect(before.has_value(), QStringLiteral("legacy audio fixture must exist"));
                if (!before)
                    return;
                auto edit = before->data.properties;
                edit.id = legacyAudioClip;
                edit.name = QStringLiteral("Renamed Legacy Audio");
                edit.gain = 0.75;
                const auto changed =
                    runtime.project().setClipProperties(commandContext(runtime), edit, second);
                const auto after = clipSnapshot(runtime, legacyAudioClip);
                suite.expect(
                    changed && after && after->trackId == second &&
                        sameClipTiming(after->data.properties, before->data.properties),
                    QStringLiteral(
                        "audio metadata edit must preserve legacy ticks and realtime truth"));
                const auto undoEdit = runtime.history().undo(commandContext(runtime));
                const auto restored = clipSnapshot(runtime, legacyAudioClip);
                suite.expect(undoEdit && restored && restored->trackId == second &&
                                 sameClipTiming(restored->data.properties, before->data.properties),
                             QStringLiteral("undo must restore legacy audio geometry exactly"));

                testRuntime.history()->reset();
                auto moveProperties = before->data.properties;
                moveProperties.id = legacyAudioClip;
                const auto moved = runtime.project().setClipProperties(commandContext(runtime),
                                                                       moveProperties, third);
                const auto movedSnapshot = clipSnapshot(runtime, legacyAudioClip);
                suite.expect(
                    moved && movedSnapshot && movedSnapshot->trackId == third &&
                        sameClipTiming(movedSnapshot->data.properties, before->data.properties),
                    QStringLiteral("cross-track move must preserve legacy audio geometry"));
                const auto undoMove = runtime.history().undo(commandContext(runtime));
                const auto movedBack = clipSnapshot(runtime, legacyAudioClip);
                suite.expect(
                    undoMove && movedBack && movedBack->trackId == second &&
                        sameClipTiming(movedBack->data.properties, before->data.properties),
                    QStringLiteral("undoing a cross-track move must preserve legacy geometry"));
            });

        suite.run(
            Automation::OperationIds::clips::set_properties,
            QStringLiteral("legacy-audio-timing-edit-undo-restores-raw-range"), [&] {
                testRuntime.history()->reset();
                const auto before = clipSnapshot(runtime, legacyAudioClip);
                suite.expect(before.has_value(), QStringLiteral("legacy audio fixture must exist"));
                if (!before)
                    return;
                auto edit = before->data.properties;
                edit.id = legacyAudioClip;
                edit.start += 240;
                const auto changed =
                    runtime.project().setClipProperties(commandContext(runtime), edit, second);
                const auto undo = runtime.history().undo(commandContext(runtime));
                const auto restored = clipSnapshot(runtime, legacyAudioClip);
                suite.expect(
                    changed && undo && restored && restored->trackId == second &&
                        sameClipTiming(restored->data.properties, before->data.properties),
                    QStringLiteral("undoing an audio timing edit must restore raw legacy ticks"));
            });

        suite.run(
            Automation::OperationIds::clips::set_properties,
            QStringLiteral("legacy-audio-timing-move-undo-restores-raw-range"), [&] {
                testRuntime.history()->reset();
                const auto before = clipSnapshot(runtime, legacyAudioClip);
                suite.expect(before.has_value(), QStringLiteral("legacy audio fixture must exist"));
                if (!before)
                    return;
                auto edit = before->data.properties;
                edit.id = legacyAudioClip;
                edit.start += 480;
                const auto changed =
                    runtime.project().setClipProperties(commandContext(runtime), edit, third);
                const auto undo = runtime.history().undo(commandContext(runtime));
                const auto restored = clipSnapshot(runtime, legacyAudioClip);
                suite.expect(
                    changed && undo && restored && restored->trackId == second &&
                        sameClipTiming(restored->data.properties, before->data.properties),
                    QStringLiteral("undoing an audio timing move must restore raw legacy ticks"));
            });

        suite.run(Automation::OperationIds::clips::set_default_language,
                  QStringLiteral("validation-revision-history"), [&] {
                      testRuntime.history()->reset();
                      const auto blank = runtime.project().setSingingClipDefaultLanguage(
                          commandContext(runtime), clip, QStringLiteral(""));
                      suite.expect(isError(blank, AutomationErrorCode::InvalidArgument,
                                           QStringLiteral("language")),
                                   QStringLiteral("empty clip language must be rejected"));
                      const auto base = runtime.documentVersion();
                      const auto changed = runtime.project().setSingingClipDefaultLanguage(
                          commandContext(runtime), clip, QStringLiteral("ja"));
                      const auto snapshot = clipSnapshot(runtime, clip);
                      const auto state =
                          runtime.history().getState(runtime.documentVersion().documentId);
                      suite.expect(
                          changed && changed.get().current.revision == base.revision + 1 &&
                              snapshot && snapshot->data.defaultLanguage == QStringLiteral("ja") &&
                              state && state.get().canUndo,
                          QStringLiteral("clip language must advance revision with History"));
                      const auto noOp = runtime.project().setSingingClipDefaultLanguage(
                          commandContext(runtime), clip, QStringLiteral("ja"));
                      suite.expect(noOp && !noOp.get().changed,
                                   QStringLiteral("identical clip language must be a no-op"));
                      const auto undo = runtime.history().undo(commandContext(runtime));
                      const auto restored = clipSnapshot(runtime, clip);
                      suite.expect(undo && restored &&
                                       restored->data.defaultLanguage == QStringLiteral("en"),
                                   QStringLiteral("clip language change must undo atomically"));
                  });

        suite.run(
            Automation::OperationIds::clips::remove, QStringLiteral("duplicates-preview-undo"),
            [&] {
                testRuntime.history()->reset();
                const auto duplicate =
                    runtime.project().removeClips(commandContext(runtime), {clip, clip});
                suite.expect(isError(duplicate, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("clip_ids")),
                             QStringLiteral("duplicate clip IDs must fail atomically"));
                const auto empty = runtime.project().removeClips(commandContext(runtime), {});
                suite.expect(empty && !empty.get().changed,
                             QStringLiteral("empty clip removal must be a no-op"));
                const auto base = runtime.documentVersion();
                const auto preview =
                    runtime.project().removeClips(commandContext(runtime, true), {clip});
                suite.expect(preview && preview.get().changed && preview.get().validatedOnly &&
                                 clipSnapshot(runtime, clip).has_value() &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("clip removal preview must preserve clip"));
                const auto removed = runtime.project().removeClips(commandContext(runtime), {clip});
                suite.expect(removed && !clipSnapshot(runtime, clip).has_value() &&
                                 removed.get().current.revision == base.revision + 1,
                             QStringLiteral("clip removal must commit once"));
                const auto undo = runtime.history().undo(commandContext(runtime));
                suite.expect(undo && clipSnapshot(runtime, clip).has_value(),
                             QStringLiteral("clip removal must be reversible"));
            });

        suite.run(
            Automation::OperationIds::tracks::remove, QStringLiteral("duplicates-preview-undo"),
            [&] {
                testRuntime.history()->reset();
                const auto duplicate =
                    runtime.project().removeTracks(commandContext(runtime), {third, third});
                suite.expect(isError(duplicate, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("track_ids")),
                             QStringLiteral("duplicate track IDs must fail atomically"));
                const auto empty = runtime.project().removeTracks(commandContext(runtime), {});
                suite.expect(empty && !empty.get().changed,
                             QStringLiteral("empty track removal must be a no-op"));
                const auto base = runtime.documentVersion();
                const auto preview =
                    runtime.project().removeTracks(commandContext(runtime, true), {third});
                suite.expect(preview && preview.get().changed && preview.get().validatedOnly &&
                                 trackSnapshot(runtime, third).has_value() &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("track removal preview must preserve track"));
                const auto removed =
                    runtime.project().removeTracks(commandContext(runtime), {third});
                suite.expect(removed && !trackSnapshot(runtime, third).has_value() &&
                                 removed.get().current.revision == base.revision + 1,
                             QStringLiteral("track removal must commit once"));
                const auto undo = runtime.history().undo(commandContext(runtime));
                suite.expect(undo && trackSnapshot(runtime, third).has_value() &&
                                 clipSnapshot(runtime, clip).has_value(),
                             QStringLiteral("track removal must restore child clips on undo"));
            });
    }

    struct NoteFixture final {
        TestRuntime testRuntime;
        TrackId trackId;
        ClipId clipId;
        NoteId firstNoteId;
        NoteId secondNoteId;

        NoteFixture() {
            auto &runtime = testRuntime.runtime();
            trackId = insertedTrack(runtime, QStringLiteral("Notes"));
            clipId = insertedSingingClip(runtime, trackId, QStringLiteral("Notes Clip"));
            auto first = noteDraft(73, 407, 60, QStringLiteral("la"), QStringLiteral("note-a"));
            first.pronunciation.original = QStringLiteral("la");
            first.pronunciation.edited = QStringLiteral("custom-la");
            first.pronunciationCandidates = {QStringLiteral("la"), QStringLiteral("lah")};
            PhonemeName onset;
            onset.language = QStringLiteral("en");
            onset.name = QStringLiteral("l");
            onset.isOnset = true;
            PhonemeName vowel;
            vowel.language = QStringLiteral("en");
            vowel.name = QStringLiteral("a");
            first.phonemes.nameSeq.edited = {onset, vowel};
            first.phonemes.offsetSeq.original = {-40, 80};
            const auto ids = insertedNotes(
                runtime, clipId,
                {first, noteDraft(600, 360, 64, QStringLiteral("mi"), QStringLiteral("note-b"))});
            if (ids.size() == 2) {
                firstNoteId = ids.at(0);
                secondNoteId = ids.at(1);
            }
            testRuntime.history()->reset();
        }
    };

    void testNoteDomain(Suite &suite) {
        NoteFixture fixture;
        auto &runtime = fixture.testRuntime.runtime();

        suite.run(
            Automation::OperationIds::notes::list, QStringLiteral("typed-ordered-snapshot"), [&] {
                const auto notes =
                    runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.clipId);
                suite.expect(notes && notes.get().size() == 2 &&
                                 notes.get().first().id == fixture.firstNoteId &&
                                 notes.get().last().id == fixture.secondNoteId &&
                                 notes.get().first().data.lyric == QStringLiteral("la") &&
                                 notes.get().first().data.clientRef.isEmpty(),
                             QStringLiteral("note query must return ordered value DTOs"));
                const auto wrong = runtime.notes().getNotes(Automation::DocumentId::create(),
                                                            Automation::ClipId(999999));
                suite.expect(
                    !wrong && wrong.getError().code == AutomationErrorCode::DocumentChanged &&
                        wrong.getError().operationId == Automation::OperationIds::notes::list,
                    QStringLiteral("document validation must precede clip resolution"));
            });

        suite.run(
            Automation::OperationIds::notes::insert,
            QStringLiteral("empty-invalid-preview-overlap"), [&] {
                const auto base = runtime.documentVersion();
                const auto empty =
                    runtime.notes().insertNotes(commandContext(runtime), fixture.clipId, {});
                suite.expect(empty && !empty.get().changed && runtime.documentVersion() == base,
                             QStringLiteral("empty note insert must be a no-op"));
                const auto invalid =
                    runtime.notes().insertNotes(commandContext(runtime), fixture.clipId,
                                                {noteDraft(1000, 0, 60, QStringLiteral("bad"))});
                suite.expect(
                    isError(invalid, AutomationErrorCode::InvalidArgument, QStringLiteral("notes")),
                    QStringLiteral("zero-length note must be rejected"));
                const auto candidate =
                    noteDraft(1200, 240, 67, QStringLiteral("so"), QStringLiteral("note-c"));
                const auto preview = runtime.notes().insertNotes(commandContext(runtime, true),
                                                                 fixture.clipId, {candidate});
                suite.expect(preview && preview.get().changed && preview.get().validatedOnly &&
                                 preview.get().createdObjects.isEmpty() &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("note insert preview must not allocate"));
                const auto insert = runtime.notes().insertNotes(commandContext(runtime),
                                                                fixture.clipId, {candidate});
                suite.expect(insert && insert.get().createdObjects.size() == 1 &&
                                 insert.get().createdObjects.first().clientRef ==
                                     QStringLiteral("note-c") &&
                                 insert.get().createdObjects.first().object.kind ==
                                     Automation::ObjectKind::Note &&
                                 insert.get().createdObjects.first().object ==
                                     insert.get().affectedObjects.first(),
                             QStringLiteral("note insert must return client binding"));

                const auto overlapDraft = noteDraft(200, 300, 62, QStringLiteral("overlap"),
                                                    QStringLiteral("note-overlap"));
                const auto overlapPreview = runtime.notes().insertNotes(
                    commandContext(runtime, true), fixture.clipId, {overlapDraft});
                const auto overlapBase = runtime.documentVersion();
                const auto overlap = runtime.notes().insertNotes(commandContext(runtime),
                                                                 fixture.clipId, {overlapDraft});
                const auto overlappedNotes =
                    runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.clipId);
                suite.expect(overlapPreview && overlapPreview.get().changed &&
                                 overlapPreview.get().validatedOnly && overlap &&
                                 overlap.get().current.revision == overlapBase.revision + 1 &&
                                 overlappedNotes && overlappedNotes.get().size() == 4,
                             QStringLiteral("overlapping note insertion must remain an atomic, "
                                            "addressable edit"));
                const auto undoOverlap = runtime.history().undo(commandContext(runtime));
                const auto afterUndo =
                    runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.clipId);
                suite.expect(undoOverlap && afterUndo && afterUndo.get().size() == 3,
                             QStringLiteral("overlapping insertion must undo once"));
            });

        suite.run(
            Automation::OperationIds::notes::move, QStringLiteral("bounds-duplicates-noop-undo"),
            [&] {
                fixture.testRuntime.history()->reset();
                const auto base = runtime.documentVersion();
                const auto duplicate =
                    runtime.notes().moveNotes(commandContext(runtime), fixture.clipId,
                                              {fixture.firstNoteId, fixture.firstNoteId}, 1, 0);
                suite.expect(isError(duplicate, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("note_ids")),
                             QStringLiteral("duplicate note IDs must be rejected"));
                const auto invalidKey = runtime.notes().moveNotes(
                    commandContext(runtime), fixture.clipId, {fixture.firstNoteId}, 0, 100);
                suite.expect(isError(invalidKey, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("delta_key")),
                             QStringLiteral("out-of-range key must be rejected"));
                const auto noOp = runtime.notes().moveNotes(commandContext(runtime), fixture.clipId,
                                                            {fixture.firstNoteId}, 0, 0);
                suite.expect(noOp && !noOp.get().changed && runtime.documentVersion() == base,
                             QStringLiteral("zero move must be a no-op"));
                const auto preview =
                    runtime.notes().moveNotes(commandContext(runtime, true), fixture.clipId,
                                              {fixture.firstNoteId, fixture.secondNoteId}, 20, 1);
                suite.expect(preview && preview.get().changed && preview.get().validatedOnly &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("move preview must preserve notes"));
                const auto moved =
                    runtime.notes().moveNotes(commandContext(runtime), fixture.clipId,
                                              {fixture.firstNoteId, fixture.secondNoteId}, 20, 1);
                const auto first = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
                const auto second = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
                suite.expect(moved && moved.get().current.revision == base.revision + 1 && first &&
                                 second && first->data.localStart == 93 &&
                                 second->data.localStart == 620 && first->data.keyIndex == 61 &&
                                 second->data.keyIndex == 65,
                             QStringLiteral("multi-note move must be one atomic revision"));
                const auto undo = runtime.history().undo(commandContext(runtime));
                const auto restored = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
                suite.expect(undo && restored && restored->data.localStart == 73 &&
                                 restored->data.keyIndex == 60,
                             QStringLiteral("multi-note move must undo atomically"));

                const auto overlapPreview = runtime.notes().moveNotes(
                    commandContext(runtime, true), fixture.clipId, {fixture.secondNoteId}, -300, 0);
                const auto overlapBase = runtime.documentVersion();
                const auto overlap = runtime.notes().moveNotes(
                    commandContext(runtime), fixture.clipId, {fixture.secondNoteId}, -300, 0);
                const auto overlapped = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
                suite.expect(overlapPreview && overlapPreview.get().changed && overlap &&
                                 overlap.get().current.revision == overlapBase.revision + 1 &&
                                 overlapped && overlapped->data.localStart == 300,
                             QStringLiteral("move into overlap must remain an atomic edit"));
                const auto undoOverlap = runtime.history().undo(commandContext(runtime));
                const auto afterUndo = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
                suite.expect(undoOverlap && afterUndo && afterUndo->data.localStart == 600,
                             QStringLiteral("overlapping move must undo once"));
            });

        suite.run(
            Automation::OperationIds::notes::resize_left, QStringLiteral("clamp-preview-commit"),
            [&] {
                fixture.testRuntime.history()->reset();
                const auto invalid = runtime.notes().resizeNotesLeft(
                    commandContext(runtime), fixture.clipId, {fixture.firstNoteId}, 10, 0);
                suite.expect(isError(invalid, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("resize")),
                             QStringLiteral("non-positive minimum length must be rejected"));
                const auto duplicate = runtime.notes().resizeNotesLeft(
                    commandContext(runtime), fixture.clipId,
                    {fixture.firstNoteId, fixture.firstNoteId}, 10, 1);
                suite.expect(isError(duplicate, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("resize")),
                             QStringLiteral("duplicate resize IDs must be rejected"));
                const auto base = runtime.documentVersion();
                const auto preview = runtime.notes().resizeNotesLeft(
                    commandContext(runtime, true), fixture.clipId, {fixture.firstNoteId}, 20, 120);
                suite.expect(preview && preview.get().changed && runtime.documentVersion() == base,
                             QStringLiteral("left-resize preview must not mutate"));
                const auto changed = runtime.notes().resizeNotesLeft(
                    commandContext(runtime), fixture.clipId, {fixture.firstNoteId}, 20, 120);
                const auto note = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
                suite.expect(changed && changed.get().current.revision == base.revision + 1 &&
                                 note && note->data.localStart == 93 && note->data.length == 387,
                             QStringLiteral("left resize must update start and length once"));
                const auto noOp = runtime.notes().resizeNotesLeft(commandContext(runtime),
                                                                  fixture.clipId, {}, 20, 120);
                suite.expect(noOp && !noOp.get().changed,
                             QStringLiteral("empty left resize must be a no-op"));
            });

        suite.run(
            Automation::OperationIds::notes::resize_right, QStringLiteral("clamp-preview-commit"),
            [&] {
                fixture.testRuntime.history()->reset();
                const auto invalid = runtime.notes().resizeNotesRight(
                    commandContext(runtime), fixture.clipId, {fixture.firstNoteId}, -10, 0);
                suite.expect(isError(invalid, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("resize")),
                             QStringLiteral("non-positive minimum length must be rejected"));
                const auto base = runtime.documentVersion();
                const auto preview = runtime.notes().resizeNotesRight(
                    commandContext(runtime, true), fixture.clipId, {fixture.firstNoteId}, 60, 120);
                suite.expect(preview && preview.get().changed && runtime.documentVersion() == base,
                             QStringLiteral("right-resize preview must not mutate"));
                const auto changed = runtime.notes().resizeNotesRight(
                    commandContext(runtime), fixture.clipId, {fixture.firstNoteId}, 60, 120);
                const auto note = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
                suite.expect(changed && changed.get().current.revision == base.revision + 1 &&
                                 note && note->data.length == 447,
                             QStringLiteral("right resize must change length once"));
                const auto noOp = runtime.notes().resizeNotesRight(commandContext(runtime),
                                                                   fixture.clipId, {}, 60, 120);
                suite.expect(noOp && !noOp.get().changed,
                             QStringLiteral("empty right resize must be a no-op"));
            });

        suite.run(
            Automation::OperationIds::notes::split, QStringLiteral("invalid-preview-create-undo"),
            [&] {
                fixture.testRuntime.history()->reset();
                const auto before = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
                const auto child =
                    noteDraft(before->data.localStart + 180, 180, before->data.keyIndex,
                              QStringLiteral("+"), QStringLiteral("split-child"));
                const auto invalid =
                    runtime.notes().splitNote(commandContext(runtime), fixture.clipId,
                                              fixture.secondNoteId, child, before->data.length);
                suite.expect(
                    isError(invalid, AutomationErrorCode::InvalidArgument, QStringLiteral("split")),
                    QStringLiteral("split length must remain inside original"));
                const auto base = runtime.documentVersion();
                const auto preview =
                    runtime.notes().splitNote(commandContext(runtime, true), fixture.clipId,
                                              fixture.secondNoteId, child, 180);
                suite.expect(preview && preview.get().validatedOnly && preview.get().changed &&
                                 preview.get().createdObjects.isEmpty() &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("split preview must not allocate child ID"));
                const auto split = runtime.notes().splitNote(
                    commandContext(runtime), fixture.clipId, fixture.secondNoteId, child, 180);
                const auto original = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
                suite.expect(split && split.get().createdObjects.size() == 1 && original &&
                                 original->data.length == 180 &&
                                 split.get().createdObjects.first().clientRef ==
                                     QStringLiteral("split-child") &&
                                 split.get().createdObjects.first().object.kind ==
                                     Automation::ObjectKind::Note &&
                                 split.get().createdObjects.first().object ==
                                     split.get().affectedObjects.last(),
                             QStringLiteral("split must shorten original and bind child"));
                const auto childId =
                    split ? NoteId(split.get().createdObjects.first().object.value) : NoteId();
                suite.expect(childId.isValid() &&
                                 noteSnapshot(runtime, fixture.clipId, childId).has_value(),
                             QStringLiteral("split child must be queryable"));
                const auto undo = runtime.history().undo(commandContext(runtime));
                const auto restored = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
                suite.expect(undo && restored && restored->data.length == before->data.length &&
                                 !noteSnapshot(runtime, fixture.clipId, childId).has_value(),
                             QStringLiteral("split must undo as one History entry"));
            });

        suite.run(
            Automation::OperationIds::notes::set_phoneme_offsets,
            QStringLiteral("shape-order-preview-noop"), [&] {
                fixture.testRuntime.history()->reset();
                const auto base = runtime.documentVersion();
                const auto badCount = runtime.notes().setPhonemeOffsets(
                    commandContext(runtime, true), fixture.clipId, fixture.firstNoteId, {0});
                suite.expect(isError(badCount, AutomationErrorCode::InvalidArgument),
                             QStringLiteral("offset count must match effective phoneme count"));
                const auto unordered = runtime.notes().setPhonemeOffsets(
                    commandContext(runtime, true), fixture.clipId, fixture.firstNoteId, {120, -20});
                suite.expect(isError(unordered, AutomationErrorCode::InvalidArgument),
                             QStringLiteral("phoneme offsets must be monotonic"));
                const auto preview = runtime.notes().setPhonemeOffsets(
                    commandContext(runtime, true), fixture.clipId, fixture.firstNoteId, {-20, 120});
                suite.expect(preview && preview.get().changed && preview.get().validatedOnly &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("valid offsets preview must not mutate"));
                const auto changed = runtime.notes().setPhonemeOffsets(
                    commandContext(runtime), fixture.clipId, fixture.firstNoteId, {-20, 120});
                const auto note = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
                suite.expect(changed && changed.get().current.revision == base.revision + 1 &&
                                 note &&
                                 note->data.phonemes.offsetSeq.edited == QList<int>({-20, 120}),
                             QStringLiteral("valid edited offsets must round-trip"));
                const auto noOp = runtime.notes().setPhonemeOffsets(
                    commandContext(runtime), fixture.clipId, fixture.firstNoteId, {-20, 120});
                suite.expect(noOp && !noOp.get().changed,
                             QStringLiteral("identical offsets must be a no-op"));
                const auto clear = runtime.notes().setPhonemeOffsets(
                    commandContext(runtime), fixture.clipId, fixture.firstNoteId, {});
                const auto cleared = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
                suite.expect(clear && clear.get().changed && cleared &&
                                 cleared->data.phonemes.offsetSeq.edited.isEmpty(),
                             QStringLiteral("empty offsets must explicitly clear the edit"));
            });

        suite.run(
            Automation::OperationIds::notes::reset_phoneme_offsets,
            QStringLiteral("cascade-preview-commit-undo"), [&] {
                PhonemeName onset;
                onset.language = QStringLiteral("en");
                onset.name = QStringLiteral("l");
                onset.isOnset = true;
                PhonemeName vowel;
                vowel.language = QStringLiteral("en");
                vowel.name = QStringLiteral("a");

                Phonemes firstPhonemes;
                firstPhonemes.nameSeq.original = {onset, vowel};
                firstPhonemes.offsetSeq.original = {-40, 700};
                firstPhonemes.offsetSeq.edited = {-20, 350};
                Phonemes secondPhonemes;
                secondPhonemes.nameSeq.original = {onset, vowel};
                secondPhonemes.offsetSeq.original = {0, 200};
                secondPhonemes.offsetSeq.edited = {-50, 200};
                const auto firstSeed = runtime.notes().setPhonemes(
                    commandContext(runtime), fixture.clipId, fixture.firstNoteId, firstPhonemes);
                const auto secondSeed = runtime.notes().setPhonemes(
                    commandContext(runtime), fixture.clipId, fixture.secondNoteId, secondPhonemes);
                suite.expect(firstSeed && secondSeed,
                             QStringLiteral("cascade fixture must seed both edited words"));
                fixture.testRuntime.history()->reset();

                const auto base = runtime.documentVersion();
                const auto preview = runtime.notes().resetPhonemeOffsets(
                    commandContext(runtime, true), fixture.clipId, {fixture.firstNoteId});
                const auto previewFirst =
                    noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
                const auto previewSecond =
                    noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
                suite.expect(
                    preview && preview.get().changed && preview.get().validatedOnly &&
                        preview.get().affectedObjects.size() == 2 && previewFirst &&
                        previewSecond &&
                        previewFirst->data.phonemes.offsetSeq.edited ==
                            firstPhonemes.offsetSeq.edited &&
                        previewSecond->data.phonemes.offsetSeq.edited ==
                            secondPhonemes.offsetSeq.edited &&
                        runtime.documentVersion() == base,
                    QStringLiteral("cascade preview must report both roots without mutation"));

                const auto changed = runtime.notes().resetPhonemeOffsets(
                    commandContext(runtime), fixture.clipId, {fixture.firstNoteId});
                const auto resetFirst = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
                const auto resetSecond =
                    noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
                suite.expect(changed && changed.get().current.revision == base.revision + 1 &&
                                 changed.get().affectedObjects.size() == 2 && resetFirst &&
                                 resetSecond &&
                                 resetFirst->data.phonemes.offsetSeq.edited.isEmpty() &&
                                 resetSecond->data.phonemes.offsetSeq.edited.isEmpty(),
                             QStringLiteral("cascade reset must commit both words atomically"));

                const auto undo = runtime.history().undo(commandContext(runtime));
                const auto restoredFirst =
                    noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
                const auto restoredSecond =
                    noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
                suite.expect(undo && restoredFirst && restoredSecond &&
                                 restoredFirst->data.phonemes.offsetSeq.edited ==
                                     firstPhonemes.offsetSeq.edited &&
                                 restoredSecond->data.phonemes.offsetSeq.edited ==
                                     secondPhonemes.offsetSeq.edited,
                             QStringLiteral("cascade reset must restore every word in one undo"));
            });

        suite.run(
            Automation::OperationIds::notes::set_word_properties,
            QStringLiteral("unicode-cascade-atomic-noop"), [&] {
                fixture.testRuntime.history()->reset();
                const auto duplicate = runtime.notes().setWordProperties(
                    commandContext(runtime), fixture.clipId,
                    {{.noteId = fixture.firstNoteId}, {.noteId = fixture.firstNoteId}});
                suite.expect(isError(duplicate, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("edits")),
                             QStringLiteral("duplicate word edits must be rejected"));
                Automation::NoteWordEditDto edit;
                edit.noteId = fixture.firstNoteId;
                edit.lyric = QStringLiteral("  你好  ");
                edit.language = QStringLiteral("zh");
                const auto base = runtime.documentVersion();
                const auto preview = runtime.notes().setWordProperties(
                    commandContext(runtime, true), fixture.clipId, {edit});
                suite.expect(preview && preview.get().changed && preview.get().validatedOnly &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("word preview must preserve original"));
                const auto changed = runtime.notes().setWordProperties(commandContext(runtime),
                                                                       fixture.clipId, {edit});
                const auto note = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
                suite.expect(
                    changed && note && note->data.lyric == QStringLiteral("你好") &&
                        note->data.language == QStringLiteral("zh") &&
                        note->data.pronunciation.edited.isEmpty() &&
                        note->data.pronunciationCandidates.isEmpty() &&
                        note->data.phonemes.nameSeq.result().isEmpty(),
                    QStringLiteral("word input change must trim Unicode and cascade reset"));
                Automation::NoteWordEditDto identical;
                identical.noteId = fixture.firstNoteId;
                identical.lyric = note->data.lyric;
                identical.language = note->data.language;
                identical.pronunciation = note->data.pronunciation;
                identical.pronunciationCandidates = note->data.pronunciationCandidates;
                identical.phonemes = note->data.phonemes;
                identical.replacePronunciation = true;
                identical.replacePronunciationCandidates = true;
                const auto noOp = runtime.notes().setWordProperties(commandContext(runtime),
                                                                    fixture.clipId, {identical});
                suite.expect(noOp && !noOp.get().changed,
                             QStringLiteral("identical word properties must be a no-op"));
            });

        suite.run(
            Automation::OperationIds::notes::remove, QStringLiteral("duplicates-preview-undo"),
            [&] {
                fixture.testRuntime.history()->reset();
                const auto duplicate =
                    runtime.notes().removeNotes(commandContext(runtime), fixture.clipId,
                                                {fixture.firstNoteId, fixture.firstNoteId});
                suite.expect(isError(duplicate, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("note_ids")),
                             QStringLiteral("duplicate removal IDs must be rejected"));
                const auto empty =
                    runtime.notes().removeNotes(commandContext(runtime), fixture.clipId, {});
                suite.expect(empty && !empty.get().changed,
                             QStringLiteral("empty note removal must be a no-op"));
                const auto base = runtime.documentVersion();
                const auto preview = runtime.notes().removeNotes(
                    commandContext(runtime, true), fixture.clipId, {fixture.secondNoteId});
                suite.expect(
                    preview && preview.get().changed && preview.get().validatedOnly &&
                        noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId).has_value() &&
                        runtime.documentVersion() == base,
                    QStringLiteral("note removal preview must preserve note"));
                const auto removed = runtime.notes().removeNotes(
                    commandContext(runtime), fixture.clipId, {fixture.secondNoteId});
                suite.expect(
                    removed &&
                        !noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId).has_value(),
                    QStringLiteral("note removal must delete requested note"));
                const auto undo = runtime.history().undo(commandContext(runtime));
                suite.expect(
                    undo && noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId).has_value(),
                    QStringLiteral("note removal must be reversible"));
            });
    }

    SpeakerInfo speaker(const QString &id) {
        return SpeakerInfo(id, id.toUpper());
    }

    SingerInfo singer(const QString &id, const QList<SpeakerInfo> &speakers) {
        return SingerInfo({id, QStringLiteral("package"), QVersionNumber(1, 0)}, id.toUpper(),
                          speakers);
    }

    SpeakerMixModel::SpeakerMixData fixedMix(const SpeakerInfo &first, const SpeakerInfo &second) {
        SpeakerMixModel::SpeakerMixData data;
        data.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
        data.sources = {{first}, {second}};
        data.fixedWeights = {2.0};
        data.sourcePresetId = QStringLiteral(" preset ");
        data.sourcePresetName = QStringLiteral(" blend ");
        return data;
    }

    SpeakerMixModel::SpeakerMixData dynamicMix(const SpeakerInfo &first,
                                               const SpeakerInfo &second) {
        SpeakerMixModel::SpeakerMixData data;
        data.mode = SpeakerMixModel::SingerSourceMode::DynamicMix;
        data.sources = {{first}, {second}};
        data.dynamicKeyframes = {
            {960, {1.0}},
            {0,   {0.0}}
        };
        return data;
    }

    void testParameterAndSpeakerDomain(Suite &suite) {
        TestRuntime testRuntime;
        auto &runtime = testRuntime.runtime();
        const auto trackId = insertedTrack(runtime, QStringLiteral("Voice"));
        const auto clipId = insertedSingingClip(runtime, trackId, QStringLiteral("Voice Clip"));
        testRuntime.history()->reset();

        suite.run(
            Automation::OperationIds::parameters::get, QStringLiteral("empty-invalid-wrong-type"),
            [&] {
                const auto empty = runtime.parameters().getParameter(
                    runtime.documentVersion().documentId, clipId, ParamInfo::Pitch, Param::Edited);
                suite.expect(empty && empty.get().curves.isEmpty() &&
                                 empty.get().document == runtime.documentVersion(),
                             QStringLiteral("empty parameter must be a valid snapshot"));
                const auto unsupported =
                    runtime.parameters().getParameter(runtime.documentVersion().documentId, clipId,
                                                      ParamInfo::Unknown, Param::Edited);
                suite.expect(!unsupported &&
                                 unsupported.getError().code ==
                                     AutomationErrorCode::InvalidArgument &&
                                 unsupported.getError().fieldPath == QStringLiteral("parameter"),
                             QStringLiteral("unsupported parameter query must fail"));
            });

        suite.run(
            Automation::OperationIds::parameters::replace,
            QStringLiteral("validate-roundtrip-noop-undo"), [&] {
                Automation::CurveDraftDto draw;
                draw.type = Automation::CurveDraftDto::Type::Draw;
                draw.localStart = 10;
                draw.step = 5;
                draw.values = {6000, 6010, 6020};
                Automation::CurveDraftDto anchor;
                anchor.type = Automation::CurveDraftDto::Type::Anchor;
                anchor.localStart = 20;
                anchor.nodes = {
                    {0,   10, AnchorNode::Linear },
                    {120, 20, AnchorNode::Hermite}
                };
                const auto base = runtime.documentVersion();
                const auto preview = runtime.parameters().replaceParameter(
                    commandContext(runtime, true), clipId, ParamInfo::Pitch, Param::Edited,
                    {draw, anchor});
                suite.expect(preview && preview.get().changed && preview.get().validatedOnly &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("parameter preview must not mutate"));
                const auto changed = runtime.parameters().replaceParameter(
                    commandContext(runtime), clipId, ParamInfo::Pitch, Param::Edited,
                    {draw, anchor});
                const auto snapshot = runtime.parameters().getParameter(
                    runtime.documentVersion().documentId, clipId, ParamInfo::Pitch, Param::Edited);
                suite.expect(changed && changed.get().current.revision == base.revision + 1 &&
                                 snapshot && snapshot.get().curves.size() == 2 &&
                                 snapshot.get().curves.at(0).values == draw.values &&
                                 snapshot.get().curves.at(1).nodes.size() == 2,
                             QStringLiteral("draw and anchor curves must round-trip atomically"));
                const auto noOp = runtime.parameters().replaceParameter(
                    commandContext(runtime), clipId, ParamInfo::Pitch, Param::Edited,
                    {draw, anchor});
                suite.expect(noOp && !noOp.get().changed,
                             QStringLiteral("identical parameter replacement must be a no-op"));
                const auto undo = runtime.history().undo(commandContext(runtime));
                const auto restored = runtime.parameters().getParameter(
                    runtime.documentVersion().documentId, clipId, ParamInfo::Pitch, Param::Edited);
                suite.expect(undo && restored && restored.get().curves.isEmpty(),
                             QStringLiteral("parameter replacement must undo once"));

                auto invalid = draw;
                invalid.step = 0;
                const auto invalidStep = runtime.parameters().replaceParameter(
                    commandContext(runtime, true), clipId, ParamInfo::Pitch, Param::Edited,
                    {invalid});
                suite.expect(isError(invalidStep, AutomationErrorCode::InvalidArgument),
                             QStringLiteral("draw curve step must be positive"));
            });

        const auto speakerA = speaker(QStringLiteral("speaker-a"));
        const auto speakerB = speaker(QStringLiteral("speaker-b"));
        const auto singerA = singer(QStringLiteral("singer-a"), {speakerA, speakerB});
        const auto fixed = fixedMix(speakerA, speakerB);
        const auto dynamic = dynamicMix(speakerA, speakerB);

        suite.run(Automation::OperationIds::tracks::set_voice,
                  QStringLiteral("preview-commit-noop"), [&] {
                      testRuntime.history()->reset();
                      const auto base = runtime.documentVersion();
                      const auto preview = runtime.parameters().selectTrackSingleSpeaker(
                          commandContext(runtime, true), trackId, singerA, speakerA);
                      suite.expect(preview && preview.get().changed &&
                                       runtime.documentVersion() == base,
                                   QStringLiteral("track speaker preview must not mutate"));
                      const auto changed = runtime.parameters().selectTrackSingleSpeaker(
                          commandContext(runtime), trackId, singerA, speakerA);
                      const auto snapshot = trackSnapshot(runtime, trackId);
                      suite.expect(changed && changed.get().current.revision == base.revision + 1 &&
                                       snapshot && snapshot->data.singerInfo == singerA &&
                                       snapshot->data.speakerInfo == speakerA,
                                   QStringLiteral("single track voice must round-trip"));
                      const auto noOp = runtime.parameters().selectTrackSingleSpeaker(
                          commandContext(runtime), trackId, singerA, speakerA);
                      suite.expect(noOp && !noOp.get().changed,
                                   QStringLiteral("identical track speaker must be a no-op"));
                  });

        suite.run(
            Automation::OperationIds::speaker_mix::track::apply,
            QStringLiteral("normalized-fixed-mix"), [&] {
                const auto base = runtime.documentVersion();
                const auto changed = runtime.parameters().applyTrackSpeakerMix(
                    commandContext(runtime), trackId, singerA, speakerA, fixed);
                const auto snapshot = trackSnapshot(runtime, trackId);
                suite.expect(
                    changed && changed.get().current.revision == base.revision + 1 && snapshot &&
                        snapshot->data.speakerMixData.mode ==
                            SpeakerMixModel::SingerSourceMode::FixedMix &&
                        snapshot->data.speakerMixData.fixedWeights == QVector<double>({1.0}) &&
                        snapshot->data.speakerMixData.sourcePresetId == QStringLiteral("preset"),
                    QStringLiteral("track preset apply must normalize weights/metadata"));
            });

        suite.run(
            Automation::OperationIds::speaker_mix::track::replace,
            QStringLiteral("preserve-voice-change-mix"), [&] {
                const auto before = trackSnapshot(runtime, trackId);
                const auto base = runtime.documentVersion();
                const auto changed = runtime.parameters().replaceTrackSpeakerMix(
                    commandContext(runtime), trackId, dynamic);
                const auto after = trackSnapshot(runtime, trackId);
                suite.expect(
                    changed && changed.get().current.revision == base.revision + 1 && before &&
                        after && after->data.singerInfo == before->data.singerInfo &&
                        after->data.speakerInfo == before->data.speakerInfo &&
                        after->data.speakerMixData.mode ==
                            SpeakerMixModel::SingerSourceMode::DynamicMix &&
                        after->data.speakerMixData.dynamicKeyframes.first().tick == 0,
                    QStringLiteral("track mix replacement must preserve voice and sort keys"));
            });

        suite.run(Automation::OperationIds::clips::set_voice,
                  QStringLiteral("owned-context"), [&] {
                      const auto base = runtime.documentVersion();
                      const auto changed = runtime.parameters().selectClipSingleSpeaker(
                          commandContext(runtime), clipId, singerA, speakerB);
                      const auto snapshot = clipSnapshot(runtime, clipId);
                      suite.expect(
                          changed && changed.get().current.revision == base.revision + 1 &&
                              snapshot && !snapshot->data.usesTrackVoiceContext &&
                              snapshot->data.ownSingerInfo == singerA &&
                              snapshot->data.ownSpeakerInfo == speakerB,
                          QStringLiteral("clip speaker selection must establish owned context"));
                  });

        suite.run(Automation::OperationIds::speaker_mix::clip::apply,
                  QStringLiteral("apply-normalized-preset"), [&] {
                      const auto base = runtime.documentVersion();
                      const auto changed = runtime.parameters().applyClipSpeakerMix(
                          commandContext(runtime), clipId, singerA, speakerA, fixed);
                      const auto snapshot = clipSnapshot(runtime, clipId);
                      suite.expect(changed && changed.get().current.revision == base.revision + 1 &&
                                       snapshot &&
                                       snapshot->data.ownSpeakerMixData.mode ==
                                           SpeakerMixModel::SingerSourceMode::FixedMix &&
                                       snapshot->data.ownSpeakerMixData.fixedWeights ==
                                           QVector<double>({1.0}),
                                   QStringLiteral("clip preset apply must normalize mix"));
                  });

        suite.run(Automation::OperationIds::speaker_mix::clip::enable_dynamic,
                  QStringLiteral("dynamic-keyframes"), [&] {
                      const auto base = runtime.documentVersion();
                      const auto changed = runtime.parameters().enableClipDynamicSpeakerMix(
                          commandContext(runtime), clipId, singerA, speakerA, dynamic);
                      const auto snapshot = clipSnapshot(runtime, clipId);
                      suite.expect(
                          changed && changed.get().current.revision == base.revision + 1 &&
                              snapshot &&
                              snapshot->data.ownSpeakerMixData.mode ==
                                  SpeakerMixModel::SingerSourceMode::DynamicMix &&
                              snapshot->data.ownSpeakerMixData.dynamicKeyframes.first().tick == 0,
                          QStringLiteral("dynamic clip mix must sort keyframes"));
                  });

        suite.run(Automation::OperationIds::speaker_mix::clip::replace,
                  QStringLiteral("preserve-owned-voice"), [&] {
                      const auto before = clipSnapshot(runtime, clipId);
                      const auto base = runtime.documentVersion();
                      const auto changed = runtime.parameters().replaceClipSpeakerMix(
                          commandContext(runtime), clipId, fixed);
                      const auto after = clipSnapshot(runtime, clipId);
                      suite.expect(
                          changed && changed.get().current.revision == base.revision + 1 &&
                              before && after &&
                              after->data.ownSingerInfo == before->data.ownSingerInfo &&
                              after->data.ownSpeakerInfo == before->data.ownSpeakerInfo &&
                              after->data.ownSpeakerMixData.mode ==
                                  SpeakerMixModel::SingerSourceMode::FixedMix,
                          QStringLiteral("clip mix replacement must preserve owned voice"));
                  });

        suite.run(Automation::OperationIds::clips::use_track_voice,
                  QStringLiteral("inherit-noop"), [&] {
                      const auto base = runtime.documentVersion();
                      const auto changed = runtime.parameters().useTrackVoiceContext(
                          commandContext(runtime), clipId);
                      const auto snapshot = clipSnapshot(runtime, clipId);
                      suite.expect(changed && changed.get().current.revision == base.revision + 1 &&
                                       snapshot && snapshot->data.usesTrackVoiceContext,
                                   QStringLiteral("clip must return to track inheritance"));
                      const auto noOp = runtime.parameters().useTrackVoiceContext(
                          commandContext(runtime), clipId);
                      suite.expect(noOp && !noOp.get().changed,
                                   QStringLiteral("already inherited context must be a no-op"));
                  });
    }

    bool hasTempo(const Automation::TimelineSnapshotDto &timeline, const int tick,
                  const double value) {
        return std::any_of(timeline.tempos.cbegin(), timeline.tempos.cend(),
                           [tick, value](const Tempo &tempo) {
                               return tempo.pos == tick && tempo.value == value;
                           });
    }

    bool hasSignature(const Automation::TimelineSnapshotDto &timeline, const int bar,
                      const int numerator, const int denominator) {
        return std::any_of(timeline.timeSignatures.cbegin(), timeline.timeSignatures.cend(),
                           [bar, numerator, denominator](const TimeSignature &signature) {
                               return signature.barIndex == bar &&
                                      signature.numerator == numerator &&
                                      signature.denominator == denominator;
                           });
    }

    void testTimelineAndHistoryDomain(Suite &suite) {
        TestRuntime testRuntime;
        auto &runtime = testRuntime.runtime();

        suite.run(
            Automation::OperationIds::timeline::get, QStringLiteral("anchors-and-version"), [&] {
                const auto timeline =
                    runtime.timeline().getTimeline(runtime.documentVersion().documentId);
                suite.expect(timeline && timeline.get().document == runtime.documentVersion() &&
                                 !timeline.get().tempos.isEmpty() &&
                                 timeline.get().tempos.first().pos == 0 &&
                                 !timeline.get().timeSignatures.isEmpty() &&
                                 timeline.get().timeSignatures.first().barIndex == 0,
                             QStringLiteral("timeline query must expose both immutable anchors"));
                const auto wrong = runtime.timeline().getTimeline(Automation::DocumentId::create());
                suite.expect(!wrong &&
                                 wrong.getError().code == AutomationErrorCode::DocumentChanged,
                             QStringLiteral("timeline query must reject old document ID"));
            });

        suite.run(
            Automation::OperationIds::tempos::set,
            QStringLiteral("invalid-preview-sorted-replace-noop"), [&] {
                const auto base = runtime.documentVersion();
                const auto badTick =
                    runtime.timeline().setTempo(commandContext(runtime), -1, 120.0);
                const auto badValue = runtime.timeline().setTempo(
                    commandContext(runtime), 960, std::numeric_limits<double>::quiet_NaN());
                suite.expect(isError(badTick, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("tempo")) &&
                                 isError(badValue, AutomationErrorCode::InvalidArgument,
                                         QStringLiteral("tempo")) &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("invalid tempo inputs must not mutate"));
                const auto preview =
                    runtime.timeline().setTempo(commandContext(runtime, true), 1920, 150.0);
                suite.expect(preview && preview.get().changed && preview.get().validatedOnly &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("tempo preview must be side-effect free"));
                const auto later =
                    runtime.timeline().setTempo(commandContext(runtime), 1920, 150.0);
                const auto earlier =
                    runtime.timeline().setTempo(commandContext(runtime), 960, 140.0);
                auto timeline =
                    runtime.timeline().getTimeline(runtime.documentVersion().documentId);
                suite.expect(later && earlier && timeline && timeline.get().tempos.size() == 3 &&
                                 timeline.get().tempos.at(1).pos == 960 &&
                                 timeline.get().tempos.at(2).pos == 1920,
                             QStringLiteral("tempo insertion must remain sorted"));
                const auto replace =
                    runtime.timeline().setTempo(commandContext(runtime), 960, 145.0);
                const auto replaced =
                    runtime.timeline().getTimeline(runtime.documentVersion().documentId);
                suite.expect(replace && replaced && hasTempo(replaced.get(), 960, 145.0),
                             QStringLiteral("same tick must replace tempo"));
                const auto noOp = runtime.timeline().setTempo(commandContext(runtime), 960, 145.0);
                suite.expect(noOp && !noOp.get().changed,
                             QStringLiteral("identical tempo must be a no-op"));
            });

        suite.run(
            Automation::OperationIds::tempos::remove,
            QStringLiteral("anchor-missing-preview-undo"), [&] {
                testRuntime.history()->reset();
                const auto anchor = runtime.timeline().deleteTempo(commandContext(runtime), 0);
                suite.expect(
                    isError(anchor, AutomationErrorCode::InvalidArgument, QStringLiteral("tick")),
                    QStringLiteral("tick-zero tempo anchor cannot be deleted"));
                const auto missing = runtime.timeline().deleteTempo(commandContext(runtime), 7777);
                suite.expect(missing && !missing.get().changed,
                             QStringLiteral("missing tempo deletion must be a no-op"));
                const auto base = runtime.documentVersion();
                const auto preview =
                    runtime.timeline().deleteTempo(commandContext(runtime, true), 960);
                suite.expect(preview && preview.get().changed && runtime.documentVersion() == base,
                             QStringLiteral("tempo delete preview must preserve point"));
                const auto removed = runtime.timeline().deleteTempo(commandContext(runtime), 960);
                const auto timeline =
                    runtime.timeline().getTimeline(runtime.documentVersion().documentId);
                suite.expect(removed && timeline && !hasTempo(timeline.get(), 960, 145.0),
                             QStringLiteral("existing non-anchor tempo must be removed"));
                const auto undo = runtime.history().undo(commandContext(runtime));
                const auto restored =
                    runtime.timeline().getTimeline(runtime.documentVersion().documentId);
                suite.expect(undo && restored && hasTempo(restored.get(), 960, 145.0),
                             QStringLiteral("tempo deletion must undo once"));
            });

        suite.run(
            Automation::OperationIds::tempos::set,
            QStringLiteral("legacy-audio-tempo-undo-restores-raw-range"), [&] {
                const auto trackId = insertedTrack(runtime, QStringLiteral("Audio Tempo"));
                const auto insert = runtime.project().insertClips(
                    commandContext(runtime),
                    {
                        {.trackId = trackId,
                         .clip = audioClipDraft(QStringLiteral("Legacy Tempo Audio"))}
                });
                const auto clipId = insert && !insert.get().affectedObjects.isEmpty()
                                        ? ClipId(insert.get().affectedObjects.first().value)
                                        : ClipId{};
                testRuntime.history()->reset();
                const auto before = clipSnapshot(runtime, clipId);
                const auto changed = runtime.timeline().setTempo(commandContext(runtime), 0, 90.0);
                const auto changedSnapshot = clipSnapshot(runtime, clipId);
                const auto undo = runtime.history().undo(commandContext(runtime));
                const auto restored = clipSnapshot(runtime, clipId);
                const auto redo = runtime.history().redo(commandContext(runtime));
                const auto redone = clipSnapshot(runtime, clipId);
                const auto undoAgain = runtime.history().undo(commandContext(runtime));
                const auto restoredAgain = clipSnapshot(runtime, clipId);
                suite.expect(
                    trackId.isValid() && insert && before && changed && changedSnapshot &&
                        changedSnapshot->data.properties.length >=
                            changedSnapshot->data.properties.clipStart +
                                changedSnapshot->data.properties.clipLen &&
                        undo && restored &&
                        sameClipTiming(restored->data.properties, before->data.properties) &&
                        redo && redone && undoAgain && restoredAgain &&
                        sameClipTiming(restoredAgain->data.properties, before->data.properties),
                    QStringLiteral(
                        "tempo undo must restore raw legacy audio ticks across redo cycles"));
            });

        suite.run(
            Automation::OperationIds::time_signatures::set,
            QStringLiteral("invalid-preview-sorted-replace-noop"), [&] {
                const auto base = runtime.documentVersion();
                const auto invalidBar =
                    runtime.timeline().setTimeSignature(commandContext(runtime), -1, 4, 4);
                const auto invalidNumerator =
                    runtime.timeline().setTimeSignature(commandContext(runtime), 2, 0, 4);
                const auto invalidDenominator =
                    runtime.timeline().setTimeSignature(commandContext(runtime), 2, 3, 3);
                suite.expect(isError(invalidBar, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("time_signature")) &&
                                 isError(invalidNumerator, AutomationErrorCode::InvalidArgument,
                                         QStringLiteral("time_signature")) &&
                                 isError(invalidDenominator, AutomationErrorCode::InvalidArgument,
                                         QStringLiteral("time_signature")) &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("invalid signatures must fail atomically"));
                const auto preview =
                    runtime.timeline().setTimeSignature(commandContext(runtime, true), 8, 6, 8);
                suite.expect(preview && preview.get().changed && runtime.documentVersion() == base,
                             QStringLiteral("signature preview must be side-effect free"));
                const auto later =
                    runtime.timeline().setTimeSignature(commandContext(runtime), 8, 6, 8);
                const auto earlier =
                    runtime.timeline().setTimeSignature(commandContext(runtime), 4, 3, 4);
                const auto timeline =
                    runtime.timeline().getTimeline(runtime.documentVersion().documentId);
                suite.expect(later && earlier && timeline &&
                                 timeline.get().timeSignatures.size() == 3 &&
                                 timeline.get().timeSignatures.at(1).barIndex == 4 &&
                                 timeline.get().timeSignatures.at(2).barIndex == 8,
                             QStringLiteral("signature insertion must remain sorted"));
                const auto replace =
                    runtime.timeline().setTimeSignature(commandContext(runtime), 4, 5, 4);
                const auto replaced =
                    runtime.timeline().getTimeline(runtime.documentVersion().documentId);
                suite.expect(replace && replaced && hasSignature(replaced.get(), 4, 5, 4),
                             QStringLiteral("same bar must replace signature"));
                const auto noOp =
                    runtime.timeline().setTimeSignature(commandContext(runtime), 4, 5, 4);
                suite.expect(noOp && !noOp.get().changed,
                             QStringLiteral("identical signature must be a no-op"));
            });

        suite.run(Automation::OperationIds::time_signatures::remove,
                  QStringLiteral("anchor-missing-preview-undo"), [&] {
                      testRuntime.history()->reset();
                      const auto anchor =
                          runtime.timeline().deleteTimeSignature(commandContext(runtime), 0);
                      suite.expect(isError(anchor, AutomationErrorCode::InvalidArgument,
                                           QStringLiteral("bar_index")),
                                   QStringLiteral("bar-zero signature anchor cannot be deleted"));
                      const auto missing =
                          runtime.timeline().deleteTimeSignature(commandContext(runtime), 777);
                      suite.expect(missing && !missing.get().changed,
                                   QStringLiteral("missing signature deletion must be a no-op"));
                      const auto base = runtime.documentVersion();
                      const auto preview =
                          runtime.timeline().deleteTimeSignature(commandContext(runtime, true), 4);
                      suite.expect(preview && preview.get().changed &&
                                       runtime.documentVersion() == base,
                                   QStringLiteral("signature delete preview must preserve point"));
                      const auto removed =
                          runtime.timeline().deleteTimeSignature(commandContext(runtime), 4);
                      const auto timeline =
                          runtime.timeline().getTimeline(runtime.documentVersion().documentId);
                      suite.expect(removed && timeline && !hasSignature(timeline.get(), 4, 5, 4),
                                   QStringLiteral("existing non-anchor signature must be removed"));
                      const auto undo = runtime.history().undo(commandContext(runtime));
                      const auto restored =
                          runtime.timeline().getTimeline(runtime.documentVersion().documentId);
                      suite.expect(undo && restored && hasSignature(restored.get(), 4, 5, 4),
                                   QStringLiteral("signature deletion must undo once"));
                  });

        suite.run(
            Automation::OperationIds::master::set_control,
            QStringLiteral("invalid-preview-noop-undo"), [&] {
                testRuntime.history()->reset();
                TrackControl invalid;
                invalid.setGain(std::numeric_limits<double>::infinity());
                const auto rejected =
                    runtime.timeline().setMasterControl(commandContext(runtime), invalid);
                suite.expect(isError(rejected, AutomationErrorCode::InvalidArgument,
                                     QStringLiteral("control")),
                             QStringLiteral("non-finite master control must be rejected"));
                TrackControl control;
                control.setGain(0.6);
                control.setPan(-0.4);
                control.setMute(true);
                const auto base = runtime.documentVersion();
                const auto preview =
                    runtime.timeline().setMasterControl(commandContext(runtime, true), control);
                suite.expect(preview && preview.get().changed && runtime.documentVersion() == base,
                             QStringLiteral("master preview must not mutate"));
                const auto changed =
                    runtime.timeline().setMasterControl(commandContext(runtime), control);
                suite.expect(changed && changed.get().current.revision == base.revision + 1,
                             QStringLiteral("master control must advance one revision"));
                const auto noOp =
                    runtime.timeline().setMasterControl(commandContext(runtime), control);
                suite.expect(noOp && !noOp.get().changed,
                             QStringLiteral("identical master control must be a no-op"));
                const auto undo = runtime.history().undo(commandContext(runtime));
                const auto redo = runtime.history().redo(commandContext(runtime));
                suite.expect(undo && undo.get().changed && redo && redo.get().changed,
                             QStringLiteral("master control must support undo and redo"));
            });

        suite.run(Automation::OperationIds::history::get_state,
                  QStringLiteral("empty-and-named-state"), [&] {
                      testRuntime.history()->reset();
                      const auto empty =
                          runtime.history().getState(runtime.documentVersion().documentId);
                      suite.expect(empty && !empty.get().canUndo && !empty.get().canRedo &&
                                       empty.get().onSavePoint,
                                   QStringLiteral("reset History must be empty at savepoint"));
                      const auto tempo =
                          runtime.timeline().setTempo(commandContext(runtime), 2880, 130.0);
                      const auto state =
                          runtime.history().getState(runtime.documentVersion().documentId);
                      suite.expect(tempo && state && state.get().canUndo && !state.get().canRedo &&
                                       !state.get().onSavePoint && !state.get().undoName.isEmpty(),
                                   QStringLiteral("committed edit must expose named undo state"));
                  });

        suite.run(Automation::OperationIds::history::undo, QStringLiteral("preview-commit-empty"),
                  [&] {
                      const auto before = runtime.documentVersion();
                      const auto preview = runtime.history().undo(commandContext(runtime, true));
                      suite.expect(preview && preview.get().validatedOnly &&
                                       preview.get().changed && runtime.documentVersion() == before,
                                   QStringLiteral("undo preview must not consume History"));
                      const auto undo = runtime.history().undo(commandContext(runtime));
                      suite.expect(undo && undo.get().changed &&
                                       undo.get().current.revision == before.revision + 1,
                                   QStringLiteral("undo must advance one revision"));
                      const auto empty = runtime.history().undo(commandContext(runtime));
                      suite.expect(empty && !empty.get().changed,
                                   QStringLiteral("empty undo must be a successful no-op"));
                  });

        suite.run(Automation::OperationIds::history::redo,
                  QStringLiteral("preview-commit-branch-clear"), [&] {
                      const auto before = runtime.documentVersion();
                      const auto preview = runtime.history().redo(commandContext(runtime, true));
                      suite.expect(preview && preview.get().validatedOnly &&
                                       preview.get().changed && runtime.documentVersion() == before,
                                   QStringLiteral("redo preview must not consume History"));
                      const auto redo = runtime.history().redo(commandContext(runtime));
                      suite.expect(redo && redo.get().changed &&
                                       redo.get().current.revision == before.revision + 1,
                                   QStringLiteral("redo must advance one revision"));
                      const auto undo = runtime.history().undo(commandContext(runtime));
                      const auto branch =
                          runtime.timeline().setTempo(commandContext(runtime), 3360, 125.0);
                      const auto state =
                          runtime.history().getState(runtime.documentVersion().documentId);
                      suite.expect(undo && branch && state && !state.get().canRedo,
                                   QStringLiteral("new edit after undo must clear redo branch"));
                  });
    }

    using MutationCall = std::function<AutomationResult<MutationResult>(const CommandContext &)>;

    void testErrorPriorityMatrix(Suite &suite) {
        TestRuntime testRuntime;
        auto &runtime = testRuntime.runtime();
        const auto trackId = insertedTrack(runtime, QStringLiteral("Priority"));
        const auto clipId = insertedSingingClip(runtime, trackId, QStringLiteral("Priority Clip"));
        const auto noteIds = insertedNotes(
            runtime, clipId,
            {noteDraft(0, 480, 60, QStringLiteral("la"), QStringLiteral("priority-note"))});
        const auto noteId = noteIds.isEmpty() ? NoteId() : noteIds.first();
        const auto draft = trackDraft(QStringLiteral("ignored"));
        const SingerInfo emptySinger;
        const SpeakerInfo emptySpeaker;
        const SpeakerMixModel::SpeakerMixData emptyMix;

        const QList<QPair<OperationId, MutationCall>> commands = {
            {Automation::OperationIds::tracks::insert,
             [&](const auto &context) {
                 return runtime.project().insertTrack(context, -1, draft);
             }                                                                                    },
            {Automation::OperationIds::tracks::move,
             [&](const auto &context) {
                 return runtime.project().moveTrack(context, TrackId(999999), -1);
             }                                                                                    },
            {Automation::OperationIds::tracks::remove,
             [&](const auto &context) {
                 return runtime.project().removeTracks(context, {TrackId(999999)});
             }                                                                                    },
            {Automation::OperationIds::tracks::set_color,
             [&](const auto &context) {
                 return runtime.project().setTrackColor(context, TrackId(999999), -1);
             }                                                                                    },
            {Automation::OperationIds::tracks::set_default_language,
             [&](const auto &context) {
                 return runtime.project().setTrackDefaultLanguage(context, TrackId(999999), {});
             }                                                                                    },
            {Automation::OperationIds::tracks::set_properties,
             [&](const auto &context) {
                 return runtime.project().setTrackProperties(
                     context, {.id = TrackId(999999), .gain = std::nan(""), .pan = 0.0});
             }                                                                                    },
            {Automation::OperationIds::clips::insert,
             [&](const auto &context) {
                 return runtime.project().insertClips(
                     context,
                     {
                         {                                                                      .trackId = TrackId(999999),                                                                                   .clip = singingClipDraft(QString())}});
             }                                                                                                  },
            {Automation::OperationIds::clips::remove,
             [&](const auto &context) {
                 return runtime.project().removeClips(context, {ClipId(999999)});
             }},
            {Automation::OperationIds::clips::set_default_language,
             [&](const auto &context) {
                 return runtime.project().setSingingClipDefaultLanguage(context, ClipId(999999),
                                                                        {});
             }                                                                                                               },
            {Automation::OperationIds::clips::set_properties,
             [&](const auto &context) {
                 return runtime.project().setClipProperties(
                     context, {.id = ClipId(999999), .gain = std::nan("")});
             }},
            {Automation::OperationIds::notes::insert,
             [&](const auto &context) {
                 return runtime.notes().insertNotes(context, ClipId(999999), {});
             }                                                                                                               },
            {Automation::OperationIds::notes::move,
             [&](const auto &context) {
                 return runtime.notes().moveNotes(context, ClipId(999999), {}, 0, 0);
             }},
            {Automation::OperationIds::notes::quantize,
             [&](const auto &context) {
                 return runtime.notes().quantizeNotes(context, ClipId(999999), {}, 0, true, true);
             }                                                                                                               },
            {Automation::OperationIds::notes::remove,
             [&](const auto &context) {
                 return runtime.notes().removeNotes(context, ClipId(999999), {});
             }},
            {Automation::OperationIds::notes::reset_phoneme_offsets,
             [&](const auto &context) {
                 return runtime.notes().resetPhonemeOffsets(context, ClipId(999999), {});
             }                                                                                                               },
            {Automation::OperationIds::notes::resize_left,
             [&](const auto &context) {
                 return runtime.notes().resizeNotesLeft(context, ClipId(999999), {}, 0, 0);
             }                                                                                                               },
            {Automation::OperationIds::notes::resize_right,
             [&](const auto &context) {
                 return runtime.notes().resizeNotesRight(context, ClipId(999999), {}, 0, 0);
             }},
            {Automation::OperationIds::notes::set_phoneme_offsets,
             [&](const auto &context) {
                 return runtime.notes().setPhonemeOffsets(context, ClipId(999999), NoteId(999999),
                                                          {});
             }                                                                                                               },
            {Automation::OperationIds::notes::set_word_properties,
             [&](const auto &context) {
                 return runtime.notes().setWordProperties(context, ClipId(999999),
                                                          {{.noteId = NoteId(999999)}});
             }},
            {Automation::OperationIds::notes::split,
             [&](const auto &context) {
                 return runtime.notes().splitNote(context, ClipId(999999), NoteId(999999), {}, 0);
             }                                                                                                               },
            {Automation::OperationIds::parameters::replace,
             [&](const auto &context) {
                 return runtime.parameters().replaceParameter(
                     context, ClipId(999999), ParamInfo::Unknown, Param::Unknown, {});
             }},
            {Automation::OperationIds::speaker_mix::clip::apply,
             [&](const auto &context) {
                 return runtime.parameters().applyClipSpeakerMix(
                     context, ClipId(999999), emptySinger, emptySpeaker, emptyMix);
             }                                                                                                               },
            {Automation::OperationIds::speaker_mix::clip::enable_dynamic,
             [&](const auto &context) {
                 return runtime.parameters().enableClipDynamicSpeakerMix(
                     context, ClipId(999999), emptySinger, emptySpeaker, emptyMix);
             }},
            {Automation::OperationIds::speaker_mix::clip::replace,
             [&](const auto &context) {
                 return runtime.parameters().replaceClipSpeakerMix(context, ClipId(999999),
                                                                   emptyMix);
             }                                                                                                               },
            {Automation::OperationIds::clips::set_voice,
             [&](const auto &context) {
                 return runtime.parameters().selectClipSingleSpeaker(context, ClipId(999999),
                                                                     emptySinger, emptySpeaker);
             }},
            {Automation::OperationIds::clips::use_track_voice,
             [&](const auto &context) {
                 return runtime.parameters().useTrackVoiceContext(context, ClipId(999999));
             }                                                                                                               },
            {Automation::OperationIds::speaker_mix::track::apply,
             [&](const auto &context) {
                 return runtime.parameters().applyTrackSpeakerMix(
                     context, TrackId(999999), emptySinger, emptySpeaker, emptyMix);
             }},
            {Automation::OperationIds::speaker_mix::track::replace,
             [&](const auto &context) {
                 return runtime.parameters().replaceTrackSpeakerMix(context, TrackId(999999),
                                                                    emptyMix);
             }                                                                                                               },
            {Automation::OperationIds::tracks::set_voice,
             [&](const auto &context) {
                 return runtime.parameters().selectTrackSingleSpeaker(context, TrackId(999999),
                                                                      emptySinger, emptySpeaker);
             }},
            {Automation::OperationIds::tempos::set,
             [&](const auto &context) { return runtime.timeline().setTempo(context, -1, -1.0); }                                                                                                               },
            {Automation::OperationIds::tempos::remove,
             [&](const auto &context) { return runtime.timeline().deleteTempo(context, 0); }},
            {Automation::OperationIds::time_signatures::set,
             [&](const auto &context) {
                 return runtime.timeline().setTimeSignature(context, -1, 0, 3);
             }                                                                                                                   },
            {Automation::OperationIds::time_signatures::remove,
             [&](const auto &context) {
                 return runtime.timeline().deleteTimeSignature(context, 0);
             }},
            {Automation::OperationIds::master::set_control,
             [&](const auto &context) {
                 TrackControl control;
                 control.setGain(std::nan(""));
                 return runtime.timeline().setMasterControl(context, control);
             }                                                                                                                   },
            {Automation::OperationIds::history::undo,
             [&](const auto &context) { return runtime.history().undo(context); }},
            {Automation::OperationIds::history::redo,
             [&](const auto &context) { return runtime.history().redo(context); }                                                },
        };

        suite.run(QStringLiteral("dispatch"), QStringLiteral("error-priority-matrix"), [&] {
            suite.expect(trackId.isValid() && clipId.isValid() && noteId.isValid(),
                         QStringLiteral("priority fixture must be valid"));
            for (const auto &[operationId, command] : commands) {
                const auto wrongDocument = command(wrongDocumentContext(runtime));
                suite.expect(!wrongDocument &&
                                 wrongDocument.getError().code ==
                                     AutomationErrorCode::DocumentChanged &&
                                 wrongDocument.getError().operationId == operationId,
                             QStringLiteral("%1 must prefer document ID error").arg(operationId));
                const auto stale = command(staleContext(runtime));
                suite.expect(!stale &&
                                 stale.getError().code == AutomationErrorCode::RevisionConflict &&
                                 stale.getError().operationId == operationId,
                             QStringLiteral("%1 must prefer revision error").arg(operationId));
            }
        });
    }

    int quantizeProbe(int argc, char *argv[]) {
        QCoreApplication application(argc, argv);
        TestRuntime testRuntime;
        auto &runtime = testRuntime.runtime();
        const auto trackId = insertedTrack(runtime, QStringLiteral("Quantize"));
        const auto clipId = insertedSingingClip(runtime, trackId, QStringLiteral("Quantize Clip"));
        const auto ids = insertedNotes(
            runtime, clipId,
            {noteDraft(73, 407, 60, QStringLiteral("la"), QStringLiteral("quantize-note"))});
        if (ids.size() != 1)
            return 2;
        const auto before = runtime.documentVersion();
        const auto result =
            runtime.notes().quantizeNotes(commandContext(runtime), clipId, ids, 16, true, true);
        if (!result || !result.get().changed ||
            result.get().current.revision != before.revision + 1)
            return 3;
        const auto note = noteSnapshot(runtime, clipId, ids.first());
        if (!note || note->data.localStart != 120 || note->data.length != 360)
            return 4;
        const auto noOp =
            runtime.notes().quantizeNotes(commandContext(runtime), clipId, ids, 16, true, true);
        return noOp && !noOp.get().changed ? 0 : 5;
    }

    void testQuantizeInChild(Suite &suite) {
        suite.run(Automation::OperationIds::notes::quantize,
                  QStringLiteral("commit-noop-process-isolation"), [&] {
                      QProcess probe;
                      probe.start(QCoreApplication::applicationFilePath(),
                                  {QStringLiteral("--quantize-probe")});
                      const auto started = probe.waitForStarted(5000);
                      const auto finished = started && probe.waitForFinished(3000);
                      if (!finished) {
                          probe.kill();
                          probe.waitForFinished(1000);
                      }
                      suite.expect(
                          started && finished && probe.exitStatus() == QProcess::NormalExit &&
                              probe.exitCode() == 0,
                          QStringLiteral("quantize must commit safely, snap geometry, and no-op "
                                         "when already aligned; exit=%1 stderr=%2")
                              .arg(probe.exitCode())
                              .arg(QString::fromUtf8(probe.readAllStandardError())));
                  });
    }

    QList<OperationId> coveredOperations() {
        return {
            Automation::OperationIds::project::get,
            Automation::OperationIds::tracks::insert,
            Automation::OperationIds::tracks::move,
            Automation::OperationIds::tracks::remove,
            Automation::OperationIds::tracks::set_color,
            Automation::OperationIds::tracks::set_default_language,
            Automation::OperationIds::tracks::set_properties,
            Automation::OperationIds::clips::insert,
            Automation::OperationIds::clips::remove,
            Automation::OperationIds::clips::set_default_language,
            Automation::OperationIds::clips::set_properties,
            Automation::OperationIds::notes::list,
            Automation::OperationIds::notes::insert,
            Automation::OperationIds::notes::move,
            Automation::OperationIds::notes::quantize,
            Automation::OperationIds::notes::remove,
            Automation::OperationIds::notes::reset_phoneme_offsets,
            Automation::OperationIds::notes::resize_left,
            Automation::OperationIds::notes::resize_right,
            Automation::OperationIds::notes::set_phoneme_offsets,
            Automation::OperationIds::notes::set_word_properties,
            Automation::OperationIds::notes::split,
            Automation::OperationIds::parameters::get,
            Automation::OperationIds::parameters::replace,
            Automation::OperationIds::speaker_mix::clip::apply,
            Automation::OperationIds::speaker_mix::clip::enable_dynamic,
            Automation::OperationIds::speaker_mix::clip::replace,
            Automation::OperationIds::clips::set_voice,
            Automation::OperationIds::clips::use_track_voice,
            Automation::OperationIds::speaker_mix::track::apply,
            Automation::OperationIds::speaker_mix::track::replace,
            Automation::OperationIds::tracks::set_voice,
            Automation::OperationIds::timeline::get,
            Automation::OperationIds::tempos::set,
            Automation::OperationIds::tempos::remove,
            Automation::OperationIds::time_signatures::set,
            Automation::OperationIds::time_signatures::remove,
            Automation::OperationIds::master::set_control,
            Automation::OperationIds::history::get_state,
            Automation::OperationIds::history::undo,
            Automation::OperationIds::history::redo,
        };
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1 && QString::fromLocal8Bit(argv[1]) == QStringLiteral("--quantize-probe"))
        return quantizeProbe(argc, argv);

    QCoreApplication application(argc, argv);
    Suite suite;
    testProjectDomain(suite);
    testNoteDomain(suite);
    testParameterAndSpeakerDomain(suite);
    testTimelineAndHistoryDomain(suite);
    testErrorPriorityMatrix(suite);
    testQuantizeInChild(suite);
    suite.requireOperations(coveredOperations());
    return suite.result();
}
