#include "tst_project_editing.h"

#include "Automation/OperationIds.h"
#include "Automation/ProjectAutomationDtos.h"
#include "TestRuntime.h"
#include "../TestSupport/TestAssertions.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/SpeakerMixData.h>
#include <lite/ProjectModel/AppModel/ParamProperties.h>
#include <lite/ProjectModel/Voice/SingerInfo.h>
#include <lite/ProjectModel/Voice/SpeakerInfo.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>

#include <QCoreApplication>
#include <QtTest>
#include <QProcess>
#include <QTextStream>
#include <QVersionNumber>
#include <QVector>

#include <algorithm>
#include <cmath>
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

    CommandContext commandContext(const CoreRuntime &runtime, const bool validateOnly = false) {
        return {
            .expected = runtime.documentVersion(),
            .validateOnly = validateOnly,
            .source = Automation::InvocationSource::Test,
        };
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
        if (!noOp || noOp.get().changed)
            return 5;

        const auto extremeLengthIds = insertedNotes(
            runtime, clipId,
            {noteDraft(0, std::numeric_limits<int>::max(), 61, QStringLiteral("length"),
                       QStringLiteral("quantize-extreme-length"))});
        if (extremeLengthIds.size() != 1)
            return 6;
        const auto beforeLengthFailure = runtime.documentVersion();
        const auto lengthFailure = runtime.notes().quantizeNotes(commandContext(runtime), clipId,
                                                                 extremeLengthIds, 30, false, true);
        if (!isError(lengthFailure, AutomationErrorCode::InvalidArgument,
                     QStringLiteral("note_ids")) ||
            runtime.documentVersion() != beforeLengthFailure)
            return 7;

        const auto lateClipId =
            insertedSingingClip(runtime, trackId, QStringLiteral("Late Quantize Clip"), 100);
        const auto lateIds = insertedNotes(
            runtime, lateClipId,
            {noteDraft(std::numeric_limits<int>::max() - 50, 1, 62, QStringLiteral("start"),
                       QStringLiteral("quantize-extreme-start"))});
        if (lateIds.size() != 1)
            return 8;
        const auto beforeStartFailure = runtime.documentVersion();
        const auto startFailure = runtime.notes().quantizeNotes(commandContext(runtime), lateClipId,
                                                                lateIds, 16, true, false);
        return isError(startFailure, AutomationErrorCode::InvalidArgument,
                       QStringLiteral("note_ids")) &&
                       runtime.documentVersion() == beforeStartFailure
                   ? 0
                   : 9;
    }

}

void ProjectEditingTests::duplicateClipsPreserveContentAndCreateIndependentObjects_data() {
    QTest::addColumn<bool>("useTargetTrack");
    QTest::newRow("keep-source-tracks") << false;
    QTest::newRow("combine-into-target-track") << true;
}

void ProjectEditingTests::duplicateClipsPreserveContentAndCreateIndependentObjects() {
    QFETCH(bool, useTargetTrack);
    NoteFixture fixture;
    auto &runtime = fixture.testRuntime.runtime();
    const auto audioTrack = insertedTrack(runtime, QStringLiteral("Backing"));
    const auto target = insertedTrack(runtime, QStringLiteral("Copies"));
    QVERIFY(audioTrack.isValid() && target.isValid());
    QVERIFY(runtime.project().moveClips(commandContext(runtime),
                                        {
                                            {fixture.clipId, fixture.trackId, 480}
    }));
    const auto first = speaker(QStringLiteral("first"));
    const auto second = speaker(QStringLiteral("second"));
    const auto voice = singer(QStringLiteral("blended"), {first, second});
    QVERIFY(runtime.parameters().enableClipDynamicSpeakerMix(
        commandContext(runtime), fixture.clipId, voice, first, dynamicMix(first, second)));
    QVERIFY(runtime.parameters().createAnchorCurve(
        commandContext(runtime), fixture.clipId, ParamInfo::Pitch, Param::Edited,
        QStringLiteral("pitch"),
        {
            {0,   6000, AnchorNode::Linear },
            {240, 6200, AnchorNode::Hermite}
    }));
    auto audio = audioClipDraft(QStringLiteral("Backing sample"));
    audio.properties.start = 960;
    audio.properties.trimStartMs = 50;
    audio.properties.playLengthMs = 750;
    audio.properties.materialLengthMs = 1000;
    audio.hasRealTimeAnchor = true;
    const auto inserted =
        runtime.project().insertClips(commandContext(runtime), {
                                                                   {audioTrack, audio}
    });
    QVERIFY(inserted);
    const ClipId audioId(inserted.get().affectedObjects.first().value);
    const auto sourceVoice = clipSnapshot(runtime, fixture.clipId);
    const auto sourceAudio = clipSnapshot(runtime, audioId);
    QVERIFY(sourceVoice && sourceAudio);
    const auto oldNotes =
        runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.clipId);
    const auto oldPitch = runtime.parameters().getParameter(
        runtime.documentVersion().documentId, fixture.clipId, ParamInfo::Pitch, Param::Edited);
    QVERIFY(oldNotes && oldPitch);
    fixture.testRuntime.history()->reset();
    const auto before = runtime.documentVersion();
    const auto beforeModel = fixture.testRuntime.model().serialize();
    Automation::ClipDuplicateDestinationDto destination;
    destination.targetStart = 3840;
    if (useTargetTrack)
        destination.targetTrackId = target;
    const QList<ClipId> sources{audioId, fixture.clipId};
    const auto preview =
        runtime.project().duplicateClips(commandContext(runtime, true), sources, destination);
    QVERIFY(preview && preview.get().validatedOnly && preview.get().changed);
    QVERIFY(preview.get().createdObjects.isEmpty());
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(fixture.testRuntime.model().serialize(), beforeModel);

    auto command = commandContext(runtime);
    command.idempotencyKey = QStringLiteral("copy-selected-clips");
    const auto duplicated = runtime.project().duplicateClips(command, sources, destination);
    QVERIFY2(duplicated, qPrintable(duplicated ? QString() : duplicated.getError().message));
    QList<ClipId> copies;
    for (const auto &created : duplicated.get().createdObjects) {
        if (created.object.kind == Automation::ObjectKind::Clip)
            copies.append(ClipId(created.object.value));
    }
    QCOMPARE(copies.size(), 2);
    const auto copyAudio = clipSnapshot(runtime, copies.first());
    const auto copyVoice = clipSnapshot(runtime, copies.last());
    QVERIFY(copyAudio && copyVoice);
    QCOMPARE(copyAudio->trackId, useTargetTrack ? target : audioTrack);
    QCOMPARE(copyVoice->trackId, useTargetTrack ? target : fixture.trackId);
    QCOMPARE(copyVoice->data.properties.start, 3840);
    QCOMPARE(copyAudio->data.properties.start - sourceAudio->data.properties.start, 3360);
    QCOMPARE(copyAudio->data.audioPath, sourceAudio->data.audioPath);
    QVERIFY(copyAudio->data.hasRealTimeAnchor);
    QCOMPARE(copyAudio->data.properties.trimStartMs, 50.0);
    QCOMPARE(copyAudio->data.properties.playLengthMs, 750.0);
    QCOMPARE(copyAudio->data.properties.materialLengthMs, 1000.0);
    QCOMPARE(copyVoice->data.ownSingerInfo, voice);
    QVERIFY(!copyVoice->data.usesTrackVoiceContext);
    const auto &oldMix = sourceVoice->data.ownSpeakerMixData;
    const auto &newMix = copyVoice->data.ownSpeakerMixData;
    QCOMPARE(newMix.mode, oldMix.mode);
    QCOMPARE(newMix.dynamicKeyframes.size(), oldMix.dynamicKeyframes.size());
    for (qsizetype i = 0; i < oldMix.dynamicKeyframes.size(); ++i) {
        const auto &oldKey = oldMix.dynamicKeyframes.at(i);
        const auto &newKey = newMix.dynamicKeyframes.at(i);
        QVERIFY(newKey.id != oldKey.id);
        QCOMPARE(newKey.tick, oldKey.tick);
        QCOMPARE(newKey.weights, oldKey.weights);
    }
    const auto copiedNotes =
        runtime.notes().getNotes(runtime.documentVersion().documentId, copies.last());
    QVERIFY(copiedNotes);
    QCOMPARE(copiedNotes.get().size(), oldNotes.get().size());
    for (qsizetype i = 0; i < oldNotes.get().size(); ++i) {
        const auto &oldNote = oldNotes.get().at(i);
        const auto &newNote = copiedNotes.get().at(i);
        QVERIFY(newNote.id != oldNote.id);
        QCOMPARE(newNote.data.localStart, oldNote.data.localStart);
        QCOMPARE(newNote.data.length, oldNote.data.length);
        QCOMPARE(newNote.data.keyIndex, oldNote.data.keyIndex);
        QCOMPARE(newNote.data.lyric, oldNote.data.lyric);
        QCOMPARE(newNote.data.pronunciation.edited, oldNote.data.pronunciation.edited);
        QCOMPARE(newNote.data.phonemes.nameSeq.edited, oldNote.data.phonemes.nameSeq.edited);
        QCOMPARE(newNote.data.phonemes.offsetSeq.original,
                 oldNote.data.phonemes.offsetSeq.original);
    }
    const auto copiedPitch = runtime.parameters().getParameter(
        runtime.documentVersion().documentId, copies.last(), ParamInfo::Pitch, Param::Edited);
    QVERIFY(copiedPitch);
    QCOMPARE(copiedPitch.get().curves.size(), 1);
    const auto &oldCurve = oldPitch.get().curves.first();
    const auto &newCurve = copiedPitch.get().curves.first();
    QVERIFY(newCurve.id != oldCurve.id);
    QCOMPARE(newCurve.nodes.size(), oldCurve.nodes.size());
    for (qsizetype i = 0; i < oldCurve.nodes.size(); ++i) {
        QVERIFY(newCurve.nodes.at(i).id != oldCurve.nodes.at(i).id);
        QCOMPARE(newCurve.nodes.at(i).position, oldCurve.nodes.at(i).position);
        QCOMPARE(newCurve.nodes.at(i).value, oldCurve.nodes.at(i).value);
        QCOMPARE(newCurve.nodes.at(i).interpolation, oldCurve.nodes.at(i).interpolation);
    }
    const auto after = runtime.documentVersion();
    QCOMPARE(after.revision, before.revision + 1);
    const auto afterModel = fixture.testRuntime.model().serialize();
    const auto retried = runtime.project().duplicateClips(command, sources, destination);
    QVERIFY(retried);
    QCOMPARE(retried.get().createdObjects, duplicated.get().createdObjects);
    QCOMPARE(runtime.documentVersion(), after);
    QCOMPARE(fixture.testRuntime.model().serialize(), afterModel);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(fixture.testRuntime.model().serialize(), beforeModel);
    QVERIFY(!fixture.testRuntime.history()->canUndo());
    QVERIFY(runtime.history().redo(commandContext(runtime)));
    QCOMPARE(fixture.testRuntime.model().serialize(), afterModel);
}

void ProjectEditingTests::batchAnchorsCommitAndUndoTogether() {
    TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();
    auto &parameters = runtime.parameters();
    const auto track = insertedTrack(runtime, "Curves");
    const auto clip = insertedSingingClip(runtime, track, "Pitch");
    QVERIFY(parameters.createAnchorCurve(
        commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited, "curve",
        {
            {0,   6000, AnchorNode::Linear},
            {960, 6400, AnchorNode::Linear}
    }));
    const auto snapshot = [&] {
        return parameters
            .getParameter(runtime.documentVersion().documentId, clip, ParamInfo::Pitch,
                          Param::Edited)
            .get()
            .curves.first();
    };
    const auto initial = snapshot();
    const auto curve = initial.id;
    testRuntime.history()->reset();
    const auto before = runtime.documentVersion();
    QVERIFY(parameters.insertAnchors(
        commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited, curve,
        {
            {720, 6300, AnchorNode::Hermite},
            {240, 6100, AnchorNode::Linear }
    }));
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    auto nodes = snapshot().nodes;
    QCOMPARE(nodes.size(), 4);
    QCOMPARE(nodes.at(1).position, 240);
    QCOMPARE(nodes.at(2).position, 720);
    const auto first = nodes.at(1).id;
    const auto second = nodes.at(2).id;
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(snapshot().nodes.size(), 2);
    QVERIFY(runtime.history().redo(commandContext(runtime)));
    QCOMPARE(snapshot().nodes.at(1).id, first);

    const auto inserted = runtime.documentVersion();
    const auto rejected =
        parameters.moveAnchors(commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited,
                               {
                                   {first,  300, 6200},
                                   {second, 300, 6500}
    });
    QVERIFY(!rejected);
    QCOMPARE(runtime.documentVersion(), inserted);
    QCOMPARE(snapshot().nodes.at(1).position, 240);
    QVERIFY(parameters.moveAnchors(commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited,
                                   {
                                       {first,  360, 6200},
                                       {second, 840, 6500}
    }));
    nodes = snapshot().nodes;
    QCOMPARE(nodes.at(1).position, 360);
    QCOMPARE(nodes.at(1).value, 6200);
    QCOMPARE(nodes.at(2).position, 840);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(snapshot().nodes.at(1).position, 240);
    QCOMPARE(snapshot().nodes.at(2).position, 720);

    QVERIFY(parameters.setAnchorInterpolations(commandContext(runtime), clip, ParamInfo::Pitch,
                                               Param::Edited, {first, second}, AnchorNode::Linear));
    QCOMPARE(snapshot().nodes.at(2).interpolation, AnchorNode::Linear);
    QVERIFY(parameters.removeAnchors(commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited,
                                     {first, second}));
    QCOMPARE(snapshot().nodes.size(), 2);
    QCOMPARE(snapshot().nodes.first().value, 6000);
    QCOMPARE(snapshot().nodes.last().value, 6400);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(snapshot().nodes.size(), 4);
}

void ProjectEditingTests::anchorCreationRetriesKeepTheCommittedIdentity() {
    TestRuntime fixture;
    auto &runtime = fixture.runtime();
    auto &parameters = runtime.parameters();
    const auto clip = insertedSingingClip(runtime, insertedTrack(runtime, "Curves"), "Pitch");
    const QList<Automation::AnchorInsertDto> anchors{
        {0,   6000, AnchorNode::Linear },
        {480, 6400, AnchorNode::Hermite}
    };
    fixture.history()->reset();
    auto request = commandContext(runtime, true);
    request.idempotencyKey = QStringLiteral("create-phrase-pitch");
    const auto preview = parameters.createAnchorCurve(request, clip, ParamInfo::Pitch,
                                                      Param::Edited, "phrase", anchors);
    QVERIFY(preview && preview.get().validatedOnly && preview.get().changed);
    QVERIFY(preview.get().createdObjects.isEmpty());
    QCOMPARE(runtime.documentVersion(), request.expected);
    request.validateOnly = false;
    const auto created = parameters.createAnchorCurve(request, clip, ParamInfo::Pitch,
                                                      Param::Edited, "phrase", anchors);
    QVERIFY(created && created.get().changed);
    QCOMPARE(created.get().createdObjects.size(), 1);
    QCOMPARE(created.get().createdObjects.first().clientRef, QStringLiteral("phrase"));
    const auto snapshot = parameters.getParameter(runtime.documentVersion().documentId, clip,
                                                  ParamInfo::Pitch, Param::Edited);
    QVERIFY(snapshot);
    QCOMPARE(snapshot.get().curves.size(), 1);
    const auto identity = snapshot.get().curves.first().id;
    QCOMPARE(created.get().createdObjects.first().object.value, identity.value());
    const auto committed = runtime.documentVersion();
    const auto *undo = fixture.history()->nextUndoEntry();
    const auto retry = parameters.createAnchorCurve(request, clip, ParamInfo::Pitch, Param::Edited,
                                                    "phrase", anchors);
    QVERIFY(retry);
    QCOMPARE(retry.get().createdObjects.first().object,
             created.get().createdObjects.first().object);
    QCOMPARE(runtime.documentVersion(), committed);
    QCOMPARE(fixture.history()->nextUndoEntry(), undo);
    auto different = anchors;
    different.last().value = 6500;
    const auto changedPayload = parameters.createAnchorCurve(request, clip, ParamInfo::Pitch,
                                                             Param::Edited, "phrase", different);
    QVERIFY(isError(changedPayload, AutomationErrorCode::IdempotencyConflict));
    const auto changedDestination = parameters.createAnchorCurve(
        request, clip, ParamInfo::Pitch, Param::Envelope, "phrase", anchors);
    QVERIFY(isError(changedDestination, AutomationErrorCode::IdempotencyConflict));
    QCOMPARE(runtime.documentVersion(), committed);
    QCOMPARE(fixture.history()->nextUndoEntry(), undo);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    const auto empty = parameters.getParameter(runtime.documentVersion().documentId, clip,
                                               ParamInfo::Pitch, Param::Edited);
    QVERIFY(empty && empty.get().curves.isEmpty());
    QVERIFY(!fixture.history()->canUndo());
    QVERIFY(runtime.history().redo(commandContext(runtime)));
    QCOMPARE(parameters
                 .getParameter(runtime.documentVersion().documentId, clip, ParamInfo::Pitch,
                               Param::Edited)
                 .get()
                 .curves.first()
                 .id,
             identity);
}

void ProjectEditingTests::rejectedAnchorBatchPreservesEveryCurve_data() {
    QTest::addColumn<QString>("operation");
    QTest::newRow("moving-across-another-curve") << QStringLiteral("move");
    QTest::newRow("inserting-across-another-curve") << QStringLiteral("insert");
    QTest::newRow("removing-a-stale-batch-target") << QStringLiteral("remove");
    QTest::newRow("interpolating-a-stale-batch-target") << QStringLiteral("interpolation");
}

void ProjectEditingTests::rejectedAnchorBatchPreservesEveryCurve() {
    QFETCH(QString, operation);
    TestRuntime fixture;
    auto &runtime = fixture.runtime();
    auto &parameters = runtime.parameters();
    const auto clip = insertedSingingClip(runtime, insertedTrack(runtime, "Curves"), "Pitch");
    QVERIFY(parameters.createAnchorCurve(commandContext(runtime), clip, ParamInfo::Pitch,
                                         Param::Edited, "left",
                                         {
                                             {0,   6000, AnchorNode::Linear},
                                             {240, 6100, AnchorNode::Linear},
                                             {480, 6200, AnchorNode::Linear}
    }));
    QVERIFY(parameters.createAnchorCurve(
        commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited, "right",
        {
            {720, 6300, AnchorNode::Linear},
            {960, 6400, AnchorNode::Linear}
    }));
    const auto curves = parameters.getParameter(runtime.documentVersion().documentId, clip,
                                                ParamInfo::Pitch, Param::Edited);
    QVERIFY(curves && curves.get().curves.size() == 2);
    const auto first = curves.get().curves.first();
    const auto stale = first.nodes.at(1).id;
    QVERIFY(parameters.removeAnchor(commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited,
                                    stale));
    fixture.history()->reset();
    const auto before = runtime.documentVersion();
    const auto model = fixture.model().serialize();
    const auto rejected = [&] {
        if (operation == QStringLiteral("move"))
            return parameters.moveAnchors(
                commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited,
                {
                    {first.nodes.first().id, 120, 6100},
                    {first.nodes.last().id,  840, 6500}
            });
        if (operation == QStringLiteral("insert"))
            return parameters.insertAnchors(
                commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited, first.id,
                {
                    {240, 6100, AnchorNode::Hermite},
                    {840, 6500, AnchorNode::Hermite}
            });
        if (operation == QStringLiteral("remove"))
            return parameters.removeAnchors(commandContext(runtime), clip, ParamInfo::Pitch,
                                            Param::Edited, {first.nodes.first().id, stale});
        return parameters.setAnchorInterpolations(commandContext(runtime), clip, ParamInfo::Pitch,
                                                  Param::Edited, {first.nodes.first().id, stale},
                                                  AnchorNode::Hermite);
    }();
    QVERIFY(!rejected);
    QCOMPARE(rejected.getError().code,
             operation == QStringLiteral("move") || operation == QStringLiteral("insert")
                 ? AutomationErrorCode::InvalidArgument
                 : AutomationErrorCode::NotFound);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(fixture.model().serialize(), model);
    QVERIFY(!fixture.history()->canUndo());
    const auto after =
        parameters.getParameter(before.documentId, clip, ParamInfo::Pitch, Param::Edited);
    QVERIFY(after);
    QCOMPARE(after.get().curves.first().nodes.first().id, first.nodes.first().id);
    QCOMPARE(after.get().curves.last().id, curves.get().curves.last().id);
}

void ProjectEditingTests::adjacentAnchorCurvesMergeWithoutLosingNodes() {
    TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();
    auto &parameters = runtime.parameters();
    const auto clip = insertedSingingClip(runtime, insertedTrack(runtime, "Curves"), "Pitch");
    QVERIFY(parameters.createAnchorCurve(
        commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited, "left",
        {
            {0,   6000, AnchorNode::Linear },
            {240, 6200, AnchorNode::Hermite}
    }));
    QVERIFY(parameters.createAnchorCurve(
        commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited, "right",
        {
            {480, 6400, AnchorNode::Linear },
            {720, 6300, AnchorNode::Hermite}
    }));
    const auto curves = [&] {
        return parameters
            .getParameter(runtime.documentVersion().documentId, clip, ParamInfo::Pitch,
                          Param::Edited)
            .get()
            .curves;
    };
    const auto before = curves();
    QCOMPARE(before.size(), 2);
    testRuntime.history()->reset();
    QVERIFY(parameters.mergeAnchorCurves(commandContext(runtime), clip, ParamInfo::Pitch,
                                         Param::Edited, before.first().id, before.last().id));
    QCOMPARE(curves().size(), 1);
    const auto merged = curves().first();
    QCOMPARE(merged.id, before.first().id);
    QCOMPARE(merged.nodes.size(), 4);
    QCOMPARE(merged.nodes.at(2).position, 480);
    QCOMPARE(merged.nodes.at(2).value, 6400);
    QCOMPARE(merged.nodes.last().interpolation, AnchorNode::Hermite);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(curves().size(), 2);
    QCOMPARE(curves().last().id, before.last().id);
    QVERIFY(runtime.history().redo(commandContext(runtime)));
    QCOMPARE(curves().first().nodes.size(), 4);
}

void ProjectEditingTests::speakerMixModeTransitionsPreserveTrackInheritance() {
    TestRuntime fixture;
    auto &runtime = fixture.runtime();
    auto &parameters = runtime.parameters();
    const auto track = insertedTrack(runtime, QStringLiteral("Voice"));
    const auto clip = insertedSingingClip(runtime, track, QStringLiteral("Inherited mix"));
    const auto soft = speaker(QStringLiteral("soft"));
    const auto strong = speaker(QStringLiteral("strong"));
    const auto voice = singer(QStringLiteral("fixture"), {soft, strong});
    SpeakerMixModel::SpeakerMixData fixed;
    fixed.mode = SpeakerMixModel::SingerSourceMode::FixedMix;
    fixed.sources = {{soft}, {strong}};
    fixed.fixedWeights = {0.35};
    const Automation::SpeakerMixTargetDto trackTarget{Automation::SpeakerMixTargetKind::Track,
                                                      track.value()};
    const Automation::SpeakerMixTargetDto clipTarget{Automation::SpeakerMixTargetKind::Clip,
                                                     clip.value()};
    const auto read = [&](const Automation::SpeakerMixTargetDto target) {
        auto result = parameters.getSpeakerMix(runtime.documentVersion().documentId, target);
        TestSupport::expect(static_cast<bool>(result),
                            QStringLiteral("The current voice context must resolve"));
        return result ? result.get() : Automation::SpeakerMixSnapshotDto{};
    };
    const auto before = runtime.documentVersion();
    const auto preview = parameters.setFixedSpeakerMix(commandContext(runtime, true), trackTarget,
                                                       voice, soft, fixed);
    QVERIFY(preview && preview.get().validatedOnly && preview.get().changed);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(read(trackTarget).singer.isEmpty());
    QVERIFY(
        parameters.setFixedSpeakerMix(commandContext(runtime), trackTarget, voice, soft, fixed));
    QCOMPARE(read(trackTarget).mix.fixedWeights, fixed.fixedWeights);
    QVERIFY(read(clipTarget).inherited);
    QCOMPARE(read(clipTarget).singer.identifier(), voice.identifier());
    QCOMPARE(read(clipTarget).mix.fixedWeights, fixed.fixedWeights);
    const auto fixedVersion = runtime.documentVersion();
    const auto unchangedTrack =
        parameters.setFixedSpeakerMix(commandContext(runtime), trackTarget, voice, soft, fixed);
    QVERIFY(unchangedTrack && !unchangedTrack.get().changed);
    QCOMPARE(runtime.documentVersion(), fixedVersion);
    fixture.history()->reset();
    QVERIFY(parameters.enableClipDynamicSpeakerMix(commandContext(runtime), clip));
    auto dynamic = read(clipTarget);
    QVERIFY(!dynamic.inherited);
    QCOMPARE(dynamic.mix.mode, SpeakerMixModel::SingerSourceMode::DynamicMix);
    QCOMPARE(dynamic.mix.dynamicKeyframes.size(), 1);
    QCOMPARE(dynamic.mix.dynamicKeyframes.first().tick, 0);
    QCOMPARE(dynamic.mix.dynamicKeyframes.first().weights, fixed.fixedWeights);
    QCOMPARE(read(trackTarget).mix.mode, SpeakerMixModel::SingerSourceMode::FixedMix);
    const auto unchangedDynamic =
        parameters.enableClipDynamicSpeakerMix(commandContext(runtime), clip);
    QVERIFY(unchangedDynamic && !unchangedDynamic.get().changed);
    auto insert = commandContext(runtime);
    insert.idempotencyKey = QStringLiteral("add-blend-change");
    QVERIFY(parameters.insertSpeakerMixKeyframe(insert, clip, 480, QVector<double>{0.8}));
    dynamic = read(clipTarget);
    QCOMPARE(dynamic.mix.dynamicKeyframes.size(), 2);
    const auto keyframeId = dynamic.mix.dynamicKeyframes.last().id;
    const auto insertedVersion = runtime.documentVersion();
    const auto *undo = fixture.history()->nextUndoEntry();
    QVERIFY(parameters.insertSpeakerMixKeyframe(insert, clip, 480, QVector<double>{0.8}));
    QCOMPARE(runtime.documentVersion(), insertedVersion);
    QCOMPARE(fixture.history()->nextUndoEntry(), undo);
    QCOMPARE(read(clipTarget).mix.dynamicKeyframes.last().id, keyframeId);
    const auto conflictingRetry =
        parameters.insertSpeakerMixKeyframe(insert, clip, 720, QVector<double>{0.8});
    QVERIFY(isError(conflictingRetry, AutomationErrorCode::IdempotencyConflict));
    const auto invalidFixed = parameters.setFixedSpeakerMix(commandContext(runtime), clipTarget,
                                                            voice, soft, dynamic.mix);
    QVERIFY(isError(invalidFixed, AutomationErrorCode::InvalidArgument));
    QCOMPARE(runtime.documentVersion(), insertedVersion);
    QVERIFY(parameters.disableClipDynamicSpeakerMix(commandContext(runtime), clip));
    const auto restoredFixed = read(clipTarget);
    QVERIFY(!restoredFixed.inherited);
    QCOMPARE(restoredFixed.mix.mode, SpeakerMixModel::SingerSourceMode::FixedMix);
    QCOMPARE(restoredFixed.mix.fixedWeights, fixed.fixedWeights);
    QVERIFY(restoredFixed.mix.dynamicKeyframes.isEmpty());
    const auto unchangedFixed =
        parameters.disableClipDynamicSpeakerMix(commandContext(runtime), clip);
    QVERIFY(unchangedFixed && !unchangedFixed.get().changed);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(read(clipTarget).mix.dynamicKeyframes.last().id, keyframeId);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(read(clipTarget).mix.dynamicKeyframes.size(), 1);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QVERIFY(read(clipTarget).inherited);
    QCOMPARE(read(clipTarget).mix.fixedWeights, fixed.fixedWeights);
    QVERIFY(!fixture.history()->canUndo());
    auto overrideMix = fixed;
    overrideMix.fixedWeights = {0.75};
    QVERIFY(parameters.setFixedSpeakerMix(commandContext(runtime), clipTarget, voice, soft,
                                          overrideMix));
    QVERIFY(!read(clipTarget).inherited);
    QCOMPARE(read(clipTarget).mix.fixedWeights, overrideMix.fixedWeights);
    QCOMPARE(read(trackTarget).mix.fixedWeights, fixed.fixedWeights);
    const auto unchangedOverride = parameters.setFixedSpeakerMix(
        commandContext(runtime), clipTarget, voice, soft, overrideMix);
    QVERIFY(unchangedOverride && !unchangedOverride.get().changed);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QVERIFY(read(clipTarget).inherited);
    QVERIFY(!fixture.history()->canUndo());
}

void ProjectEditingTests::dynamicSpeakerKeyframesEditAndUndo() {
    TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();
    auto &parameters = runtime.parameters();
    const auto clip = insertedSingingClip(runtime, insertedTrack(runtime, "Voice"), "Mix");
    const auto soft = speaker("soft");
    const auto strong = speaker("strong");
    QVERIFY(parameters.enableClipDynamicSpeakerMix(commandContext(runtime), clip,
                                                   singer("fixture", {soft, strong}), soft,
                                                   dynamicMix(soft, strong)));
    const auto mix = [&] { return clipSnapshot(runtime, clip)->data.ownSpeakerMixData; };
    testRuntime.history()->reset();
    QVERIFY(parameters.insertSpeakerMixKeyframe(commandContext(runtime), clip, 480));
    auto frames = mix().dynamicKeyframes;
    QCOMPARE(frames.size(), 3);
    QCOMPARE(frames.at(1).tick, 480);
    QCOMPARE(frames.at(1).weights, QVector<double>{0.5});
    const auto middle = Automation::SpeakerMixKeyframeId(frames.at(1).id);
    const auto last = Automation::SpeakerMixKeyframeId(frames.last().id);
    QVERIFY(parameters.setSpeakerMixKeyframeWeights(commandContext(runtime), clip, middle,
                                                    {0.25, 0.75}));
    QCOMPARE(mix().dynamicKeyframes.at(1).weights, QVector<double>{0.25});
    const auto version = runtime.documentVersion();
    QVERIFY(!parameters.moveSpeakerMixKeyframes(commandContext(runtime), clip,
                                                {
                                                    {middle, 720},
                                                    {last,   720}
    }));
    QCOMPARE(runtime.documentVersion(), version);
    QCOMPARE(mix().dynamicKeyframes.at(1).tick, 480);
    QVERIFY(parameters.moveSpeakerMixKeyframes(commandContext(runtime), clip,
                                               {
                                                   {middle, 600 },
                                                   {last,   1200}
    }));
    QCOMPARE(mix().dynamicKeyframes.at(1).tick, 600);
    QCOMPARE(mix().dynamicKeyframes.last().tick, 1200);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(mix().dynamicKeyframes.at(1).tick, 480);
    QCOMPARE(mix().dynamicKeyframes.last().tick, 960);
    QVERIFY(parameters.removeSpeakerMixKeyframes(commandContext(runtime), clip, {middle, last}));
    QCOMPARE(mix().dynamicKeyframes.size(), 1);
    QCOMPARE(mix().dynamicKeyframes.first().tick, 0);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(mix().dynamicKeyframes.size(), 3);
    QVERIFY(parameters.setClipDynamicSpeakerMixBypassed(commandContext(runtime), clip, true));
    QVERIFY(mix().dynamicBypassed);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QVERIFY(!mix().dynamicBypassed);
}

void ProjectEditingTests::batchTrackOrderAndClipTrimming() {
    TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();
    const auto first = insertedTrack(runtime, "A");
    const auto second = insertedTrack(runtime, "B");
    const auto third = insertedTrack(runtime, "C");
    const auto fourth = insertedTrack(runtime, "D");
    const auto clip = insertedSingingClip(runtime, second, "Phrase", 480);
    const auto notes = insertedNotes(runtime, clip, {noteDraft(120, 480, 60, "la")});
    QCOMPARE(notes.size(), 1);
    const auto order = [&] {
        QList<TrackId> result;
        const auto project = runtime.project().getProject(runtime.documentVersion().documentId);
        for (const auto &track : project.get().tracks)
            result.append(track.id);
        return result;
    };
    testRuntime.history()->reset();
    QVERIFY(runtime.project().moveTracks(commandContext(runtime), {third, first}, 4));
    QCOMPARE(order(), (QList<TrackId>{second, fourth, first, third}));
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(order(), (QList<TrackId>{first, second, third, fourth}));
    const auto timing = clipSnapshot(runtime, clip)->data.properties;
    QVERIFY(runtime.project().resizeClipLeft(commandContext(runtime), clip, 720));
    const auto trimmed = clipSnapshot(runtime, clip)->data.properties;
    QCOMPARE(trimmed.start + trimmed.clipStart, 720);
    QCOMPARE(trimmed.start + trimmed.clipStart + trimmed.clipLen,
             timing.start + timing.clipStart + timing.clipLen);
    QCOMPARE(noteSnapshot(runtime, clip, notes.first())->data.localStart, 120);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QVERIFY(sameClipTiming(clipSnapshot(runtime, clip)->data.properties, timing));
    QVERIFY(runtime.project().resizeClipRight(commandContext(runtime), clip, 2400));
    const auto right = clipSnapshot(runtime, clip)->data.properties;
    QCOMPARE(right.start + right.clipStart + right.clipLen, 2400);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QVERIFY(sameClipTiming(clipSnapshot(runtime, clip)->data.properties, timing));
}

void ProjectEditingTests::noteSearch_data() {
    QTest::addColumn<QString>("query");
    QTest::addColumn<QString>("mode");
    QTest::addColumn<bool>("caseSensitive");
    QTest::addColumn<bool>("regex");
    QTest::addColumn<QStringList>("expected");
    QTest::newRow("case-insensitive") << QStringLiteral("la") << QStringLiteral("exact") << false
                                      << false << QStringList{"La", "la"};
    QTest::newRow("prefix") << QStringLiteral("la") << QStringLiteral("starts_with") << true
                            << false << QStringList{"la", "lala"};
    QTest::newRow("contains") << QStringLiteral("a") << QStringLiteral("contains") << true << false
                              << QStringList{"La", "la", "lala"};
    QTest::newRow("expression") << QStringLiteral("l(a)+") << QStringLiteral("exact") << false
                                << true << QStringList{"La", "la"};
    QTest::newRow("no-match") << QStringLiteral("missing") << QStringLiteral("contains") << false
                              << false << QStringList{};
}

void ProjectEditingTests::noteSearch() {
    QFETCH(QString, query);
    QFETCH(QString, mode);
    QFETCH(bool, caseSensitive);
    QFETCH(bool, regex);
    QFETCH(QStringList, expected);
    TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();
    const auto clip = insertedSingingClip(runtime, insertedTrack(runtime, "Lyrics"), "Phrase");
    QCOMPARE(insertedNotes(runtime, clip,
                           {noteDraft(0, 120, 60, "La"), noteDraft(120, 120, 60, "la"),
                            noteDraft(240, 120, 60, "lala"), noteDraft(360, 120, 60, "mi")})
                 .size(),
             4);
    const auto before = runtime.documentVersion();
    const auto result =
        runtime.notes().searchNotes(before.documentId, clip, query, mode, caseSensitive, regex);
    QVERIFY(result);
    QStringList found;
    for (const auto &match : result.get())
        found.append(match.lyric);
    QCOMPARE(found, expected);
    QCOMPARE(runtime.documentVersion(), before);
}

void ProjectEditingTests::splitAtPreservesPhraseAndUndo() {
    NoteFixture fixture;
    auto &runtime = fixture.testRuntime.runtime();
    const auto before = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId)->data;
    const auto split = before.localStart + 120;
    fixture.testRuntime.history()->reset();
    QVERIFY(runtime.notes().splitNoteAt(commandContext(runtime), fixture.clipId,
                                        fixture.firstNoteId, split));
    const auto result =
        runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.clipId);
    QVERIFY(result);
    QCOMPARE(result.get().size(), 3);
    QCOMPARE(result.get().at(0).data.length, 120);
    QCOMPARE(result.get().at(1).data.localStart, split);
    QCOMPARE(result.get().at(1).data.length, before.length - 120);
    QCOMPARE(result.get().at(1).data.keyIndex, before.keyIndex);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId)->data.length,
             before.length);
    QCOMPARE(
        runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.clipId).get().size(),
        2);
}

void ProjectEditingTests::trackEditing() {
    TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();

    TrackId first;
    TrackId second;
    TrackId third;
    {
        // Automation::OperationIds::tracks::insert / QStringLiteral("validation-and-create")

        const auto initial = runtime.documentVersion();
        const auto invalid = runtime.project().insertTrack(commandContext(runtime), -1,
                                                           trackDraft(QStringLiteral("invalid")));
        QVERIFY2((isError(invalid, AutomationErrorCode::InvalidArgument, QStringLiteral("index"))),
                 qPrintable(QStringLiteral("negative insertion index must be rejected")));
        const auto preview = runtime.project().insertTrack(
            commandContext(runtime, true), 0,
            trackDraft(QStringLiteral("First"), QStringLiteral("track-first")));
        QVERIFY2((preview && preview.get().validatedOnly && preview.get().changed &&
                  preview.get().createdObjects.isEmpty() && runtime.documentVersion() == initial),
                 qPrintable(QStringLiteral("validate-only must not allocate or mutate")));
        const auto insert = runtime.project().insertTrack(
            commandContext(runtime), 0,
            trackDraft(QStringLiteral("First"), QStringLiteral("track-first")));
        QVERIFY2((insert && insert.get().current.revision == initial.revision + 1 &&
                  insert.get().createdObjects.size() == 1),
                 qPrintable(QStringLiteral("track insertion must be one revision with binding")));
        if (insert)
            first = TrackId(insert.get().affectedObjects.first().value);
        second = insertedTrack(runtime, QStringLiteral("Second"));
        third = insertedTrack(runtime, QStringLiteral("Third"));
        QVERIFY2((first.isValid() && second.isValid() && third.isValid()),
                 qPrintable(QStringLiteral("fixture tracks must be created")));
    };

    {
        // Automation::OperationIds::project::get / QStringLiteral("ordered-value-snapshot")

        const auto result = runtime.project().getProject(runtime.documentVersion().documentId);
        QVERIFY2((result && result.get().document == runtime.documentVersion() &&
                  result.get().tracks.size() == 3 && result.get().tracks.at(0).id == first &&
                  result.get().tracks.at(1).id == second && result.get().tracks.at(2).id == third),
                 qPrintable(QStringLiteral("project query must preserve ordered typed IDs")));
        const auto wrong = runtime.project().getProject(Automation::DocumentId::create());
        QVERIFY2((!wrong && wrong.getError().code == AutomationErrorCode::DocumentChanged &&
                  wrong.getError().operationId == Automation::OperationIds::project::get),
                 qPrintable(QStringLiteral("query must reject an old document generation")));
    };

    {
        // Automation::OperationIds::tracks::move / QStringLiteral("preview-noop-commit-undo")

        testRuntime.history()->reset();
        const auto base = runtime.documentVersion();
        const auto invalid = runtime.project().moveTrack(commandContext(runtime), first, 4);
        QVERIFY2((isError(invalid, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("target_index")) &&
                  runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("out-of-range target must not mutate")));
        const auto preview = runtime.project().moveTrack(commandContext(runtime, true), first, 2);
        QVERIFY2((preview && preview.get().validatedOnly && preview.get().changed &&
                  runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("move preview must predict without mutation")));
        const auto move = runtime.project().moveTrack(commandContext(runtime), first, 2);
        const auto moved = runtime.project().getProject(runtime.documentVersion().documentId);
        QVERIFY2((move && move.get().current.revision == base.revision + 1 && moved &&
                  moved.get().tracks.at(1).id == first),
                 qPrintable(QStringLiteral("track move must update order once")));
        const auto noOp = runtime.project().moveTrack(commandContext(runtime), first, 2);
        QVERIFY2((noOp && !noOp.get().changed && runtime.documentVersion() == move.get().current),
                 qPrintable(QStringLiteral("moving to current index must be a no-op")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        const auto restored = runtime.project().getProject(runtime.documentVersion().documentId);
        QVERIFY2(
            (undo && undo.get().changed && restored && restored.get().tracks.first().id == first),
            qPrintable(QStringLiteral("track move must undo as one entry")));

        if (!restored || restored.get().tracks.first().id != first)
            runtime.history().undo(commandContext(runtime));
        const auto endBase = runtime.documentVersion();
        const auto endPreview =
            runtime.project().moveTrack(commandContext(runtime, true), first, 3);
        QVERIFY2(
            (endPreview && endPreview.get().changed && endPreview.get().validatedOnly &&
             runtime.documentVersion() == endBase),
            qPrintable(QStringLiteral("end insertion preview must accept index equal to size")));
        const auto moveToEnd = runtime.project().moveTrack(commandContext(runtime), first, 3);
        const auto atEnd = runtime.project().getProject(runtime.documentVersion().documentId);
        QVERIFY2((moveToEnd && atEnd && atEnd.get().tracks.last().id == first),
                 qPrintable(QStringLiteral("track must support insertion after the last row")));
        if (moveToEnd)
            runtime.history().undo(commandContext(runtime));
    };

    {
        // Automation::OperationIds::tracks::set_properties /
        // QStringLiteral("atomic-properties")

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
        QVERIFY2((isError(rejected, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("properties.control")) &&
                  runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("non-finite track control must be atomic failure")));
        const auto preview =
            runtime.project().setTrackProperties(commandContext(runtime, true), edit);
        QVERIFY2((preview && preview.get().changed && preview.get().validatedOnly &&
                  runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("property preview must be side-effect free")));
        const auto changed = runtime.project().setTrackProperties(commandContext(runtime), edit);
        const auto snapshot = trackSnapshot(runtime, second);
        QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && snapshot &&
                  snapshot->data.name == edit.name && snapshot->data.gain == edit.gain &&
                  snapshot->data.pan == edit.pan && snapshot->data.mute &&
                  snapshot->data.colorIndex == 1),
                 qPrintable(QStringLiteral(
                     "track property edits must commit atomically without resetting color")));
        const auto noOp = runtime.project().setTrackProperties(commandContext(runtime), edit);
        QVERIFY2(
            (noOp && !noOp.get().changed && runtime.documentVersion() == changed.get().current),
            qPrintable(QStringLiteral("identical track properties must be a no-op")));
    };

    {
        // Automation::OperationIds::tracks::set_color /
        // QStringLiteral("history-state-and-noop")

        testRuntime.history()->reset();
        const auto base = runtime.documentVersion();
        const auto invalid = runtime.project().setTrackColor(commandContext(runtime), third, -1);
        QVERIFY2(
            (isError(invalid, AutomationErrorCode::InvalidArgument, QStringLiteral("color_index"))),
            qPrintable(QStringLiteral("negative color must be rejected")));
        const auto changed = runtime.project().setTrackColor(commandContext(runtime), third, 7);
        const auto state = runtime.history().getState(runtime.documentVersion().documentId);
        const auto snapshot = trackSnapshot(runtime, third);
        QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && snapshot &&
                  snapshot->data.colorIndex == 7 && state && state.get().canUndo),
                 qPrintable(QStringLiteral("color must advance revision with one History entry")));
        const auto noOp = runtime.project().setTrackColor(commandContext(runtime), third, 7);
        QVERIFY2(
            (noOp && !noOp.get().changed && runtime.documentVersion() == changed.get().current),
            qPrintable(QStringLiteral("identical color must be a no-op")));
    };

    {
        // Automation::OperationIds::tracks::set_default_language /
        // QStringLiteral("unicode-and-noop")

        testRuntime.history()->reset();
        const auto base = runtime.documentVersion();
        const auto empty = runtime.project().setTrackDefaultLanguage(commandContext(runtime), third,
                                                                     QStringLiteral("  "));
        QVERIFY2((isError(empty, AutomationErrorCode::InvalidArgument, QStringLiteral("language"))),
                 qPrintable(QStringLiteral("blank language must be rejected")));
        const auto changed = runtime.project().setTrackDefaultLanguage(
            commandContext(runtime), third, QStringLiteral("zh-汉字"));
        const auto snapshot = trackSnapshot(runtime, third);
        QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && snapshot &&
                  snapshot->data.defaultLanguage == QStringLiteral("zh-汉字")),
                 qPrintable(QStringLiteral("Unicode language ID must round-trip")));
        const auto noOp = runtime.project().setTrackDefaultLanguage(commandContext(runtime), third,
                                                                    QStringLiteral("zh-汉字"));
        const auto state = runtime.history().getState(runtime.documentVersion().documentId);
        QVERIFY2((noOp && !noOp.get().changed && state && state.get().canUndo),
                 qPrintable(QStringLiteral("language state change must create one History entry")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        const auto restored = trackSnapshot(runtime, third);
        QVERIFY2((undo && restored && restored->data.defaultLanguage == QStringLiteral("en")),
                 qPrintable(QStringLiteral("track language change must undo atomically")));
    };
}

void ProjectEditingTests::singingClipEditing() {
    TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();
    const auto first = insertedTrack(runtime, QStringLiteral("First"));
    const auto second = insertedTrack(runtime, QStringLiteral("Second"));
    const auto third = insertedTrack(runtime, QStringLiteral("Third"));
    QVERIFY(first.isValid() && second.isValid() && third.isValid());
    ClipId clip;
    {
        // Automation::OperationIds::clips::insert /
        // QStringLiteral("empty-invalid-preview-create")

        const auto base = runtime.documentVersion();
        const auto empty = runtime.project().insertClips(commandContext(runtime), {});
        QVERIFY2((empty && !empty.get().changed && runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("empty insert must be a no-op")));
        auto invalidDraft = singingClipDraft(QStringLiteral("Invalid"));
        invalidDraft.properties.length = -1;
        const auto invalid = runtime.project().insertClips(
            commandContext(runtime), {
                                         {.trackId = second, .clip = invalidDraft}
        });
        QVERIFY2((isError(invalid, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("clip.properties"))),
                 qPrintable(QStringLiteral("invalid clip geometry must be rejected")));
        auto visibleOverflowDraft = singingClipDraft(QStringLiteral("Visible Overflow"));
        visibleOverflowDraft.properties.start = std::numeric_limits<int>::max();
        visibleOverflowDraft.properties.clipStart = 0;
        visibleOverflowDraft.properties.clipLen = 1;
        const auto visibleOverflow = runtime.project().insertClips(
            commandContext(runtime), {
                                         {.trackId = second, .clip = visibleOverflowDraft}
        });
        QVERIFY2((isError(visibleOverflow, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("clip.properties"))),
                 qPrintable(QStringLiteral("visible clip end must fit the model tick type")));
        auto localOverflowDraft = singingClipDraft(QStringLiteral("Local Overflow"));
        localOverflowDraft.properties.start = -std::numeric_limits<int>::max();
        localOverflowDraft.properties.clipStart = std::numeric_limits<int>::max();
        localOverflowDraft.properties.clipLen = 1;
        const auto localOverflow = runtime.project().insertClips(
            commandContext(runtime), {
                                         {.trackId = second, .clip = localOverflowDraft}
        });
        QVERIFY2((isError(localOverflow, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("clip.properties"))),
                 qPrintable(QStringLiteral("clip-local end must fit the model tick type")));
        auto noteOverflowDraft = singingClipDraft(QStringLiteral("Note Overflow"));
        noteOverflowDraft.notes.append(
            noteDraft(std::numeric_limits<int>::max() - 1, 2, 60, QStringLiteral("bad")));
        const auto noteOverflow = runtime.project().insertClips(
            commandContext(runtime), {
                                         {.trackId = second, .clip = noteOverflowDraft}
        });
        Automation::CurveDraftDto overflowCurve;
        overflowCurve.localStart = std::numeric_limits<int>::max() - 1;
        overflowCurve.step = 2;
        overflowCurve.values = {6000, 6010};
        Automation::ParamCurvesDraftDto overflowParameter;
        overflowParameter.name = ParamInfo::Pitch;
        overflowParameter.type = Param::Edited;
        overflowParameter.curves = {overflowCurve};
        auto curveOverflowDraft = singingClipDraft(QStringLiteral("Curve Overflow"));
        curveOverflowDraft.params = {overflowParameter};
        const auto curveOverflow = runtime.project().insertClips(
            commandContext(runtime), {
                                         {.trackId = second, .clip = curveOverflowDraft}
        });
        QVERIFY2((isError(noteOverflow, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("clip.notes")) &&
                  isError(curveOverflow, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("clip.parameters.curves.values"))),
                 qPrintable(QStringLiteral("nested note and draw ranges must fit the model tick "
                                           "type")));
        const auto draft =
            singingClipDraft(QStringLiteral("歌声 Clip"), QStringLiteral("clip-main"));
        const auto preview = runtime.project().insertClips(commandContext(runtime, true),
                                                           {
                                                               {.trackId = second, .clip = draft}
        });
        QVERIFY2((preview && preview.get().validatedOnly &&
                  preview.get().createdObjects.isEmpty() && runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("clip preview must not allocate IDs")));
        const auto insert = runtime.project().insertClips(commandContext(runtime),
                                                          {
                                                              {.trackId = second, .clip = draft}
        });
        QVERIFY2((insert && insert.get().createdObjects.size() == 1 &&
                  insert.get().createdObjects.first().clientRef == QStringLiteral("clip-main")),
                 qPrintable(QStringLiteral("clip create must bind client_ref")));
        if (insert)
            clip = ClipId(insert.get().affectedObjects.first().value);
    };

    {
        // Automation::OperationIds::clips::duplicate /
        // QStringLiteral("translated-range-overflow-is-atomic")

        auto farDraft = singingClipDraft(QStringLiteral("Far Clip"), QStringLiteral("clip-far"));
        farDraft.properties.start = std::numeric_limits<int>::max() - 10;
        farDraft.properties.length = 1;
        farDraft.properties.clipStart = 0;
        farDraft.properties.clipLen = 1;
        const auto inserted = runtime.project().insertClips(
            commandContext(runtime), {
                                         {.trackId = second, .clip = farDraft}
        });
        const auto farClip = inserted && !inserted.get().affectedObjects.isEmpty()
                                 ? ClipId(inserted.get().affectedObjects.first().value)
                                 : ClipId{};
        QVERIFY2((farClip.isValid()),
                 qPrintable(QStringLiteral("far clip fixture must be created")));
        testRuntime.history()->reset();
        const auto base = runtime.documentVersion();
        const Automation::ClipDuplicateDestinationDto destination{
            .targetTrackId = second,
            .targetStart = 100,
        };
        const auto preview = runtime.project().duplicateClips(commandContext(runtime, true),
                                                              {clip, farClip}, destination);
        const auto commit =
            runtime.project().duplicateClips(commandContext(runtime), {clip, farClip}, destination);
        QVERIFY2((isError(preview, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("destination.target_start")) &&
                  isError(commit, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("destination.target_start")) &&
                  runtime.documentVersion() == base),
                 qPrintable(
                     QStringLiteral("translated duplicate ranges must be checked before preview or "
                                    "commit")));
    };

    {
        // Automation::OperationIds::clips::move /
        // QStringLiteral("visible-range-overflow-is-atomic")

        testRuntime.history()->reset();
        const auto base = runtime.documentVersion();
        const QList<Automation::ClipMoveDto> moves{
            {.id = clip, .targetTrackId = second, .start = std::numeric_limits<int>::max()},
        };
        const auto preview = runtime.project().moveClips(commandContext(runtime, true), moves);
        const auto commit = runtime.project().moveClips(commandContext(runtime), moves);
        QVERIFY2(
            (isError(preview, AutomationErrorCode::InvalidArgument,
                     QStringLiteral("moves.start")) &&
             isError(commit, AutomationErrorCode::InvalidArgument, QStringLiteral("moves.start")) &&
             runtime.documentVersion() == base),
            qPrintable(QStringLiteral("clip moves must reject an overflowing visible "
                                      "range before preview or commit")));
    };

    {
        // Automation::OperationIds::clips::set_properties /
        // QStringLiteral("legacy-range-move-and-edit-atomically")

        testRuntime.history()->reset();
        const auto before = clipSnapshot(runtime, clip);
        QVERIFY2((before.has_value()), qPrintable(QStringLiteral("fixture clip must exist")));
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
        QVERIFY2(
            (isError(rejected, AutomationErrorCode::InvalidArgument, QStringLiteral("properties"))),
            qPrintable(QStringLiteral("invalid clip properties must not partially move")));
        auto visibleOverflow = edit;
        visibleOverflow.start = std::numeric_limits<int>::max();
        visibleOverflow.clipStart = 0;
        visibleOverflow.clipLen = 1;
        const auto rejectedVisibleOverflow = runtime.project().setClipProperties(
            commandContext(runtime, true), visibleOverflow, third);
        QVERIFY2(
            (isError(rejectedVisibleOverflow, AutomationErrorCode::InvalidArgument,
                     QStringLiteral("properties"))),
            qPrintable(QStringLiteral("clip edit must reject an unrepresentable visible end")));
        auto localOverflow = edit;
        localOverflow.start = -std::numeric_limits<int>::max();
        localOverflow.clipStart = std::numeric_limits<int>::max();
        localOverflow.clipLen = 1;
        const auto rejectedLocalOverflow = runtime.project().setClipProperties(
            commandContext(runtime, true), localOverflow, third);
        QVERIFY2(
            (isError(rejectedLocalOverflow, AutomationErrorCode::InvalidArgument,
                     QStringLiteral("properties"))),
            qPrintable(QStringLiteral("clip edit must reject an unrepresentable clip-local end")));
        const auto base = runtime.documentVersion();
        const auto preview =
            runtime.project().setClipProperties(commandContext(runtime, true), edit, third);
        QVERIFY2((preview && preview.get().changed && preview.get().validatedOnly &&
                  runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("clip edit preview must be side-effect free")));
        const auto changed =
            runtime.project().setClipProperties(commandContext(runtime), edit, third);
        const auto after = clipSnapshot(runtime, clip);
        QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && after &&
                  after->trackId == third && after->data.properties.name == edit.name &&
                  after->data.properties.start == edit.start &&
                  after->data.properties.length == edit.length &&
                  after->data.properties.clipStart == edit.clipStart &&
                  after->data.properties.clipLen == edit.clipLen && after->data.properties.mute),
                 qPrintable(QStringLiteral(
                     "legacy clip range and move must commit once without normalization")));
        const auto noOp = runtime.project().setClipProperties(commandContext(runtime), edit, third);
        QVERIFY2(
            (noOp && !noOp.get().changed && runtime.documentVersion() == changed.get().current),
            qPrintable(QStringLiteral("identical clip edit must be a no-op")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        const auto restored = clipSnapshot(runtime, clip);
        QVERIFY2((undo && restored && restored->trackId == second &&
                  restored->data.properties.name == before->data.properties.name),
                 qPrintable(QStringLiteral("combined clip move/edit must undo atomically")));
    };

    {
        // Automation::OperationIds::clips::set_default_language /
        // QStringLiteral("validation-revision-history")

        testRuntime.history()->reset();
        const auto blank = runtime.project().setSingingClipDefaultLanguage(
            commandContext(runtime), clip, QStringLiteral(""));
        QVERIFY2((isError(blank, AutomationErrorCode::InvalidArgument, QStringLiteral("language"))),
                 qPrintable(QStringLiteral("empty clip language must be rejected")));
        const auto base = runtime.documentVersion();
        const auto changed = runtime.project().setSingingClipDefaultLanguage(
            commandContext(runtime), clip, QStringLiteral("ja"));
        const auto snapshot = clipSnapshot(runtime, clip);
        const auto state = runtime.history().getState(runtime.documentVersion().documentId);
        QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && snapshot &&
                  snapshot->data.defaultLanguage == QStringLiteral("ja") && state &&
                  state.get().canUndo),
                 qPrintable(QStringLiteral("clip language must advance revision with History")));
        const auto noOp = runtime.project().setSingingClipDefaultLanguage(
            commandContext(runtime), clip, QStringLiteral("ja"));
        QVERIFY2((noOp && !noOp.get().changed),
                 qPrintable(QStringLiteral("identical clip language must be a no-op")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        const auto restored = clipSnapshot(runtime, clip);
        QVERIFY2((undo && restored && restored->data.defaultLanguage == QStringLiteral("en")),
                 qPrintable(QStringLiteral("clip language change must undo atomically")));
    };

    {
        // Automation::OperationIds::clips::remove / QStringLiteral("duplicates-preview-undo")

        testRuntime.history()->reset();
        const auto duplicate = runtime.project().removeClips(commandContext(runtime), {clip, clip});
        QVERIFY2(
            (isError(duplicate, AutomationErrorCode::InvalidArgument, QStringLiteral("clip_ids"))),
            qPrintable(QStringLiteral("duplicate clip IDs must fail atomically")));
        const auto empty = runtime.project().removeClips(commandContext(runtime), {});
        QVERIFY2((empty && !empty.get().changed),
                 qPrintable(QStringLiteral("empty clip removal must be a no-op")));
        const auto base = runtime.documentVersion();
        const auto preview = runtime.project().removeClips(commandContext(runtime, true), {clip});
        QVERIFY2((preview && preview.get().changed && preview.get().validatedOnly &&
                  clipSnapshot(runtime, clip).has_value() && runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("clip removal preview must preserve clip")));
        const auto removed = runtime.project().removeClips(commandContext(runtime), {clip});
        QVERIFY2((removed && !clipSnapshot(runtime, clip).has_value() &&
                  removed.get().current.revision == base.revision + 1),
                 qPrintable(QStringLiteral("clip removal must commit once")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        QVERIFY2((undo && clipSnapshot(runtime, clip).has_value()),
                 qPrintable(QStringLiteral("clip removal must be reversible")));
    };
}

void ProjectEditingTests::legacyAudioClipEditing() {
    TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();
    const auto first = insertedTrack(runtime, QStringLiteral("First"));
    const auto second = insertedTrack(runtime, QStringLiteral("Second"));
    const auto third = insertedTrack(runtime, QStringLiteral("Third"));
    QVERIFY(first.isValid() && second.isValid() && third.isValid());
    ClipId legacyAudioClip;
    {
        // Automation::OperationIds::clips::insert / QStringLiteral("legacy-audio-range-create")

        const auto insert = runtime.project().insertClips(
            commandContext(runtime), {
                                         {.trackId = second,
                                          .clip = audioClipDraft(QStringLiteral("Legacy Audio"),
                                          QStringLiteral("legacy-audio"))}
        });
        if (insert && !insert.get().affectedObjects.isEmpty())
            legacyAudioClip = ClipId(insert.get().affectedObjects.first().value);
        const auto snapshot = clipSnapshot(runtime, legacyAudioClip);
        QVERIFY2(
            (insert && snapshot && snapshot->data.type == Automation::ClipDraftDto::Type::Audio &&
             snapshot->data.properties.length == 100 && snapshot->data.properties.clipStart == 50 &&
             snapshot->data.properties.clipLen == 100),
            qPrintable(
                QStringLiteral("legacy audio geometry must be inserted without normalization")));
        if (!snapshot)
            return;

        const auto reusable = Automation::validate(snapshot->data);
        QVERIFY2((static_cast<bool>(reusable)),
                 qPrintable(QStringLiteral("a legacy audio snapshot must validate for reuse")));
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
        QVERIFY2((copied && copied.get().createdObjects.size() == 1 && copiedSnapshot &&
                  sameClipTiming(copiedSnapshot->data.properties, copiedDraft.properties)),
                 qPrintable(QStringLiteral(
                     "a pasted legacy audio snapshot must preserve its exact timing geometry")));

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
                ? clipSnapshot(runtime, ClipId(conflicting.get().affectedObjects.first().value))
                : std::nullopt;
        QVERIFY2(
            (conflicting && conflictingSnapshot &&
             conflictingSnapshot->data.properties.start +
                     conflictingSnapshot->data.properties.clipStart ==
                 conflictingDraft.properties.start + conflictingDraft.properties.clipStart &&
             conflictingSnapshot->data.properties.clipLen != conflictingDraft.properties.clipLen &&
             conflictingSnapshot->data.properties.length >=
                 conflictingSnapshot->data.properties.clipStart +
                     conflictingSnapshot->data.properties.clipLen &&
             conflictingSnapshot->data.properties.playLengthMs ==
                 conflictingDraft.properties.playLengthMs),
            qPrintable(QStringLiteral(
                "an inconsistent anchored draft must reconcile ticks to realtime truth")));

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
                ? clipSnapshot(runtime, ClipId(oversized.get().affectedObjects.first().value))
                : std::nullopt;
        QVERIFY2(
            (oversized && oversizedSnapshot &&
             oversizedSnapshot->data.properties.start == oversizedDraft.properties.start &&
             oversizedSnapshot->data.properties.clipStart == oversizedDraft.properties.clipStart &&
             oversizedSnapshot->data.properties.clipLen == oversizedDraft.properties.clipLen &&
             oversizedSnapshot->data.properties.length != oversizedDraft.properties.length &&
             oversizedSnapshot->data.properties.length ==
                 oversizedSnapshot->data.properties.clipStart +
                     oversizedSnapshot->data.properties.clipLen),
            qPrintable(QStringLiteral(
                "an oversized anchored length must reconcile to its material duration")));
    };

    {
        // Automation::OperationIds::clips::set_properties /
        // QStringLiteral("legacy-audio-metadata-edit-preserves-range")

        testRuntime.history()->reset();
        const auto before = clipSnapshot(runtime, legacyAudioClip);
        QVERIFY2((before.has_value()),
                 qPrintable(QStringLiteral("legacy audio fixture must exist")));
        if (!before)
            return;
        auto edit = before->data.properties;
        edit.id = legacyAudioClip;
        edit.name = QStringLiteral("Renamed Legacy Audio");
        edit.gain = 0.75;
        const auto changed =
            runtime.project().setClipProperties(commandContext(runtime), edit, second);
        const auto after = clipSnapshot(runtime, legacyAudioClip);
        QVERIFY2((changed && after && after->trackId == second &&
                  sameClipTiming(after->data.properties, before->data.properties)),
                 qPrintable(QStringLiteral(
                     "audio metadata edit must preserve legacy ticks and realtime truth")));
        const auto undoEdit = runtime.history().undo(commandContext(runtime));
        const auto restored = clipSnapshot(runtime, legacyAudioClip);
        QVERIFY2((undoEdit && restored && restored->trackId == second &&
                  sameClipTiming(restored->data.properties, before->data.properties)),
                 qPrintable(QStringLiteral("undo must restore legacy audio geometry exactly")));

        testRuntime.history()->reset();
        auto moveProperties = before->data.properties;
        moveProperties.id = legacyAudioClip;
        const auto moved =
            runtime.project().setClipProperties(commandContext(runtime), moveProperties, third);
        const auto movedSnapshot = clipSnapshot(runtime, legacyAudioClip);
        QVERIFY2(
            (moved && movedSnapshot && movedSnapshot->trackId == third &&
             sameClipTiming(movedSnapshot->data.properties, before->data.properties)),
            qPrintable(QStringLiteral("cross-track move must preserve legacy audio geometry")));
        const auto undoMove = runtime.history().undo(commandContext(runtime));
        const auto movedBack = clipSnapshot(runtime, legacyAudioClip);
        QVERIFY2(
            (undoMove && movedBack && movedBack->trackId == second &&
             sameClipTiming(movedBack->data.properties, before->data.properties)),
            qPrintable(QStringLiteral("undoing a cross-track move must preserve legacy geometry")));
    };

    {
        // Automation::OperationIds::clips::set_properties /
        // QStringLiteral("legacy-audio-timing-edit-undo-restores-raw-range")

        testRuntime.history()->reset();
        const auto before = clipSnapshot(runtime, legacyAudioClip);
        QVERIFY2((before.has_value()),
                 qPrintable(QStringLiteral("legacy audio fixture must exist")));
        if (!before)
            return;
        auto edit = before->data.properties;
        edit.id = legacyAudioClip;
        edit.start += 240;
        const auto changed =
            runtime.project().setClipProperties(commandContext(runtime), edit, second);
        const auto undo = runtime.history().undo(commandContext(runtime));
        const auto restored = clipSnapshot(runtime, legacyAudioClip);
        QVERIFY2((changed && undo && restored && restored->trackId == second &&
                  sameClipTiming(restored->data.properties, before->data.properties)),
                 qPrintable(
                     QStringLiteral("undoing an audio timing edit must restore raw legacy ticks")));
    };

    {
        // Automation::OperationIds::clips::set_properties /
        // QStringLiteral("legacy-audio-timing-move-undo-restores-raw-range")

        testRuntime.history()->reset();
        const auto before = clipSnapshot(runtime, legacyAudioClip);
        QVERIFY2((before.has_value()),
                 qPrintable(QStringLiteral("legacy audio fixture must exist")));
        if (!before)
            return;
        auto edit = before->data.properties;
        edit.id = legacyAudioClip;
        edit.start += 480;
        const auto changed =
            runtime.project().setClipProperties(commandContext(runtime), edit, third);
        const auto undo = runtime.history().undo(commandContext(runtime));
        const auto restored = clipSnapshot(runtime, legacyAudioClip);
        QVERIFY2((changed && undo && restored && restored->trackId == second &&
                  sameClipTiming(restored->data.properties, before->data.properties)),
                 qPrintable(
                     QStringLiteral("undoing an audio timing move must restore raw legacy ticks")));
    };
}

void ProjectEditingTests::trackRemovalRestoresChildren() {
    TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();
    const auto first = insertedTrack(runtime, QStringLiteral("First"));
    const auto second = insertedTrack(runtime, QStringLiteral("Second"));
    const auto third = insertedTrack(runtime, QStringLiteral("Third"));
    QVERIFY(first.isValid() && second.isValid() && third.isValid());
    const auto clip = insertedSingingClip(runtime, third, QStringLiteral("Child Clip"));
    QVERIFY(clip.isValid());
    {
        // Automation::OperationIds::tracks::remove / QStringLiteral("duplicates-preview-undo")

        testRuntime.history()->reset();
        const auto duplicate =
            runtime.project().removeTracks(commandContext(runtime), {third, third});
        QVERIFY2(
            (isError(duplicate, AutomationErrorCode::InvalidArgument, QStringLiteral("track_ids"))),
            qPrintable(QStringLiteral("duplicate track IDs must fail atomically")));
        const auto empty = runtime.project().removeTracks(commandContext(runtime), {});
        QVERIFY2((empty && !empty.get().changed),
                 qPrintable(QStringLiteral("empty track removal must be a no-op")));
        const auto base = runtime.documentVersion();
        const auto preview = runtime.project().removeTracks(commandContext(runtime, true), {third});
        QVERIFY2((preview && preview.get().changed && preview.get().validatedOnly &&
                  trackSnapshot(runtime, third).has_value() && runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("track removal preview must preserve track")));
        const auto removed = runtime.project().removeTracks(commandContext(runtime), {third});
        QVERIFY2((removed && !trackSnapshot(runtime, third).has_value() &&
                  removed.get().current.revision == base.revision + 1),
                 qPrintable(QStringLiteral("track removal must commit once")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        QVERIFY2((undo && trackSnapshot(runtime, third).has_value() &&
                  clipSnapshot(runtime, clip).has_value()),
                 qPrintable(QStringLiteral("track removal must restore child clips on undo")));

        auto explicitZeroDraft = trackDraft(QStringLiteral("Explicit Zero"));
        explicitZeroDraft.colorIndex = 0;
        explicitZeroDraft.resolveColorIndex = false;
        const auto explicitZeroInsert =
            runtime.project().insertTrack(commandContext(runtime), 3, explicitZeroDraft);
        const auto explicitZeroId =
            explicitZeroInsert && !explicitZeroInsert.get().affectedObjects.isEmpty()
                ? TrackId(explicitZeroInsert.get().affectedObjects.first().value)
                : TrackId{};
        testRuntime.history()->reset();
        const auto explicitZeroRemove =
            runtime.project().removeTracks(commandContext(runtime), {explicitZeroId});
        const auto explicitZeroUndo = runtime.history().undo(commandContext(runtime));
        const auto explicitZeroRestored = trackSnapshot(runtime, explicitZeroId);
        QVERIFY2(
            (explicitZeroInsert && explicitZeroRemove && explicitZeroUndo && explicitZeroRestored &&
             explicitZeroRestored->data.colorIndex == 0),
            qPrintable(QStringLiteral("track removal undo must preserve an explicit zero color")));
    };
}

void ProjectEditingTests::listNotes() {
    NoteFixture fixture;
    QVERIFY(fixture.firstNoteId.isValid() && fixture.secondNoteId.isValid());
    auto &runtime = fixture.testRuntime.runtime();

    // Automation::OperationIds::notes::list / QStringLiteral("typed-ordered-snapshot")

    const auto notes =
        runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.clipId);
    QVERIFY2((notes && notes.get().size() == 2 && notes.get().first().id == fixture.firstNoteId &&
              notes.get().last().id == fixture.secondNoteId &&
              notes.get().first().data.lyric == QStringLiteral("la") &&
              notes.get().first().data.clientRef.isEmpty()),
             qPrintable(QStringLiteral("note query must return ordered value DTOs")));
    const auto wrong =
        runtime.notes().getNotes(Automation::DocumentId::create(), Automation::ClipId(999999));
    QVERIFY2((!wrong && wrong.getError().code == AutomationErrorCode::DocumentChanged &&
              wrong.getError().operationId == Automation::OperationIds::notes::list),
             qPrintable(QStringLiteral("document validation must precede clip resolution")));
}

void ProjectEditingTests::insertNotes() {
    NoteFixture fixture;
    QVERIFY(fixture.firstNoteId.isValid() && fixture.secondNoteId.isValid());
    auto &runtime = fixture.testRuntime.runtime();

    // Automation::OperationIds::notes::insert /
    // QStringLiteral("empty-invalid-preview-overlap")

    const auto base = runtime.documentVersion();
    const auto empty = runtime.notes().insertNotes(commandContext(runtime), fixture.clipId, {});
    QVERIFY2((empty && !empty.get().changed && runtime.documentVersion() == base),
             qPrintable(QStringLiteral("empty note insert must be a no-op")));
    const auto invalid = runtime.notes().insertNotes(
        commandContext(runtime), fixture.clipId, {noteDraft(1000, 0, 60, QStringLiteral("bad"))});
    const auto overflow = runtime.notes().insertNotes(
        commandContext(runtime), fixture.clipId,
        {noteDraft(std::numeric_limits<int>::max() - 1, 2, 60, QStringLiteral("bad"))});
    QVERIFY2((isError(invalid, AutomationErrorCode::InvalidArgument, QStringLiteral("notes")) &&
              isError(overflow, AutomationErrorCode::InvalidArgument, QStringLiteral("notes"))),
             qPrintable(QStringLiteral("zero-length and overflowing notes must be rejected")));
    const auto candidate = noteDraft(1200, 240, 67, QStringLiteral("so"), QStringLiteral("note-c"));
    const auto preview =
        runtime.notes().insertNotes(commandContext(runtime, true), fixture.clipId, {candidate});
    QVERIFY2((preview && preview.get().changed && preview.get().validatedOnly &&
              preview.get().createdObjects.isEmpty() && runtime.documentVersion() == base),
             qPrintable(QStringLiteral("note insert preview must not allocate")));
    const auto insert =
        runtime.notes().insertNotes(commandContext(runtime), fixture.clipId, {candidate});
    QVERIFY2((insert && insert.get().createdObjects.size() == 1 &&
              insert.get().createdObjects.first().clientRef == QStringLiteral("note-c") &&
              insert.get().createdObjects.first().object.kind == Automation::ObjectKind::Note &&
              insert.get().createdObjects.first().object == insert.get().affectedObjects.first()),
             qPrintable(QStringLiteral("note insert must return client binding")));

    const auto overlapDraft =
        noteDraft(200, 300, 62, QStringLiteral("overlap"), QStringLiteral("note-overlap"));
    const auto overlapPreview =
        runtime.notes().insertNotes(commandContext(runtime, true), fixture.clipId, {overlapDraft});
    const auto overlapBase = runtime.documentVersion();
    const auto overlap =
        runtime.notes().insertNotes(commandContext(runtime), fixture.clipId, {overlapDraft});
    const auto overlappedNotes =
        runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.clipId);
    QVERIFY2((overlapPreview && overlapPreview.get().changed &&
              overlapPreview.get().validatedOnly && overlap &&
              overlap.get().current.revision == overlapBase.revision + 1 && overlappedNotes &&
              overlappedNotes.get().size() == 4),
             qPrintable(QStringLiteral("overlapping note insertion must remain an atomic, "
                                       "addressable edit")));
    const auto undoOverlap = runtime.history().undo(commandContext(runtime));
    const auto afterUndo =
        runtime.notes().getNotes(runtime.documentVersion().documentId, fixture.clipId);
    QVERIFY2((undoOverlap && afterUndo && afterUndo.get().size() == 3),
             qPrintable(QStringLiteral("overlapping insertion must undo once")));
}

void ProjectEditingTests::moveNotes() {
    NoteFixture fixture;
    QVERIFY(fixture.firstNoteId.isValid() && fixture.secondNoteId.isValid());
    auto &runtime = fixture.testRuntime.runtime();

    // Automation::OperationIds::notes::move / QStringLiteral("bounds-duplicates-noop-undo")

    fixture.testRuntime.history()->reset();
    const auto base = runtime.documentVersion();
    const auto duplicate = runtime.notes().moveNotes(
        commandContext(runtime), fixture.clipId, {fixture.firstNoteId, fixture.firstNoteId}, 1, 0);
    QVERIFY2((isError(duplicate, AutomationErrorCode::InvalidArgument, QStringLiteral("note_ids"))),
             qPrintable(QStringLiteral("duplicate note IDs must be rejected")));
    const auto invalidKey = runtime.notes().moveNotes(commandContext(runtime), fixture.clipId,
                                                      {fixture.firstNoteId}, 0, 100);
    QVERIFY2(
        (isError(invalidKey, AutomationErrorCode::InvalidArgument, QStringLiteral("delta_key"))),
        qPrintable(QStringLiteral("out-of-range key must be rejected")));
    const auto overflowingTick =
        runtime.notes().moveNotes(commandContext(runtime), fixture.clipId, {fixture.firstNoteId},
                                  std::numeric_limits<int>::max(), 0);
    const auto overflowingKey =
        runtime.notes().moveNotes(commandContext(runtime), fixture.clipId, {fixture.firstNoteId}, 0,
                                  std::numeric_limits<int>::max());
    QVERIFY2((isError(overflowingTick, AutomationErrorCode::InvalidArgument,
                      QStringLiteral("delta_tick")) &&
              isError(overflowingKey, AutomationErrorCode::InvalidArgument,
                      QStringLiteral("delta_key")) &&
              runtime.documentVersion() == base),
             qPrintable(QStringLiteral("projected note positions and keys must not overflow")));
    const auto noOp = runtime.notes().moveNotes(commandContext(runtime), fixture.clipId,
                                                {fixture.firstNoteId}, 0, 0);
    QVERIFY2((noOp && !noOp.get().changed && runtime.documentVersion() == base),
             qPrintable(QStringLiteral("zero move must be a no-op")));
    const auto preview =
        runtime.notes().moveNotes(commandContext(runtime, true), fixture.clipId,
                                  {fixture.firstNoteId, fixture.secondNoteId}, 20, 1);
    QVERIFY2((preview && preview.get().changed && preview.get().validatedOnly &&
              runtime.documentVersion() == base),
             qPrintable(QStringLiteral("move preview must preserve notes")));
    const auto moved =
        runtime.notes().moveNotes(commandContext(runtime), fixture.clipId,
                                  {fixture.firstNoteId, fixture.secondNoteId}, 20, 1);
    const auto first = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    const auto second = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
    QVERIFY2((moved && moved.get().current.revision == base.revision + 1 && first && second &&
              first->data.localStart == 93 && second->data.localStart == 620 &&
              first->data.keyIndex == 61 && second->data.keyIndex == 65),
             qPrintable(QStringLiteral("multi-note move must be one atomic revision")));
    const auto undo = runtime.history().undo(commandContext(runtime));
    const auto restored = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    QVERIFY2((undo && restored && restored->data.localStart == 73 && restored->data.keyIndex == 60),
             qPrintable(QStringLiteral("multi-note move must undo atomically")));

    const auto overlapPreview = runtime.notes().moveNotes(
        commandContext(runtime, true), fixture.clipId, {fixture.secondNoteId}, -300, 0);
    const auto overlapBase = runtime.documentVersion();
    const auto overlap = runtime.notes().moveNotes(commandContext(runtime), fixture.clipId,
                                                   {fixture.secondNoteId}, -300, 0);
    const auto overlapped = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
    QVERIFY2((overlapPreview && overlapPreview.get().changed && overlap &&
              overlap.get().current.revision == overlapBase.revision + 1 && overlapped &&
              overlapped->data.localStart == 300),
             qPrintable(QStringLiteral("move into overlap must remain an atomic edit")));
    const auto undoOverlap = runtime.history().undo(commandContext(runtime));
    const auto afterUndo = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
    QVERIFY2((undoOverlap && afterUndo && afterUndo->data.localStart == 600),
             qPrintable(QStringLiteral("overlapping move must undo once")));
}

void ProjectEditingTests::resizeNotesLeft() {
    NoteFixture fixture;
    QVERIFY(fixture.firstNoteId.isValid() && fixture.secondNoteId.isValid());
    auto &runtime = fixture.testRuntime.runtime();

    // Automation::OperationIds::notes::resize_left / QStringLiteral("clamp-preview-commit")

    fixture.testRuntime.history()->reset();
    const auto invalid = runtime.notes().resizeNotesLeft(commandContext(runtime), fixture.clipId,
                                                         {fixture.firstNoteId}, 10, 0);
    QVERIFY2((isError(invalid, AutomationErrorCode::InvalidArgument, QStringLiteral("resize"))),
             qPrintable(QStringLiteral("non-positive minimum length must be rejected")));
    const auto duplicate = runtime.notes().resizeNotesLeft(
        commandContext(runtime), fixture.clipId, {fixture.firstNoteId, fixture.firstNoteId}, 10, 1);
    QVERIFY2((isError(duplicate, AutomationErrorCode::InvalidArgument, QStringLiteral("resize"))),
             qPrintable(QStringLiteral("duplicate resize IDs must be rejected")));
    const auto base = runtime.documentVersion();
    const auto preview = runtime.notes().resizeNotesLeft(
        commandContext(runtime, true), fixture.clipId, {fixture.firstNoteId}, 20, 120);
    QVERIFY2((preview && preview.get().changed && runtime.documentVersion() == base),
             qPrintable(QStringLiteral("left-resize preview must not mutate")));
    const auto changed = runtime.notes().resizeNotesLeft(commandContext(runtime), fixture.clipId,
                                                         {fixture.firstNoteId}, 20, 120);
    const auto note = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && note &&
              note->data.localStart == 93 && note->data.length == 387),
             qPrintable(QStringLiteral("left resize must update start and length once")));
    const auto noOp =
        runtime.notes().resizeNotesLeft(commandContext(runtime), fixture.clipId, {}, 20, 120);
    QVERIFY2((noOp && !noOp.get().changed),
             qPrintable(QStringLiteral("empty left resize must be a no-op")));
}

void ProjectEditingTests::resizeNotesRight() {
    NoteFixture fixture;
    QVERIFY(fixture.firstNoteId.isValid() && fixture.secondNoteId.isValid());
    auto &runtime = fixture.testRuntime.runtime();

    // Automation::OperationIds::notes::resize_right /
    // QStringLiteral("clamp-preview-commit")

    fixture.testRuntime.history()->reset();
    const auto invalid = runtime.notes().resizeNotesRight(commandContext(runtime), fixture.clipId,
                                                          {fixture.firstNoteId}, -10, 0);
    QVERIFY2((isError(invalid, AutomationErrorCode::InvalidArgument, QStringLiteral("resize"))),
             qPrintable(QStringLiteral("non-positive minimum length must be rejected")));
    const auto base = runtime.documentVersion();
    const auto overflowing =
        runtime.notes().resizeNotesRight(commandContext(runtime), fixture.clipId,
                                         {fixture.firstNoteId}, std::numeric_limits<int>::max(), 1);
    QVERIFY2(
        (isError(overflowing, AutomationErrorCode::InvalidArgument, QStringLiteral("delta_tick")) &&
         runtime.documentVersion() == base),
        qPrintable(QStringLiteral("projected note length must not overflow")));
    const auto preview = runtime.notes().resizeNotesRight(
        commandContext(runtime, true), fixture.clipId, {fixture.firstNoteId}, 60, 120);
    QVERIFY2((preview && preview.get().changed && runtime.documentVersion() == base),
             qPrintable(QStringLiteral("right-resize preview must not mutate")));
    const auto changed = runtime.notes().resizeNotesRight(commandContext(runtime), fixture.clipId,
                                                          {fixture.firstNoteId}, 60, 120);
    const auto note = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && note &&
              note->data.length == 467),
             qPrintable(QStringLiteral("right resize must change length once")));
    const auto noOp =
        runtime.notes().resizeNotesRight(commandContext(runtime), fixture.clipId, {}, 60, 120);
    QVERIFY2((noOp && !noOp.get().changed),
             qPrintable(QStringLiteral("empty right resize must be a no-op")));
}

void ProjectEditingTests::splitNote() {
    NoteFixture fixture;
    QVERIFY(fixture.firstNoteId.isValid() && fixture.secondNoteId.isValid());
    auto &runtime = fixture.testRuntime.runtime();

    // Automation::OperationIds::notes::split /
    // QStringLiteral("invalid-preview-create-undo")

    fixture.testRuntime.history()->reset();
    const auto before = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
    const auto child = noteDraft(before->data.localStart + 180, 180, before->data.keyIndex,
                                 QStringLiteral("+"), QStringLiteral("split-child"));
    const auto invalid = runtime.notes().splitNote(
        commandContext(runtime), fixture.clipId, fixture.secondNoteId, child, before->data.length);
    QVERIFY2((isError(invalid, AutomationErrorCode::InvalidArgument, QStringLiteral("split"))),
             qPrintable(QStringLiteral("split length must remain inside original")));
    const auto base = runtime.documentVersion();
    const auto preview = runtime.notes().splitNote(commandContext(runtime, true), fixture.clipId,
                                                   fixture.secondNoteId, child, 180);
    QVERIFY2((preview && preview.get().validatedOnly && preview.get().changed &&
              preview.get().createdObjects.isEmpty() && runtime.documentVersion() == base),
             qPrintable(QStringLiteral("split preview must not allocate child ID")));
    const auto split = runtime.notes().splitNote(commandContext(runtime), fixture.clipId,
                                                 fixture.secondNoteId, child, 180);
    const auto original = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
    QVERIFY2((split && split.get().createdObjects.size() == 1 && original &&
              original->data.length == 180 &&
              split.get().createdObjects.first().clientRef == QStringLiteral("split-child") &&
              split.get().createdObjects.first().object.kind == Automation::ObjectKind::Note &&
              split.get().createdObjects.first().object == split.get().affectedObjects.last()),
             qPrintable(QStringLiteral("split must shorten original and bind child")));
    const auto childId = split ? NoteId(split.get().createdObjects.first().object.value) : NoteId();
    QVERIFY2((childId.isValid() && noteSnapshot(runtime, fixture.clipId, childId).has_value()),
             qPrintable(QStringLiteral("split child must be queryable")));
    const auto undo = runtime.history().undo(commandContext(runtime));
    const auto restored = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
    QVERIFY2((undo && restored && restored->data.length == before->data.length &&
              !noteSnapshot(runtime, fixture.clipId, childId).has_value()),
             qPrintable(QStringLiteral("split must undo as one History entry")));
}

void ProjectEditingTests::setPhonemeOffsets() {
    NoteFixture fixture;
    QVERIFY(fixture.firstNoteId.isValid() && fixture.secondNoteId.isValid());
    auto &runtime = fixture.testRuntime.runtime();

    // Automation::OperationIds::notes::set_phoneme_offsets /
    // QStringLiteral("shape-order-preview-noop")

    fixture.testRuntime.history()->reset();
    const auto base = runtime.documentVersion();
    const auto badCount = runtime.notes().setPhonemeOffsets(
        commandContext(runtime, true), fixture.clipId, fixture.firstNoteId, {0});
    QVERIFY2((isError(badCount, AutomationErrorCode::InvalidArgument)),
             qPrintable(QStringLiteral("offset count must match effective phoneme count")));
    const auto unordered = runtime.notes().setPhonemeOffsets(
        commandContext(runtime, true), fixture.clipId, fixture.firstNoteId, {120, -20});
    QVERIFY2((isError(unordered, AutomationErrorCode::InvalidArgument)),
             qPrintable(QStringLiteral("phoneme offsets must be monotonic")));
    const auto preview = runtime.notes().setPhonemeOffsets(
        commandContext(runtime, true), fixture.clipId, fixture.firstNoteId, {-20, 120});
    QVERIFY2((preview && preview.get().changed && preview.get().validatedOnly &&
              runtime.documentVersion() == base),
             qPrintable(QStringLiteral("valid offsets preview must not mutate")));
    const auto changed = runtime.notes().setPhonemeOffsets(commandContext(runtime), fixture.clipId,
                                                           fixture.firstNoteId, {-20, 120});
    const auto note = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && note &&
              note->data.phonemes.offsetSeq.edited == QList<int>({-20, 120})),
             qPrintable(QStringLiteral("valid edited offsets must round-trip")));
    const auto noOp = runtime.notes().setPhonemeOffsets(commandContext(runtime), fixture.clipId,
                                                        fixture.firstNoteId, {-20, 120});
    QVERIFY2((noOp && !noOp.get().changed),
             qPrintable(QStringLiteral("identical offsets must be a no-op")));
    const auto clear = runtime.notes().setPhonemeOffsets(commandContext(runtime), fixture.clipId,
                                                         fixture.firstNoteId, {});
    const auto cleared = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    QVERIFY2((clear && clear.get().changed && cleared &&
              cleared->data.phonemes.offsetSeq.edited.isEmpty()),
             qPrintable(QStringLiteral("empty offsets must explicitly clear the edit")));
}

void ProjectEditingTests::resetPhonemeOffsetsCascades() {
    NoteFixture fixture;
    QVERIFY(fixture.firstNoteId.isValid() && fixture.secondNoteId.isValid());
    auto &runtime = fixture.testRuntime.runtime();

    // Automation::OperationIds::notes::reset_phoneme_offsets /
    // QStringLiteral("cascade-preview-commit-undo")

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
    const auto firstSeed = runtime.notes().setPhonemes(commandContext(runtime), fixture.clipId,
                                                       fixture.firstNoteId, firstPhonemes);
    const auto secondSeed = runtime.notes().setPhonemes(commandContext(runtime), fixture.clipId,
                                                        fixture.secondNoteId, secondPhonemes);
    QVERIFY2((firstSeed && secondSeed),
             qPrintable(QStringLiteral("cascade fixture must seed both edited words")));
    fixture.testRuntime.history()->reset();

    const auto base = runtime.documentVersion();
    const auto preview = runtime.notes().resetPhonemeOffsets(commandContext(runtime, true),
                                                             fixture.clipId, {fixture.firstNoteId});
    const auto previewFirst = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    const auto previewSecond = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
    QVERIFY2((preview && preview.get().changed && preview.get().validatedOnly &&
              preview.get().affectedObjects.size() == 2 && previewFirst && previewSecond &&
              previewFirst->data.phonemes.offsetSeq.edited == firstPhonemes.offsetSeq.edited &&
              previewSecond->data.phonemes.offsetSeq.edited == secondPhonemes.offsetSeq.edited &&
              runtime.documentVersion() == base),
             qPrintable(QStringLiteral("cascade preview must report both roots without mutation")));

    const auto changed = runtime.notes().resetPhonemeOffsets(commandContext(runtime),
                                                             fixture.clipId, {fixture.firstNoteId});
    const auto resetFirst = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    const auto resetSecond = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
    QVERIFY2((changed && changed.get().current.revision == base.revision + 1 &&
              changed.get().affectedObjects.size() == 2 && resetFirst && resetSecond &&
              resetFirst->data.phonemes.offsetSeq.edited.isEmpty() &&
              resetSecond->data.phonemes.offsetSeq.edited.isEmpty()),
             qPrintable(QStringLiteral("cascade reset must commit both words atomically")));

    const auto undo = runtime.history().undo(commandContext(runtime));
    const auto restoredFirst = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    const auto restoredSecond = noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId);
    QVERIFY2((undo && restoredFirst && restoredSecond &&
              restoredFirst->data.phonemes.offsetSeq.edited == firstPhonemes.offsetSeq.edited &&
              restoredSecond->data.phonemes.offsetSeq.edited == secondPhonemes.offsetSeq.edited),
             qPrintable(QStringLiteral("cascade reset must restore every word in one undo")));
}

void ProjectEditingTests::setWordProperties() {
    NoteFixture fixture;
    QVERIFY(fixture.firstNoteId.isValid() && fixture.secondNoteId.isValid());
    auto &runtime = fixture.testRuntime.runtime();

    // Automation::OperationIds::notes::set_word_properties /
    // QStringLiteral("unicode-cascade-atomic-noop")

    fixture.testRuntime.history()->reset();
    const auto duplicate = runtime.notes().setWordProperties(
        commandContext(runtime), fixture.clipId,
        {{.noteId = fixture.firstNoteId}, {.noteId = fixture.firstNoteId}});
    QVERIFY2((isError(duplicate, AutomationErrorCode::InvalidArgument, QStringLiteral("edits"))),
             qPrintable(QStringLiteral("duplicate word edits must be rejected")));
    Automation::NoteWordEditDto edit;
    edit.noteId = fixture.firstNoteId;
    edit.lyric = QStringLiteral("  你好  ");
    edit.language = QStringLiteral("zh");
    const auto base = runtime.documentVersion();
    const auto preview =
        runtime.notes().setWordProperties(commandContext(runtime, true), fixture.clipId, {edit});
    QVERIFY2((preview && preview.get().changed && preview.get().validatedOnly &&
              runtime.documentVersion() == base),
             qPrintable(QStringLiteral("word preview must preserve original")));
    const auto changed =
        runtime.notes().setWordProperties(commandContext(runtime), fixture.clipId, {edit});
    const auto note = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    QVERIFY2((changed && note && note->data.lyric == QStringLiteral("你好") &&
              note->data.language == QStringLiteral("zh") &&
              note->data.pronunciation.edited.isEmpty() &&
              note->data.pronunciationCandidates.isEmpty() &&
              note->data.phonemes.nameSeq.result().isEmpty()),
             qPrintable(QStringLiteral("word input change must trim Unicode and cascade reset")));
    Automation::NoteWordEditDto identical;
    identical.noteId = fixture.firstNoteId;
    identical.lyric = note->data.lyric;
    identical.language = note->data.language;
    identical.pronunciation = note->data.pronunciation;
    identical.pronunciationCandidates = note->data.pronunciationCandidates;
    identical.phonemes = note->data.phonemes;
    identical.replacePronunciation = true;
    identical.replacePronunciationCandidates = true;
    const auto noOp =
        runtime.notes().setWordProperties(commandContext(runtime), fixture.clipId, {identical});
    QVERIFY2((noOp && !noOp.get().changed),
             qPrintable(QStringLiteral("identical word properties must be a no-op")));
}

void ProjectEditingTests::removeNotes() {
    NoteFixture fixture;
    QVERIFY(fixture.firstNoteId.isValid() && fixture.secondNoteId.isValid());
    auto &runtime = fixture.testRuntime.runtime();

    // Automation::OperationIds::notes::remove / QStringLiteral("duplicates-preview-undo")

    fixture.testRuntime.history()->reset();
    const auto duplicate = runtime.notes().removeNotes(commandContext(runtime), fixture.clipId,
                                                       {fixture.firstNoteId, fixture.firstNoteId});
    QVERIFY2((isError(duplicate, AutomationErrorCode::InvalidArgument, QStringLiteral("note_ids"))),
             qPrintable(QStringLiteral("duplicate removal IDs must be rejected")));
    const auto empty = runtime.notes().removeNotes(commandContext(runtime), fixture.clipId, {});
    QVERIFY2((empty && !empty.get().changed),
             qPrintable(QStringLiteral("empty note removal must be a no-op")));
    const auto base = runtime.documentVersion();
    const auto preview = runtime.notes().removeNotes(commandContext(runtime, true), fixture.clipId,
                                                     {fixture.secondNoteId});
    QVERIFY2((preview && preview.get().changed && preview.get().validatedOnly &&
              noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId).has_value() &&
              runtime.documentVersion() == base),
             qPrintable(QStringLiteral("note removal preview must preserve note")));
    const auto removed = runtime.notes().removeNotes(commandContext(runtime), fixture.clipId,
                                                     {fixture.secondNoteId});
    QVERIFY2((removed && !noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId).has_value()),
             qPrintable(QStringLiteral("note removal must delete requested note")));
    const auto undo = runtime.history().undo(commandContext(runtime));
    QVERIFY2((undo && noteSnapshot(runtime, fixture.clipId, fixture.secondNoteId).has_value()),
             qPrintable(QStringLiteral("note removal must be reversible")));
}

void ProjectEditingTests::curveTransforms_data() {
    QTest::addColumn<int>("transformKind");
    QTest::newRow("shape") << int(CurveTransform::Kind::Shape);
    QTest::newRow("scale") << int(CurveTransform::Kind::Scale);
    QTest::newRow("modulate-pitch") << int(CurveTransform::Kind::ModulatePitch);
}

void ProjectEditingTests::curveTransforms() {
    QFETCH(int, transformKind);
    using CurveTransform::Kind;
    const auto kind = static_cast<Kind>(transformKind);
    const MouthOpeningParamProperties properties;

    const auto name = kind == Kind::ModulatePitch ? ParamInfo::Pitch : ParamInfo::MouthOpening;
    Automation::ParameterRuntimeServices services;
    services.prepareTransform =
        [&properties](SingingClip *, ParamInfo::Name,
                      const Kind kind) -> AutomationResult<CurveTransform::Config> {
        CurveTransform::Config config;
        config.kind = kind;
        config.properties = &properties;
        config.tickToMilliseconds = [](const int tick) { return double(tick); };
        config.pitchBaselineAtTick = [](int) { return std::optional<double>(6000.0); };
        return config;
    };
    TestRuntime testRuntime({}, {}, {}, {}, {}, {}, {}, {}, {}, std::move(services));
    auto &runtime = testRuntime.runtime();
    const auto trackId = insertedTrack(runtime, QStringLiteral("Curves"));
    const auto clipId = insertedSingingClip(runtime, trackId, QStringLiteral("Curve Clip"));
    const auto &operation = kind == Kind::ModulatePitch
                                ? Automation::OperationIds::parameters::modulate
                            : kind == Kind::Shape ? Automation::OperationIds::parameters::shape
                                                  : Automation::OperationIds::parameters::scale;
    {
        // operation / QStringLiteral("clamped-range-result-and-undo")

        Automation::CurveDraftDto first;
        first.type = Automation::CurveDraftDto::Type::Draw;
        first.localStart = 20;
        first.values =
            kind == Kind::ModulatePitch ? QList<int>{6100, 6500, 6200} : QList<int>{100, 900, 500};
        auto second = first;
        second.localStart = 60;
        runtime.parameters().replaceParameter(commandContext(runtime), clipId, name, Param::Edited,
                                              {first, second});
        testRuntime.history()->reset();
        const Automation::ParameterTransformDto request{1, 99, kind == Kind::Scale ? 0.5 : 0.0, 1,
                                                        99};
        const auto applied = runtime.parameters().transformParameter(commandContext(runtime),
                                                                     clipId, name, kind, request);
        const auto snapshot = [&] {
            return runtime.parameters()
                .getParameter(runtime.documentVersion().documentId, clipId, name, Param::Edited)
                .get()
                .curves;
        };
        const auto expected = kind == Kind::ModulatePitch ? QList<int>{6000, 6000, 6000}
                              : kind == Kind::Shape       ? QList<int>{100, 300, 500}
                                                          : QList<int>{50, 450, 250};
        const QList<Automation::ResolvedValue> bounds{
            {QStringLiteral("/local_start"),      20},
            {QStringLiteral("/local_end"),        35},
            {QStringLiteral("/transition_start"), 20},
            {QStringLiteral("/transition_end"),   35},
        };
        QVERIFY2((applied && applied.get().changed && applied.get().resolvedValues == bounds &&
                  snapshot().first().values == expected &&
                  snapshot().last().values == second.values),
                 qPrintable(QStringLiteral(
                     "transform must report its clamped range and preserve the next segment")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        QVERIFY2((undo && snapshot().first().values == first.values),
                 qPrintable(QStringLiteral("one undo must restore the transform source")));
        const auto redo = runtime.history().redo(commandContext(runtime));
        QVERIFY2((redo && snapshot().first().values == expected),
                 qPrintable(QStringLiteral("one redo must restore the transform result")));
        auto unchanged = request;
        unchanged.factor = 1.0;
        const auto noOp = runtime.parameters().transformParameter(commandContext(runtime), clipId,
                                                                  name, kind, unchanged);
        QVERIFY2((noOp && !noOp.get().changed && noOp.get().resolvedValues == bounds),
                 qPrintable(QStringLiteral("a no-op must still report the actual selected range")));
    };
}

void ProjectEditingTests::parameterEditing() {
    TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();
    const auto trackId = insertedTrack(runtime, QStringLiteral("Voice"));
    const auto clipId = insertedSingingClip(runtime, trackId, QStringLiteral("Voice Clip"));
    testRuntime.history()->reset();

    {
        // Automation::OperationIds::parameters::get /
        // QStringLiteral("empty-invalid-wrong-type")

        const auto empty = runtime.parameters().getParameter(
            runtime.documentVersion().documentId, clipId, ParamInfo::Pitch, Param::Edited);
        QVERIFY2((empty && empty.get().curves.isEmpty() &&
                  empty.get().document == runtime.documentVersion()),
                 qPrintable(QStringLiteral("empty parameter must be a valid snapshot")));
        const auto unsupported = runtime.parameters().getParameter(
            runtime.documentVersion().documentId, clipId, ParamInfo::Unknown, Param::Edited);
        QVERIFY2((!unsupported &&
                  unsupported.getError().code == AutomationErrorCode::InvalidArgument &&
                  unsupported.getError().fieldPath == QStringLiteral("parameter")),
                 qPrintable(QStringLiteral("unsupported parameter query must fail")));
    };

    {
        // Automation::OperationIds::parameters::replace /
        // QStringLiteral("validate-roundtrip-noop-undo")

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
            commandContext(runtime, true), clipId, ParamInfo::Pitch, Param::Edited, {draw, anchor});
        QVERIFY2((preview && preview.get().changed && preview.get().validatedOnly &&
                  runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("parameter preview must not mutate")));
        const auto changed = runtime.parameters().replaceParameter(
            commandContext(runtime), clipId, ParamInfo::Pitch, Param::Edited, {draw, anchor});
        const auto snapshot = runtime.parameters().getParameter(
            runtime.documentVersion().documentId, clipId, ParamInfo::Pitch, Param::Edited);
        QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && snapshot &&
                  snapshot.get().curves.size() == 2 &&
                  snapshot.get().curves.at(0).values == draw.values &&
                  snapshot.get().curves.at(1).nodes.size() == 2),
                 qPrintable(QStringLiteral("draw and anchor curves must round-trip atomically")));
        const auto noOp = runtime.parameters().replaceParameter(
            commandContext(runtime), clipId, ParamInfo::Pitch, Param::Edited, {draw, anchor});
        QVERIFY2((noOp && !noOp.get().changed),
                 qPrintable(QStringLiteral("identical parameter replacement must be a no-op")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        const auto restored = runtime.parameters().getParameter(
            runtime.documentVersion().documentId, clipId, ParamInfo::Pitch, Param::Edited);
        QVERIFY2((undo && restored && restored.get().curves.isEmpty()),
                 qPrintable(QStringLiteral("parameter replacement must undo once")));

        auto invalid = draw;
        invalid.step = 0;
        const auto invalidStep = runtime.parameters().replaceParameter(
            commandContext(runtime, true), clipId, ParamInfo::Pitch, Param::Edited, {invalid});
        auto overflow = draw;
        overflow.localStart = std::numeric_limits<int>::max() - 1;
        overflow.step = 2;
        overflow.values = {6000, 6010};
        const auto invalidRange = runtime.parameters().replaceParameter(
            commandContext(runtime, true), clipId, ParamInfo::Pitch, Param::Edited, {overflow});
        auto invalidAnchor = anchor;
        invalidAnchor.nodes = {
            {0, 10, AnchorNode::Linear}
        };
        const auto invalidTopology =
            runtime.parameters().replaceParameter(commandContext(runtime, true), clipId,
                                                  ParamInfo::Pitch, Param::Edited, {invalidAnchor});
        QVERIFY2((isError(invalidStep, AutomationErrorCode::InvalidArgument) &&
                  isError(invalidRange, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("curves.values")) &&
                  isError(invalidTopology, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("curves.nodes"))),
                 qPrintable(QStringLiteral("draw geometry and anchor topology must be valid")));
    };

    {
        // Automation::OperationIds::parameters::draw / QStringLiteral("overflow-rejected")

        const auto base = runtime.documentVersion();
        const auto overflow = runtime.parameters().drawParameter(
            commandContext(runtime), clipId, ParamInfo::Pitch, Param::Edited,
            std::numeric_limits<int>::max() - 1, 2, {6000, 6010}, false);
        QVERIFY2(
            (isError(overflow, AutomationErrorCode::InvalidArgument, QStringLiteral("values")) &&
             runtime.documentVersion() == base),
            qPrintable(QStringLiteral("draw range overflow must fail without mutation")));
    };

    {
        // Automation::OperationIds::parameters::trace /
        // QStringLiteral("preserve-gaps-and-anchors")

        const auto created = runtime.parameters().createAnchorCurve(
            commandContext(runtime), clipId, ParamInfo::Pitch, Param::Edited,
            QStringLiteral("huge-anchor"),
            {
                {0,                               6000, AnchorNode::Linear},
                {std::numeric_limits<int>::max(), 6000, AnchorNode::Linear},
        });
        QVERIFY2((created && created.get().changed),
                 qPrintable(QStringLiteral("the trace bound fixture must create its anchor")));
        const auto base = runtime.documentVersion();
        const auto traced = runtime.parameters().traceParameter(commandContext(runtime), clipId,
                                                                ParamInfo::Pitch, 0, 5);
        QVERIFY2((traced && !traced.get().changed && runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("trace without original data must preserve anchors")));

        auto before = runtime.parameters()
                          .getParameter(base.documentId, clipId, ParamInfo::Pitch, Param::Edited)
                          .get()
                          .curves;
        Automation::CurveDraftDto draw;
        draw.type = Automation::CurveDraftDto::Type::Draw;
        draw.localStart = 0;
        draw.step = 5;
        draw.values = QList<int>(8, 6000);
        before.prepend(draw);
        runtime.parameters().replaceParameter(commandContext(runtime), clipId, ParamInfo::Pitch,
                                              Param::Edited, before);
        auto first = draw;
        first.values = {6100, 6110};
        auto second = first;
        second.localStart = 20;
        second.values = {6200, 6210};
        runtime.parameters().replaceParameter(commandContext(runtime), clipId, ParamInfo::Pitch,
                                              Param::Original, {first, second});
        const auto beforeTrace = runtime.documentVersion();
        const auto applied = runtime.parameters().traceParameter(commandContext(runtime), clipId,
                                                                 ParamInfo::Pitch, 0, 30);
        const auto snapshot = [&] {
            return runtime.parameters()
                .getParameter(runtime.documentVersion().documentId, clipId, ParamInfo::Pitch,
                              Param::Edited)
                .get()
                .curves;
        };
        const auto after = snapshot();
        QVERIFY2(
            (applied && applied.get().changed &&
             applied.get().current.revision == beforeTrace.revision + 1 && after.size() == 2 &&
             after.first().values == QList<int>{6100, 6110, 6000, 6000, 6200, 6210, 6000, 6000} &&
             after.last().id == before.last().id && after.last().nodes.size() == 2 &&
             after.last().nodes.last().position == std::numeric_limits<int>::max()),
            qPrintable(QStringLiteral("trace must preserve holes, outside samples and anchors")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        QVERIFY2((undo && snapshot().first().values == draw.values),
                 qPrintable(QStringLiteral("one undo must restore the complete trace edit")));
        const auto redo = runtime.history().redo(commandContext(runtime));
        QVERIFY2((redo && snapshot().first().values == after.first().values &&
                  snapshot().last().id == after.last().id),
                 qPrintable(QStringLiteral("one redo must restore the traced curves")));
        const auto noOp = runtime.parameters().traceParameter(commandContext(runtime), clipId,
                                                              ParamInfo::Pitch, 0, 30);
        QVERIFY2((noOp && !noOp.get().changed),
                 qPrintable(QStringLiteral("repeating trace must not add an undo step")));
    };
}

void ProjectEditingTests::drawAndErasePreserveOtherParameterCurves_data() {
    QTest::addColumn<bool>("overlay");
    QTest::newRow("overlay-existing-samples") << true;
    QTest::newRow("replace-a-range") << false;
}

void ProjectEditingTests::drawAndErasePreserveOtherParameterCurves() {
    QFETCH(bool, overlay);
    TestRuntime fixture;
    auto &runtime = fixture.runtime();
    auto &parameters = runtime.parameters();
    const auto clip = insertedSingingClip(runtime, insertedTrack(runtime, "Curves"), "Pitch");
    Automation::CurveDraftDto draw;
    draw.type = Automation::CurveDraftDto::Type::Draw;
    draw.step = 5;
    draw.values = {6000, 6010, 6020, 6030, 6040, 6050, 6060, 6070};
    Automation::CurveDraftDto anchor;
    anchor.type = Automation::CurveDraftDto::Type::Anchor;
    anchor.nodes = {
        {100, 6000, AnchorNode::Linear },
        {200, 6100, AnchorNode::Hermite}
    };
    QVERIFY(parameters.replaceParameter(commandContext(runtime), clip, ParamInfo::Pitch,
                                        Param::Edited, {draw, anchor}));
    const auto curves = [&] {
        return parameters
            .getParameter(runtime.documentVersion().documentId, clip, ParamInfo::Pitch,
                          Param::Edited)
            .get()
            .curves;
    };
    const auto draws = [&] {
        auto result = curves();
        result.removeIf(
            [](const auto &curve) { return curve.type != Automation::CurveDraftDto::Type::Draw; });
        return result;
    };
    const auto original = curves();
    const auto originalAnchor = original.last();
    fixture.history()->reset();
    const auto before = runtime.documentVersion();
    const auto preview =
        parameters.drawParameter(commandContext(runtime, true), clip, ParamInfo::Pitch,
                                 Param::Edited, 10, 5, {6200, 6210, 6220, 6230}, overlay);
    QVERIFY(preview && preview.get().changed && preview.get().validatedOnly);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(draws().first().values, draw.values);
    QVERIFY(!fixture.history()->canUndo());
    const auto applied =
        parameters.drawParameter(commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited, 10,
                                 5, {6200, 6210, 6220, 6230}, overlay);
    QVERIFY(applied && applied.get().changed);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    const auto drawn = draws();
    if (overlay) {
        QCOMPARE(drawn.size(), 1);
        QCOMPARE(drawn.first().localStart, 0);
        QCOMPARE(drawn.first().values,
                 (QList<int>{6000, 6010, 6200, 6210, 6220, 6230, 6060, 6070}));
    } else {
        QCOMPARE(drawn.size(), 3);
        QCOMPARE(drawn.at(0).values, (QList<int>{6000, 6010}));
        QCOMPARE(drawn.at(1).localStart, 10);
        QCOMPARE(drawn.at(1).values, (QList<int>{6200, 6210, 6220, 6230}));
        QCOMPARE(drawn.at(2).localStart, 30);
        QCOMPARE(drawn.at(2).values, (QList<int>{6060, 6070}));
    }
    const auto beforeErase = runtime.documentVersion();
    const auto erasePreview = parameters.eraseParameter(commandContext(runtime, true), clip,
                                                        ParamInfo::Pitch, Param::Edited, 10, 30);
    QVERIFY(erasePreview && erasePreview.get().changed && erasePreview.get().validatedOnly);
    QCOMPARE(runtime.documentVersion(), beforeErase);
    QCOMPARE(draws().size(), drawn.size());
    const auto erased = parameters.eraseParameter(commandContext(runtime), clip, ParamInfo::Pitch,
                                                  Param::Edited, 10, 30);
    QVERIFY(erased && erased.get().changed);
    QCOMPARE(runtime.documentVersion().revision, beforeErase.revision + 1);
    const auto remaining = draws();
    QCOMPARE(remaining.size(), 2);
    QCOMPARE(remaining.first().localStart, 0);
    QCOMPARE(remaining.first().values, (QList<int>{6000, 6010}));
    QCOMPARE(remaining.last().localStart, 30);
    QCOMPARE(remaining.last().values, (QList<int>{6060, 6070}));
    const auto afterErase = curves();
    const auto preserved = std::find_if(afterErase.cbegin(), afterErase.cend(),
                                        [&](const auto &c) { return c.id == originalAnchor.id; });
    QVERIFY(preserved != afterErase.cend());
    QCOMPARE(preserved->nodes.size(), originalAnchor.nodes.size());
    QCOMPARE(preserved->nodes.first().id, originalAnchor.nodes.first().id);
    QCOMPARE(preserved->nodes.last().value, originalAnchor.nodes.last().value);
    const auto *undoEntry = fixture.history()->nextUndoEntry();
    const auto emptyErase = parameters.eraseParameter(commandContext(runtime), clip,
                                                      ParamInfo::Pitch, Param::Edited, 50, 80);
    QVERIFY(emptyErase && !emptyErase.get().changed);
    QCOMPARE(fixture.history()->nextUndoEntry(), undoEntry);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(draws().size(), drawn.size());
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(draws().first().values, draw.values);
    QVERIFY(!fixture.history()->canUndo());
    QVERIFY(runtime.history().redo(commandContext(runtime)));
    QVERIFY(runtime.history().redo(commandContext(runtime)));
    QCOMPARE(draws().last().values, remaining.last().values);
}

void ProjectEditingTests::nonAdjacentAnchorMergePreservesDocument() {
    TestRuntime fixture;
    auto &runtime = fixture.runtime();
    auto &parameters = runtime.parameters();
    const auto clip = insertedSingingClip(runtime, insertedTrack(runtime, "Curves"), "Pitch");
    for (int start : {0, 480, 960}) {
        QVERIFY(parameters.createAnchorCurve(
            commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited,
            QStringLiteral("segment-%1").arg(start),
            {
                {start,       6000, AnchorNode::Linear },
                {start + 240, 6200, AnchorNode::Hermite}
        }));
    }
    const auto curves = parameters.getParameter(runtime.documentVersion().documentId, clip,
                                                ParamInfo::Pitch, Param::Edited);
    QVERIFY(curves);
    QCOMPARE(curves.get().curves.size(), 3);
    fixture.history()->reset();
    const auto before = runtime.documentVersion();
    const auto model = fixture.model().serialize();
    const auto result =
        parameters.mergeAnchorCurves(commandContext(runtime), clip, ParamInfo::Pitch, Param::Edited,
                                     curves.get().curves.first().id, curves.get().curves.last().id);
    QVERIFY(!result);
    QCOMPARE(result.getError().code, AutomationErrorCode::InvalidArgument);
    QCOMPARE(result.getError().fieldPath, QStringLiteral("source_curve_id"));
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(fixture.model().serialize(), model);
    QVERIFY(!fixture.history()->canUndo());
}

void ProjectEditingTests::speakerMixEditing() {
    TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();
    const auto trackId = insertedTrack(runtime, QStringLiteral("Voice"));
    const auto clipId = insertedSingingClip(runtime, trackId, QStringLiteral("Voice Clip"));
    testRuntime.history()->reset();

    const auto speakerA = speaker(QStringLiteral("speaker-a"));
    const auto speakerB = speaker(QStringLiteral("speaker-b"));
    const auto singerA = singer(QStringLiteral("singer-a"), {speakerA, speakerB});
    const auto fixed = fixedMix(speakerA, speakerB);
    const auto dynamic = dynamicMix(speakerA, speakerB);

    {
        // Automation::OperationIds::tracks::set_voice / QStringLiteral("preview-commit-noop")

        testRuntime.history()->reset();
        const auto base = runtime.documentVersion();
        const auto preview = runtime.parameters().selectTrackSingleSpeaker(
            commandContext(runtime, true), trackId, singerA, speakerA);
        QVERIFY2((preview && preview.get().changed && runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("track speaker preview must not mutate")));
        const auto changed = runtime.parameters().selectTrackSingleSpeaker(
            commandContext(runtime), trackId, singerA, speakerA);
        const auto snapshot = trackSnapshot(runtime, trackId);
        QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && snapshot &&
                  snapshot->data.singerInfo == singerA && snapshot->data.speakerInfo == speakerA),
                 qPrintable(QStringLiteral("single track voice must round-trip")));
        const auto noOp = runtime.parameters().selectTrackSingleSpeaker(commandContext(runtime),
                                                                        trackId, singerA, speakerA);
        QVERIFY2((noOp && !noOp.get().changed),
                 qPrintable(QStringLiteral("identical track speaker must be a no-op")));
    };

    {
        // Automation::OperationIds::speaker_mix::track::apply /
        // QStringLiteral("normalized-fixed-mix")

        const auto base = runtime.documentVersion();
        const auto changed = runtime.parameters().applyTrackSpeakerMix(
            commandContext(runtime), trackId, singerA, speakerA, fixed);
        const auto snapshot = trackSnapshot(runtime, trackId);
        QVERIFY2(
            (changed && changed.get().current.revision == base.revision + 1 && snapshot &&
             snapshot->data.speakerMixData.mode == SpeakerMixModel::SingerSourceMode::FixedMix &&
             snapshot->data.speakerMixData.fixedWeights == QVector<double>({1.0}) &&
             snapshot->data.speakerMixData.sourcePresetId == QStringLiteral("preset")),
            qPrintable(QStringLiteral("track preset apply must normalize weights/metadata")));
    };

    {
        // Automation::OperationIds::speaker_mix::track::replace /
        // QStringLiteral("preserve-voice-change-mix")

        const auto before = trackSnapshot(runtime, trackId);
        const auto base = runtime.documentVersion();
        const auto changed =
            runtime.parameters().replaceTrackSpeakerMix(commandContext(runtime), trackId, dynamic);
        const auto after = trackSnapshot(runtime, trackId);
        QVERIFY2(
            (changed && changed.get().current.revision == base.revision + 1 && before && after &&
             after->data.singerInfo == before->data.singerInfo &&
             after->data.speakerInfo == before->data.speakerInfo &&
             after->data.speakerMixData.mode == SpeakerMixModel::SingerSourceMode::DynamicMix &&
             after->data.speakerMixData.dynamicKeyframes.first().tick == 0),
            qPrintable(QStringLiteral("track mix replacement must preserve voice and sort keys")));
    };

    {
        // Automation::OperationIds::clips::set_voice / QStringLiteral("owned-context")

        const auto base = runtime.documentVersion();
        const auto changed = runtime.parameters().selectClipSingleSpeaker(
            commandContext(runtime), clipId, singerA, speakerB);
        const auto snapshot = clipSnapshot(runtime, clipId);
        QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && snapshot &&
                  !snapshot->data.usesTrackVoiceContext &&
                  snapshot->data.ownSingerInfo == singerA &&
                  snapshot->data.ownSpeakerInfo == speakerB),
                 qPrintable(QStringLiteral("clip speaker selection must establish owned context")));
    };

    {
        // Automation::OperationIds::speaker_mix::clip::apply /
        // QStringLiteral("apply-normalized-preset")

        const auto base = runtime.documentVersion();
        const auto changed = runtime.parameters().applyClipSpeakerMix(
            commandContext(runtime), clipId, singerA, speakerA, fixed);
        const auto snapshot = clipSnapshot(runtime, clipId);
        QVERIFY2(
            (changed && changed.get().current.revision == base.revision + 1 && snapshot &&
             snapshot->data.ownSpeakerMixData.mode == SpeakerMixModel::SingerSourceMode::FixedMix &&
             snapshot->data.ownSpeakerMixData.fixedWeights == QVector<double>({1.0})),
            qPrintable(QStringLiteral("clip preset apply must normalize mix")));
    };

    {
        // Automation::OperationIds::speaker_mix::clip::enable_dynamic /
        // QStringLiteral("dynamic-keyframes")

        const auto base = runtime.documentVersion();
        const auto changed = runtime.parameters().enableClipDynamicSpeakerMix(
            commandContext(runtime), clipId, singerA, speakerA, dynamic);
        const auto snapshot = clipSnapshot(runtime, clipId);
        QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && snapshot &&
                  snapshot->data.ownSpeakerMixData.mode ==
                      SpeakerMixModel::SingerSourceMode::DynamicMix &&
                  snapshot->data.ownSpeakerMixData.dynamicKeyframes.first().tick == 0),
                 qPrintable(QStringLiteral("dynamic clip mix must sort keyframes")));
    };

    {
        // Automation::OperationIds::speaker_mix::clip::replace /
        // QStringLiteral("preserve-owned-voice")

        const auto before = clipSnapshot(runtime, clipId);
        const auto base = runtime.documentVersion();
        const auto changed =
            runtime.parameters().replaceClipSpeakerMix(commandContext(runtime), clipId, fixed);
        const auto after = clipSnapshot(runtime, clipId);
        QVERIFY2(
            (changed && changed.get().current.revision == base.revision + 1 && before && after &&
             after->data.ownSingerInfo == before->data.ownSingerInfo &&
             after->data.ownSpeakerInfo == before->data.ownSpeakerInfo &&
             after->data.ownSpeakerMixData.mode == SpeakerMixModel::SingerSourceMode::FixedMix),
            qPrintable(QStringLiteral("clip mix replacement must preserve owned voice")));
    };

    {
        // Automation::OperationIds::clips::use_track_voice / QStringLiteral("inherit-noop")

        const auto base = runtime.documentVersion();
        const auto changed =
            runtime.parameters().useTrackVoiceContext(commandContext(runtime), clipId);
        const auto snapshot = clipSnapshot(runtime, clipId);
        QVERIFY2((changed && changed.get().current.revision == base.revision + 1 && snapshot &&
                  snapshot->data.usesTrackVoiceContext),
                 qPrintable(QStringLiteral("clip must return to track inheritance")));
        const auto noOp =
            runtime.parameters().useTrackVoiceContext(commandContext(runtime), clipId);
        QVERIFY2((noOp && !noOp.get().changed),
                 qPrintable(QStringLiteral("already inherited context must be a no-op")));
    };
}

void ProjectEditingTests::clearingTrackVoicePreservesIndependentClips() {
    TestRuntime fixture;
    auto &runtime = fixture.runtime();
    auto &parameters = runtime.parameters();
    const auto trackId = insertedTrack(runtime, QStringLiteral("Voices"));
    const auto followingId = insertedSingingClip(runtime, trackId, QStringLiteral("Following"));
    const auto independentId = insertedSingingClip(runtime, trackId, QStringLiteral("Independent"));
    const auto first = speaker(QStringLiteral("clear"));
    const auto second = speaker(QStringLiteral("soft"));
    const auto voice = singer(QStringLiteral("voice"), {first, second});
    QVERIFY(parameters.selectTrackSingleSpeaker(commandContext(runtime), trackId, voice, first));
    QVERIFY(
        parameters.selectClipSingleSpeaker(commandContext(runtime), independentId, voice, second));
    auto *track = fixture.model().tracks().first();
    auto *following = static_cast<SingingClip *>(fixture.model().findClipById(followingId.value()));
    auto *independent =
        static_cast<SingingClip *>(fixture.model().findClipById(independentId.value()));
    QVERIFY(following && independent);
    const auto independentVoice = independent->effectiveVoiceContext();
    const EffectiveVoiceContext emptyInherited{.followsTrack = true};
    const auto original = fixture.model().serialize();
    fixture.history()->reset();
    const auto before = runtime.documentVersion();

    const auto preview = parameters.clearTrackVoice(commandContext(runtime, true), trackId);
    QVERIFY(preview && preview.get().changed);
    QCOMPARE(fixture.model().serialize(), original);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(!fixture.history()->canUndo());
    const auto cleared = parameters.clearTrackVoice(commandContext(runtime), trackId);
    QVERIFY(cleared && cleared.get().changed);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    QCOMPARE(track->voiceContext(), EffectiveVoiceContext{});
    QCOMPARE(following->effectiveVoiceContext(), emptyInherited);
    QVERIFY(following->usesTrackVoiceContext());
    QCOMPARE(independent->effectiveVoiceContext(), independentVoice);
    QVERIFY(!independent->usesTrackVoiceContext());
    const auto clearedVersion = runtime.documentVersion();
    const auto repeated = parameters.clearTrackVoice(commandContext(runtime), trackId);
    QVERIFY(repeated && !repeated.get().changed);
    QCOMPARE(runtime.documentVersion(), clearedVersion);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(fixture.model().serialize(), original);
    QCOMPARE(following->speakerInfo(), first);
    QCOMPARE(independent->effectiveVoiceContext(), independentVoice);
    QVERIFY(!fixture.history()->canUndo());
    QVERIFY(runtime.history().redo(commandContext(runtime)));
    QCOMPARE(following->effectiveVoiceContext(), emptyInherited);
    QCOMPARE(independent->effectiveVoiceContext(), independentVoice);
}

void ProjectEditingTests::clearingClipVoiceStopsInheritanceUntilRestored() {
    TestRuntime fixture;
    auto &runtime = fixture.runtime();
    auto &parameters = runtime.parameters();
    const auto trackId = insertedTrack(runtime, QStringLiteral("Voices"));
    const auto clipId = insertedSingingClip(runtime, trackId, QStringLiteral("Silent clip"));
    const auto siblingId = insertedSingingClip(runtime, trackId, QStringLiteral("Following"));
    const auto first = speaker(QStringLiteral("clear"));
    const auto second = speaker(QStringLiteral("soft"));
    const auto voice = singer(QStringLiteral("voice"), {first, second});
    QVERIFY(parameters.selectTrackSingleSpeaker(commandContext(runtime), trackId, voice, first));
    auto *clip = static_cast<SingingClip *>(fixture.model().findClipById(clipId.value()));
    auto *sibling = static_cast<SingingClip *>(fixture.model().findClipById(siblingId.value()));
    QVERIFY(clip && sibling);
    fixture.history()->reset();
    const auto original = fixture.model().serialize();
    const auto before = runtime.documentVersion();
    const auto preview = parameters.clearClipVoice(commandContext(runtime, true), clipId);
    QVERIFY(preview && preview.get().changed);
    QCOMPARE(fixture.model().serialize(), original);
    QCOMPARE(runtime.documentVersion(), before);
    const auto cleared = parameters.clearClipVoice(commandContext(runtime), clipId);
    QVERIFY(cleared && cleared.get().changed);
    QVERIFY(!clip->usesTrackVoiceContext());
    QCOMPARE(clip->effectiveVoiceContext(), EffectiveVoiceContext{});
    QCOMPARE(sibling->speakerInfo(), first);
    QCOMPARE(runtime.documentVersion().revision, before.revision + 1);
    const auto repeated = parameters.clearClipVoice(commandContext(runtime), clipId);
    QVERIFY(repeated && !repeated.get().changed);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(fixture.model().serialize(), original);
    QVERIFY(clip->usesTrackVoiceContext());
    QCOMPARE(clip->speakerInfo(), first);
    QVERIFY(!fixture.history()->canUndo());
    QVERIFY(runtime.history().redo(commandContext(runtime)));
    QVERIFY(parameters.selectTrackSingleSpeaker(commandContext(runtime), trackId, voice, second));
    QCOMPARE(sibling->speakerInfo(), second);
    QCOMPARE(clip->effectiveVoiceContext(), EffectiveVoiceContext{});
    const auto restored = parameters.useTrackVoiceContext(commandContext(runtime), clipId);
    QVERIFY(restored && restored.get().changed);
    QCOMPARE(clip->speakerInfo(), second);
    QVERIFY(clip->usesTrackVoiceContext());
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QVERIFY(!clip->usesTrackVoiceContext());
    QCOMPARE(clip->effectiveVoiceContext(), EffectiveVoiceContext{});
    QCOMPARE(sibling->speakerInfo(), second);
}

void ProjectEditingTests::pronunciationSourcesAndResetPreserveAutomaticWords() {
    NoteFixture fixture;
    auto &runtime = fixture.testRuntime.runtime();
    auto &notes = runtime.notes();
    const auto original = fixture.testRuntime.model().serialize();
    const auto before = runtime.documentVersion();
    const auto automatic = notes.setPronunciation(commandContext(runtime, true), fixture.clipId,
                                                  fixture.firstNoteId, true, "auto-la");
    QVERIFY(automatic && automatic.get().changed);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(fixture.testRuntime.model().serialize(), original);
    QVERIFY(notes.setPronunciation(commandContext(runtime), fixture.clipId, fixture.firstNoteId,
                                   true, "auto-la"));
    auto current = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    QVERIFY(current);
    QCOMPARE(current->data.pronunciation.original, QStringLiteral("auto-la"));
    QCOMPARE(current->data.pronunciation.edited, QStringLiteral("custom-la"));
    QCOMPARE(current->data.lyric, QStringLiteral("la"));
    const auto automaticVersion = runtime.documentVersion();
    const auto repeated = notes.setPronunciation(commandContext(runtime), fixture.clipId,
                                                 fixture.firstNoteId, true, "auto-la");
    QVERIFY(repeated && !repeated.get().changed);
    QCOMPARE(runtime.documentVersion(), automaticVersion);
    QVERIFY(notes.setPronunciation(commandContext(runtime), fixture.clipId, fixture.firstNoteId,
                                   false, "manual-la"));
    current = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    QVERIFY(current);
    QCOMPARE(current->data.pronunciation.original, QStringLiteral("auto-la"));
    QCOMPARE(current->data.pronunciation.edited, QStringLiteral("manual-la"));
    QVERIFY(notes.resetPronunciation(commandContext(runtime), fixture.clipId, fixture.firstNoteId));
    current = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    QVERIFY(current);
    QCOMPARE(current->data.pronunciation.original, QStringLiteral("auto-la"));
    QVERIFY(current->data.pronunciation.edited.isEmpty());
    const auto resetVersion = runtime.documentVersion();
    const auto resetAgain =
        notes.resetPronunciation(commandContext(runtime), fixture.clipId, fixture.firstNoteId);
    QVERIFY(resetAgain && !resetAgain.get().changed);
    QCOMPARE(runtime.documentVersion(), resetVersion);
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    current = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    QVERIFY(current);
    QCOMPARE(current->data.pronunciation.edited, QStringLiteral("manual-la"));
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    current = noteSnapshot(runtime, fixture.clipId, fixture.firstNoteId);
    QVERIFY(current);
    QCOMPARE(current->data.pronunciation.edited, QStringLiteral("custom-la"));
    QCOMPARE(current->data.pronunciation.original, QStringLiteral("auto-la"));
    QVERIFY(runtime.history().undo(commandContext(runtime)));
    QCOMPARE(fixture.testRuntime.model().serialize(), original);
    QVERIFY(!fixture.testRuntime.history()->canUndo());
}

void ProjectEditingTests::timelineAndHistoryDomain() {
    TestRuntime testRuntime;
    auto &runtime = testRuntime.runtime();

    {
        // Automation::OperationIds::timeline::get / QStringLiteral("anchors-and-version")

        const auto timeline = runtime.timeline().getTimeline(runtime.documentVersion().documentId);
        QVERIFY2((timeline && timeline.get().document == runtime.documentVersion() &&
                  !timeline.get().tempos.isEmpty() && timeline.get().tempos.first().pos == 0 &&
                  !timeline.get().timeSignatures.isEmpty() &&
                  timeline.get().timeSignatures.first().barIndex == 0),
                 qPrintable(QStringLiteral("timeline query must expose both immutable anchors")));
        const auto wrong = runtime.timeline().getTimeline(Automation::DocumentId::create());
        QVERIFY2((!wrong && wrong.getError().code == AutomationErrorCode::DocumentChanged),
                 qPrintable(QStringLiteral("timeline query must reject old document ID")));
    };

    {
        // Automation::OperationIds::tempos::set /
        // QStringLiteral("invalid-preview-sorted-replace-noop")

        const auto base = runtime.documentVersion();
        const auto badTick = runtime.timeline().setTempo(commandContext(runtime), -1, 120.0);
        const auto badValue = runtime.timeline().setTempo(commandContext(runtime), 960,
                                                          std::numeric_limits<double>::quiet_NaN());
        QVERIFY2(
            (isError(badTick, AutomationErrorCode::InvalidArgument, QStringLiteral("tempo")) &&
             isError(badValue, AutomationErrorCode::InvalidArgument, QStringLiteral("tempo")) &&
             runtime.documentVersion() == base),
            qPrintable(QStringLiteral("invalid tempo inputs must not mutate")));
        const auto preview =
            runtime.timeline().setTempo(commandContext(runtime, true), 1920, 150.0);
        QVERIFY2((preview && preview.get().changed && preview.get().validatedOnly &&
                  runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("tempo preview must be side-effect free")));
        const auto later = runtime.timeline().setTempo(commandContext(runtime), 1920, 150.0);
        const auto earlier = runtime.timeline().setTempo(commandContext(runtime), 960, 140.0);
        auto timeline = runtime.timeline().getTimeline(runtime.documentVersion().documentId);
        QVERIFY2((later && earlier && timeline && timeline.get().tempos.size() == 3 &&
                  timeline.get().tempos.at(1).pos == 960 &&
                  timeline.get().tempos.at(2).pos == 1920),
                 qPrintable(QStringLiteral("tempo insertion must remain sorted")));
        const auto replace = runtime.timeline().setTempo(commandContext(runtime), 960, 145.0);
        const auto replaced = runtime.timeline().getTimeline(runtime.documentVersion().documentId);
        QVERIFY2((replace && replaced && hasTempo(replaced.get(), 960, 145.0)),
                 qPrintable(QStringLiteral("same tick must replace tempo")));
        const auto noOp = runtime.timeline().setTempo(commandContext(runtime), 960, 145.0);
        QVERIFY2((noOp && !noOp.get().changed),
                 qPrintable(QStringLiteral("identical tempo must be a no-op")));
    };

    {
        // Automation::OperationIds::tempos::remove /
        // QStringLiteral("anchor-missing-preview-undo")

        testRuntime.history()->reset();
        const auto anchor = runtime.timeline().deleteTempo(commandContext(runtime), 0);
        QVERIFY2((isError(anchor, AutomationErrorCode::InvalidArgument, QStringLiteral("tick"))),
                 qPrintable(QStringLiteral("tick-zero tempo anchor cannot be deleted")));
        const auto missing = runtime.timeline().deleteTempo(commandContext(runtime), 7777);
        QVERIFY2((missing && !missing.get().changed),
                 qPrintable(QStringLiteral("missing tempo deletion must be a no-op")));
        const auto base = runtime.documentVersion();
        const auto preview = runtime.timeline().deleteTempo(commandContext(runtime, true), 960);
        QVERIFY2((preview && preview.get().changed && runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("tempo delete preview must preserve point")));
        const auto removed = runtime.timeline().deleteTempo(commandContext(runtime), 960);
        const auto timeline = runtime.timeline().getTimeline(runtime.documentVersion().documentId);
        QVERIFY2((removed && timeline && !hasTempo(timeline.get(), 960, 145.0)),
                 qPrintable(QStringLiteral("existing non-anchor tempo must be removed")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        const auto restored = runtime.timeline().getTimeline(runtime.documentVersion().documentId);
        QVERIFY2((undo && restored && hasTempo(restored.get(), 960, 145.0)),
                 qPrintable(QStringLiteral("tempo deletion must undo once")));
    };

    {
        // Automation::OperationIds::tempos::set /
        // QStringLiteral("legacy-audio-tempo-undo-restores-raw-range")

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
        QVERIFY2((trackId.isValid() && insert && before && changed && changedSnapshot &&
                  changedSnapshot->data.properties.length >=
                      changedSnapshot->data.properties.clipStart +
                          changedSnapshot->data.properties.clipLen &&
                  undo && restored &&
                  sameClipTiming(restored->data.properties, before->data.properties) && redo &&
                  redone && undoAgain && restoredAgain &&
                  sameClipTiming(restoredAgain->data.properties, before->data.properties)),
                 qPrintable(QStringLiteral(
                     "tempo undo must restore raw legacy audio ticks across redo cycles")));
    };

    {
        // Automation::OperationIds::time_signatures::set /
        // QStringLiteral("invalid-preview-sorted-replace-noop")

        const auto base = runtime.documentVersion();
        const auto invalidBar =
            runtime.timeline().setTimeSignature(commandContext(runtime), -1, 4, 4);
        const auto invalidNumerator =
            runtime.timeline().setTimeSignature(commandContext(runtime), 2, 0, 4);
        const auto invalidDenominator =
            runtime.timeline().setTimeSignature(commandContext(runtime), 2, 3, 3);
        const auto overflowingPosition = runtime.timeline().setTimeSignature(
            commandContext(runtime), std::numeric_limits<int>::max(), 4, 4);
        const auto overflowingBarLength = runtime.timeline().setTimeSignature(
            commandContext(runtime), 2, std::numeric_limits<int>::max(), 4);
        QVERIFY2((isError(invalidBar, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("time_signature")) &&
                  isError(invalidNumerator, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("time_signature")) &&
                  isError(invalidDenominator, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("time_signature")) &&
                  isError(overflowingPosition, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("time_signature")) &&
                  isError(overflowingBarLength, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("time_signature")) &&
                  runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("invalid signatures must fail atomically")));
        const auto preview =
            runtime.timeline().setTimeSignature(commandContext(runtime, true), 8, 6, 8);
        QVERIFY2((preview && preview.get().changed && runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("signature preview must be side-effect free")));
        const auto later = runtime.timeline().setTimeSignature(commandContext(runtime), 8, 6, 8);
        const auto earlier = runtime.timeline().setTimeSignature(commandContext(runtime), 4, 3, 4);
        const auto timeline = runtime.timeline().getTimeline(runtime.documentVersion().documentId);
        QVERIFY2((later && earlier && timeline && timeline.get().timeSignatures.size() == 3 &&
                  timeline.get().timeSignatures.at(1).barIndex == 4 &&
                  timeline.get().timeSignatures.at(2).barIndex == 8),
                 qPrintable(QStringLiteral("signature insertion must remain sorted")));
        const auto replace = runtime.timeline().setTimeSignature(commandContext(runtime), 4, 5, 4);
        const auto replaced = runtime.timeline().getTimeline(runtime.documentVersion().documentId);
        QVERIFY2((replace && replaced && hasSignature(replaced.get(), 4, 5, 4)),
                 qPrintable(QStringLiteral("same bar must replace signature")));
        const auto noOp = runtime.timeline().setTimeSignature(commandContext(runtime), 4, 5, 4);
        QVERIFY2((noOp && !noOp.get().changed),
                 qPrintable(QStringLiteral("identical signature must be a no-op")));
    };

    {
        // Automation::OperationIds::time_signatures::remove /
        // QStringLiteral("anchor-missing-preview-undo")

        testRuntime.history()->reset();
        const auto anchor = runtime.timeline().deleteTimeSignature(commandContext(runtime), 0);
        QVERIFY2(
            (isError(anchor, AutomationErrorCode::InvalidArgument, QStringLiteral("bar_index"))),
            qPrintable(QStringLiteral("bar-zero signature anchor cannot be deleted")));
        const auto missing = runtime.timeline().deleteTimeSignature(commandContext(runtime), 777);
        QVERIFY2((missing && !missing.get().changed),
                 qPrintable(QStringLiteral("missing signature deletion must be a no-op")));
        const auto base = runtime.documentVersion();
        const auto preview =
            runtime.timeline().deleteTimeSignature(commandContext(runtime, true), 4);
        QVERIFY2((preview && preview.get().changed && runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("signature delete preview must preserve point")));
        const auto removed = runtime.timeline().deleteTimeSignature(commandContext(runtime), 4);
        const auto timeline = runtime.timeline().getTimeline(runtime.documentVersion().documentId);
        QVERIFY2((removed && timeline && !hasSignature(timeline.get(), 4, 5, 4)),
                 qPrintable(QStringLiteral("existing non-anchor signature must be removed")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        const auto restored = runtime.timeline().getTimeline(runtime.documentVersion().documentId);
        QVERIFY2((undo && restored && hasSignature(restored.get(), 4, 5, 4)),
                 qPrintable(QStringLiteral("signature deletion must undo once")));

        TestRuntime overflowTestRuntime;
        auto &overflowRuntime = overflowTestRuntime.runtime();
        const auto hugeAnchor = overflowRuntime.timeline().setTimeSignature(
            commandContext(overflowRuntime), 0, 1000000, 1);
        const auto bridge = overflowRuntime.timeline().setTimeSignature(
            commandContext(overflowRuntime), 1, 1, 2048);
        const auto laterSignature =
            overflowRuntime.timeline().setTimeSignature(commandContext(overflowRuntime), 2, 4, 4);
        const auto overflowBase = overflowRuntime.documentVersion();
        const auto overflowPreview = overflowRuntime.timeline().deleteTimeSignature(
            commandContext(overflowRuntime, true), 1);
        const auto overflowRemoval =
            overflowRuntime.timeline().deleteTimeSignature(commandContext(overflowRuntime), 1);
        const auto overflowTimeline =
            overflowRuntime.timeline().getTimeline(overflowRuntime.documentVersion().documentId);
        QVERIFY2((hugeAnchor && bridge && laterSignature &&
                  isError(overflowPreview, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("bar_index")) &&
                  isError(overflowRemoval, AutomationErrorCode::InvalidArgument,
                          QStringLiteral("bar_index")) &&
                  overflowRuntime.documentVersion() == overflowBase && overflowTimeline &&
                  hasSignature(overflowTimeline.get(), 1, 1, 2048) &&
                  hasSignature(overflowTimeline.get(), 2, 4, 4)),
                 qPrintable(QStringLiteral(
                     "signature deletion must reject a derived tick overflow atomically")));
    };

    {
        // Automation::OperationIds::master::set_control /
        // QStringLiteral("invalid-preview-noop-undo")

        testRuntime.history()->reset();
        TrackControl invalid;
        invalid.setGain(std::numeric_limits<double>::infinity());
        const auto rejected = runtime.timeline().setMasterControl(commandContext(runtime), invalid);
        QVERIFY2(
            (isError(rejected, AutomationErrorCode::InvalidArgument, QStringLiteral("control"))),
            qPrintable(QStringLiteral("non-finite master control must be rejected")));
        TrackControl control;
        control.setGain(0.6);
        control.setPan(-0.4);
        control.setMute(true);
        const auto base = runtime.documentVersion();
        const auto preview =
            runtime.timeline().setMasterControl(commandContext(runtime, true), control);
        QVERIFY2((preview && preview.get().changed && runtime.documentVersion() == base),
                 qPrintable(QStringLiteral("master preview must not mutate")));
        const auto changed = runtime.timeline().setMasterControl(commandContext(runtime), control);
        QVERIFY2((changed && changed.get().current.revision == base.revision + 1),
                 qPrintable(QStringLiteral("master control must advance one revision")));
        const auto noOp = runtime.timeline().setMasterControl(commandContext(runtime), control);
        QVERIFY2((noOp && !noOp.get().changed),
                 qPrintable(QStringLiteral("identical master control must be a no-op")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        const auto redo = runtime.history().redo(commandContext(runtime));
        QVERIFY2((undo && undo.get().changed && redo && redo.get().changed),
                 qPrintable(QStringLiteral("master control must support undo and redo")));
    };

    {
        // Automation::OperationIds::history::get_state /
        // QStringLiteral("empty-and-named-state")

        testRuntime.history()->reset();
        const auto empty = runtime.history().getState(runtime.documentVersion().documentId);
        QVERIFY2((empty && !empty.get().canUndo && !empty.get().canRedo && empty.get().onSavePoint),
                 qPrintable(QStringLiteral("reset History must be empty at savepoint")));
        const auto tempo = runtime.timeline().setTempo(commandContext(runtime), 2880, 130.0);
        const auto state = runtime.history().getState(runtime.documentVersion().documentId);
        QVERIFY2((tempo && state && state.get().canUndo && !state.get().canRedo &&
                  !state.get().onSavePoint && !state.get().undoName.isEmpty()),
                 qPrintable(QStringLiteral("committed edit must expose named undo state")));
    };

    {
        // Automation::OperationIds::history::undo / QStringLiteral("preview-commit-empty")

        const auto before = runtime.documentVersion();
        const auto preview = runtime.history().undo(commandContext(runtime, true));
        QVERIFY2((preview && preview.get().validatedOnly && preview.get().changed &&
                  runtime.documentVersion() == before),
                 qPrintable(QStringLiteral("undo preview must not consume History")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        QVERIFY2((undo && undo.get().changed && undo.get().current.revision == before.revision + 1),
                 qPrintable(QStringLiteral("undo must advance one revision")));
        const auto empty = runtime.history().undo(commandContext(runtime));
        QVERIFY2((empty && !empty.get().changed),
                 qPrintable(QStringLiteral("empty undo must be a successful no-op")));
    };

    {
        // Automation::OperationIds::history::redo /
        // QStringLiteral("preview-commit-branch-clear")

        const auto before = runtime.documentVersion();
        const auto preview = runtime.history().redo(commandContext(runtime, true));
        QVERIFY2((preview && preview.get().validatedOnly && preview.get().changed &&
                  runtime.documentVersion() == before),
                 qPrintable(QStringLiteral("redo preview must not consume History")));
        const auto redo = runtime.history().redo(commandContext(runtime));
        QVERIFY2((redo && redo.get().changed && redo.get().current.revision == before.revision + 1),
                 qPrintable(QStringLiteral("redo must advance one revision")));
        const auto undo = runtime.history().undo(commandContext(runtime));
        const auto branch = runtime.timeline().setTempo(commandContext(runtime), 3360, 125.0);
        const auto state = runtime.history().getState(runtime.documentVersion().documentId);
        QVERIFY2((undo && branch && state && !state.get().canRedo),
                 qPrintable(QStringLiteral("new edit after undo must clear redo branch")));
    };
}

void ProjectEditingTests::quantizeInChild() {
    {
        // Automation::OperationIds::notes::quantize /
        // QStringLiteral("commit-noop-process-isolation")

        QProcess probe;
        probe.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--quantize-probe")});
        const auto started = probe.waitForStarted(5000);
        const auto finished = started && probe.waitForFinished(3000);
        if (!finished) {
            probe.kill();
            probe.waitForFinished(1000);
        }
        QVERIFY2((started && finished && probe.exitStatus() == QProcess::NormalExit &&
                  probe.exitCode() == 0),
                 qPrintable(QStringLiteral("quantize must commit safely, reject out-of-range "
                                           "geometry, and no-op when aligned; exit=%1 stderr=%2")
                                .arg(probe.exitCode())
                                .arg(QString::fromUtf8(probe.readAllStandardError()))));
    };
}

int runProjectQuantizeProbe(int argc, char **argv) {
    return quantizeProbe(argc, argv);
}
