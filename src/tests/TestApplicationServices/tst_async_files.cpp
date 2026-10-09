#include "tst_application_services.h"

#include "AsyncFileDomainSupport.h"
#include "../TestSupport/ProjectSnapshot.h"

#include <QCoreApplication>
#include <QtTest>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <limits>

namespace {
    using namespace AutomationAsyncFileTests;

    [[nodiscard]] Automation::InferenceMutationRequest
        inferenceRequest(const RuntimeHarness &harness) {
        Automation::InferenceMutationRequest request;
        request.kind = Automation::InferenceMutationKind::ApplyPitch;
        request.clipId = harness.singingClipId();
        request.pieceId = Automation::PieceId(1000);
        request.noteIds = {harness.noteId()};
        return request;
    }

    [[nodiscard]] Automation::TrackDraftDto importedTrack(const QString &clientRef) {
        Automation::TrackDraftDto track;
        track.clientRef = clientRef;
        track.name = QStringLiteral("Imported Track");
        track.gain = 1.0;
        track.defaultLanguage = QStringLiteral("en");
        Automation::ClipDraftDto clip;
        clip.clientRef = clientRef + QStringLiteral("-clip");
        clip.type = Automation::ClipDraftDto::Type::Singing;
        clip.properties.name = QStringLiteral("Imported Clip");
        clip.properties.length = 960;
        clip.properties.clipLen = 960;
        clip.properties.gain = 1.0;
        clip.defaultLanguage = QStringLiteral("en");
        track.clips.append(clip);
        return track;
    }

    [[nodiscard]] Automation::AudioExportConfigDto audioConfig(const RuntimeHarness &harness,
                                                               const QString &name) {
        Automation::AudioExportConfigDto config;
        config.fileName = name;
        config.fileDirectory = harness.temporaryDirectoryPath();
        return config;
    }

}

void ApplicationServicesTests::inferenceCommitPolicy_data() {
    QTest::addColumn<bool>("advancesRevision");
    QTest::newRow("persistent-result") << true;
    QTest::newRow("transient-result") << false;
}

void ApplicationServicesTests::inferenceCommitPolicy() {
    QFETCH(bool, advancesRevision);
    RuntimeHarness harness;
    QVERIFY(harness.isReady());
    auto &runtime = harness.runtime();
    harness.inferenceAdvancesRevision = advancesRevision;
    const auto request = inferenceRequest(harness);
    const auto base = runtime.documentVersion();
    const auto expectedRevision = base.revision + (advancesRevision ? 1 : 0);

    const auto preview = runtime.inference().applyMutation(harness.context(true), request);
    QVERIFY(preview);
    QVERIFY(preview.get().mutation.validatedOnly);
    QVERIFY(preview.get().mutation.changed);
    QCOMPARE(preview.get().mutation.previous, base);
    QCOMPARE(preview.get().mutation.current.documentId, base.documentId);
    QCOMPARE(preview.get().mutation.current.revision, expectedRevision);
    QCOMPARE(runtime.documentVersion(), base);
    QCOMPARE(harness.inferencePrepareCount, 1);
    QCOMPARE(harness.inferenceApplyCount, 0);

    const auto committed = runtime.inference().applyMutation(harness.context(), request);
    QVERIFY(committed);
    QVERIFY(committed.get().mutation.changed);
    QVERIFY(!committed.get().mutation.validatedOnly);
    QCOMPARE(committed.get().mutation.current.revision, expectedRevision);
    QCOMPARE(runtime.documentVersion().revision, expectedRevision);
    QCOMPARE(harness.inferencePrepareCount, 2);
    QCOMPARE(harness.inferenceApplyCount, 1);
    QCOMPARE(harness.lastPreparedInferenceKind, request.kind);
    QCOMPARE(harness.lastAppliedInferenceKind, request.kind);
    QCOMPARE(committed.get().sideEffects.changedPieces,
             QList<Automation::PieceId>{request.pieceId});

    harness.inferenceChanged = false;
    const auto noOpBase = runtime.documentVersion();
    const auto noOp = runtime.inference().applyMutation(harness.context(), request);
    QVERIFY(noOp);
    QVERIFY(!noOp.get().mutation.changed);
    QCOMPARE(runtime.documentVersion(), noOpBase);
    QCOMPARE(harness.inferenceApplyCount, 1);
}

void ApplicationServicesTests::inferenceFailureAndGenerationGuards() {
    RuntimeHarness harness;
    QVERIFY(harness.isReady());
    auto &runtime = harness.runtime();
    const auto request = inferenceRequest(harness);
    const auto operation = Automation::OperationIds::inference::apply_pitch;

    auto wrongDocument = harness.context();
    wrongDocument.expected.documentId = Automation::DocumentId::create();
    ++wrongDocument.expected.revision;
    const auto rejectedDocument = runtime.inference().applyMutation(wrongDocument, request);
    QVERIFY(isError(rejectedDocument, Automation::AutomationErrorCode::DocumentChanged, operation));
    QCOMPARE(harness.inferencePrepareCount, 0);

    auto stale = harness.context();
    ++stale.expected.revision;
    const auto rejectedRevision = runtime.inference().applyMutation(stale, request);
    QVERIFY(
        isError(rejectedRevision, Automation::AutomationErrorCode::RevisionConflict, operation));
    QCOMPARE(harness.inferencePrepareCount, 0);

    Automation::AutomationError backendError;
    backendError.code = Automation::AutomationErrorCode::InferenceError;
    backendError.message = QStringLiteral("controlled preparation failure");
    harness.inferenceError = backendError;
    const auto rejectedBackend = runtime.inference().applyMutation(harness.context(), request);
    QVERIFY(isError(rejectedBackend, backendError.code, operation));
    QCOMPARE(rejectedBackend.getError().message, backendError.message);
    harness.inferenceError.reset();

    const auto sharedBase = runtime.documentVersion();
    const auto first =
        runtime.inference().applyMutation(RuntimeHarness::contextFor(sharedBase), request);
    QVERIFY(first);
    const auto rebased =
        Automation::rebaseDocumentVersionWithinGeneration(sharedBase, runtime.documentVersion());
    QVERIFY(rebased);
    auto siblingRequest = request;
    siblingRequest.kind = Automation::InferenceMutationKind::ApplyVariance;
    const auto sibling = runtime.inference().applyMutation(
        RuntimeHarness::contextFor(rebased.get()), siblingRequest);
    QVERIFY(sibling);
    QCOMPARE(runtime.documentVersion().revision, sharedBase.revision + 2);

    const auto staleGeneration =
        Automation::DocumentVersion{Automation::DocumentId::create(), sharedBase.revision};
    const auto workflowContext = runtime.documentWorkflowCommitContext(sharedBase);
    QVERIFY(workflowContext);
    QCOMPARE(workflowContext.get().expected, runtime.documentVersion());
    QCOMPARE(workflowContext.get().source, Automation::InvocationSource::TrustedGui);
    QVERIFY(isError(Automation::rebaseDocumentVersionWithinGeneration(staleGeneration,
                                                                      runtime.documentVersion()),
                    Automation::AutomationErrorCode::DocumentChanged));
    QVERIFY(isError(runtime.documentWorkflowCommitContext(staleGeneration),
                    Automation::AutomationErrorCode::DocumentChanged));

    const auto staleContext = harness.context();
    const auto preparesBeforeReplacement = harness.inferencePrepareCount;
    QVERIFY(
        runtime.documents().commitNewDocument(harness.context(), RuntimeHarness::emptyDocument()));
    const auto late = runtime.inference().applyMutation(staleContext, request);
    QVERIFY(isError(late, Automation::AutomationErrorCode::DocumentChanged, operation));
    QCOMPARE(harness.inferencePrepareCount, preparesBeforeReplacement);

    RuntimeHarness unavailable({.inferenceServices = false});
    QVERIFY(unavailable.isReady());
    const auto unavailableRequest = inferenceRequest(unavailable);
    const auto missing =
        unavailable.runtime().inference().applyMutation(unavailable.context(), unavailableRequest);
    QVERIFY(isError(missing, Automation::AutomationErrorCode::ModuleNotReady, operation));
}

void ApplicationServicesTests::audioClipDomain() {
    RuntimeHarness harness;
    QVERIFY2((harness.isReady()), qPrintable(QStringLiteral("audio clip harness must initialize")));
    auto &runtime = harness.runtime();
    auto *audio =
        dynamic_cast<AudioClip *>(harness.model().findClipById(harness.audioClipId().value()));
    QVERIFY2((audio != nullptr),
             qPrintable(QStringLiteral("fixture audio clip must be addressable")));
    if (!audio)
        return;

    {
        // Automation::OperationIds::audio_clips::apply_decode_cache /
        // QStringLiteral("derived-preview-commit-no-op-and-stale-path")

        AudioInfoModel info;
        info.sampleRate = 48000;
        info.channels = 2;
        info.frames = 96000;
        info.peakCache.append({-20, 20});
        const auto base = runtime.documentVersion();
        const auto asset = Automation::audioAssetSnapshotDto(*audio);
        const auto preview = runtime.project().applyAudioDecodeCache(
            harness.context(true), harness.audioClipId(), asset, info);
        QVERIFY2((preview && preview.get().validatedOnly && preview.get().changed &&
                  runtime.documentVersion() == base && audio->audioInfo().sampleRate != 48000),
                 qPrintable(QStringLiteral("decode preview must not install cache data")));
        const auto commit = runtime.project().applyAudioDecodeCache(
            harness.context(), harness.audioClipId(), asset, info);
        const auto noOp = runtime.project().applyAudioDecodeCache(
            harness.context(), harness.audioClipId(), asset, info);
        auto staleAsset = asset;
        staleAsset.path = QStringLiteral("stale.wav");
        const auto stale = runtime.project().applyAudioDecodeCache(
            harness.context(), harness.audioClipId(), staleAsset, info);
        QVERIFY2((commit && commit.get().changed && noOp && !noOp.get().changed &&
                  runtime.documentVersion() == base && audio->audioInfo().sampleRate == 48000 &&
                  audio->pathStatus() == AudioClip::PathStatus::Normal &&
                  isError(stale, Automation::AutomationErrorCode::InvalidArgument,
                          Automation::OperationIds::audio_clips::apply_decode_cache)),
                 qPrintable(QStringLiteral(
                     "decode cache must be derived, no-op aware, and path guarded")));
    };

    {
        // Automation::OperationIds::audio_clips::set_path_status /
        // QStringLiteral("derived-status-and-validation")

        const auto base = runtime.documentVersion();
        const auto asset = Automation::audioAssetSnapshotDto(*audio);
        const auto preview =
            runtime.project().setAudioClipPathStatus(harness.context(true), harness.audioClipId(),
                                                     asset, AudioClip::PathStatus::Unconfirmed);
        const auto commit = runtime.project().setAudioClipPathStatus(
            harness.context(), harness.audioClipId(), asset, AudioClip::PathStatus::Unconfirmed);
        const auto noOp = runtime.project().setAudioClipPathStatus(
            harness.context(), harness.audioClipId(), asset, AudioClip::PathStatus::Unconfirmed);
        auto staleAsset = asset;
        staleAsset.path = QStringLiteral("stale.wav");
        const auto stale = runtime.project().setAudioClipPathStatus(
            harness.context(), harness.audioClipId(), staleAsset, AudioClip::PathStatus::Missing);
        QVERIFY2(
            (preview && preview.get().validatedOnly && commit && commit.get().changed && noOp &&
             !noOp.get().changed && audio->pathStatus() == AudioClip::PathStatus::Unconfirmed &&
             runtime.documentVersion() == base &&
             isError(stale, Automation::AutomationErrorCode::InvalidArgument,
                     Automation::OperationIds::audio_clips::set_path_status)),
            qPrintable(
                QStringLiteral("path status must update without revision and reject stale paths")));
    };

    {
        // Automation::OperationIds::audio_clips::set_hash /
        // QStringLiteral("derived-hash-and-empty-input")

        const auto base = runtime.documentVersion();
        const auto asset = Automation::audioAssetSnapshotDto(*audio);
        const auto preview = runtime.project().setAudioClipHash(
            harness.context(true), harness.audioClipId(), asset, QStringLiteral("sha512-a"));
        const auto commit = runtime.project().setAudioClipHash(
            harness.context(), harness.audioClipId(), asset, QStringLiteral("sha512-a"));
        const auto updatedAsset = Automation::audioAssetSnapshotDto(*audio);
        const auto noOp = runtime.project().setAudioClipHash(
            harness.context(), harness.audioClipId(), updatedAsset, QStringLiteral("sha512-a"));
        const auto empty = runtime.project().setAudioClipHash(
            harness.context(), harness.audioClipId(), updatedAsset, {});
        QVERIFY2(
            (preview && preview.get().validatedOnly && commit && commit.get().changed && noOp &&
             !noOp.get().changed && audio->pathInfo().sha512 == QStringLiteral("sha512-a") &&
             runtime.documentVersion() == base &&
             isError(empty, Automation::AutomationErrorCode::InvalidArgument,
                     Automation::OperationIds::audio_clips::set_hash)),
            qPrintable(QStringLiteral("hash writeback must be derived and reject empty hashes")));
    };

    {
        // Automation::OperationIds::audio_clips::apply_resolved_path /
        // QStringLiteral("derived-path-resolution-and-empty-target")

        QTemporaryDir resolvedFiles;
        const auto resolvedPath =
            QDir(resolvedFiles.path()).filePath(QStringLiteral("resolved.wav"));
        QFile resolvedFile(resolvedPath);
        const bool fixtureReady = resolvedFiles.isValid() &&
                                  resolvedFile.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                                  resolvedFile.write("resolved-audio") == 14;
        QVERIFY2((fixtureReady),
                 qPrintable(QStringLiteral("resolved audio fixture must be available")));
        if (!fixtureReady)
            return;
        const auto base = runtime.documentVersion();
        const auto asset = Automation::audioAssetSnapshotDto(*audio);
        const auto preview = runtime.project().applyResolvedAudioPath(
            harness.context(true), harness.audioClipId(), asset, resolvedPath,
            AudioClip::PathStatus::Normal);
        const auto commit = runtime.project().applyResolvedAudioPath(
            harness.context(), harness.audioClipId(), asset, resolvedPath,
            AudioClip::PathStatus::Normal);
        const auto resolvedAsset = Automation::audioAssetSnapshotDto(*audio);
        const auto noOp = runtime.project().applyResolvedAudioPath(
            harness.context(), harness.audioClipId(), resolvedAsset, audio->path(),
            AudioClip::PathStatus::Normal);
        const auto empty = runtime.project().applyResolvedAudioPath(
            harness.context(), harness.audioClipId(), resolvedAsset, {},
            AudioClip::PathStatus::Normal);
        QVERIFY2(
            (preview && preview.get().validatedOnly && commit && commit.get().changed && noOp &&
             !noOp.get().changed && audio->path() == resolvedPath &&
             runtime.documentVersion() == base &&
             isError(empty, Automation::AutomationErrorCode::InvalidArgument,
                     Automation::OperationIds::audio_clips::apply_resolved_path)),
            qPrintable(QStringLiteral("resolved path must be snapshot-guarded derived state")));
    };

    {
        // Automation::OperationIds::audio_clips::relocate /
        // QStringLiteral("history-commit-preview-no-op-and-invalid")

        AudioPathInfo info;
        info.relativeDir = QStringLiteral("media");
        info.sha512 = QStringLiteral("sha512-b");
        const QJsonObject format{
            {QStringLiteral("codec"), QStringLiteral("pcm")}
        };
        const auto base = runtime.documentVersion();
        const auto preview =
            runtime.project().relocateAudioClip(harness.context(true), harness.audioClipId(),
                                                QStringLiteral("relocated.wav"), info, format);
        const auto commit =
            runtime.project().relocateAudioClip(harness.context(), harness.audioClipId(),
                                                QStringLiteral("relocated.wav"), info, format);
        const auto noOp =
            runtime.project().relocateAudioClip(harness.context(), harness.audioClipId(),
                                                QStringLiteral("relocated.wav"), info, format);
        const auto empty = runtime.project().relocateAudioClip(
            harness.context(), harness.audioClipId(), {}, info, format);
        const auto wrongType = runtime.project().relocateAudioClip(
            harness.context(), harness.singingClipId(), QStringLiteral("wrong.wav"), info, format);
        QVERIFY2((preview && preview.get().validatedOnly && commit &&
                  commit.get().current.revision == base.revision + 1 && noOp &&
                  !noOp.get().changed && audio->path() == QStringLiteral("relocated.wav") &&
                  isError(empty, Automation::AutomationErrorCode::InvalidArgument,
                          Automation::OperationIds::audio_clips::relocate) &&
                  isError(wrongType, Automation::AutomationErrorCode::WrongObjectType,
                          Automation::OperationIds::audio_clips::relocate)),
                 qPrintable(
                     QStringLiteral("relocation must be one history revision with typed errors")));
    };

    {
        // Automation::OperationIds::audio_clips::confirm_path /
        // QStringLiteral("prepared-path-preview-single-history-commit-and-undo")

        const auto previous = Automation::audioAssetSnapshotDto(*audio);
        const auto preparedPath = harness.temporaryPath(QStringLiteral("confirmed.wav"));
        const AudioPathInfo preparedInfo{{}, QStringLiteral("sha512-confirmed")};
        const QJsonObject preparedFormat{
            {QStringLiteral("entryClassName"), QStringLiteral("PreparedFormatEntry")},
            {QStringLiteral("userData"),       QStringLiteral("prepared-data")      },
        };
        const auto base = runtime.documentVersion();
        const auto preview =
            runtime.project().confirmAudioClipPath(harness.context(true), harness.audioClipId(),
                                                   preparedPath, preparedInfo, preparedFormat);
        const auto afterPreview = Automation::audioAssetSnapshotDto(*audio);
        const auto commit = runtime.project().confirmAudioClipPath(
            harness.context(), harness.audioClipId(), preparedPath, preparedInfo, preparedFormat);
        const auto committed = Automation::audioAssetSnapshotDto(*audio);
        const auto undo = runtime.history().undo(harness.context());
        const auto restored = Automation::audioAssetSnapshotDto(*audio);
        QVERIFY2((preview && preview.get().validatedOnly && preview.get().changed &&
                  afterPreview == previous && commit && commit.get().changed &&
                  commit.get().current.revision == base.revision + 1 &&
                  committed.path == preparedPath &&
                  committed.pathInfo.sha512 == QStringLiteral("sha512-confirmed") &&
                  committed.formatData == preparedFormat && undo && undo.get().changed &&
                  restored.path == previous.path &&
                  restored.pathInfo.relativeDir == previous.pathInfo.relativeDir &&
                  restored.pathInfo.sha512 == previous.pathInfo.sha512 &&
                  restored.formatData == previous.formatData),
                 qPrintable(QStringLiteral("prepared confirmation must preview without mutation "
                                           "and commit one undoable path triple")));
    };

    {
        // Automation::OperationIds::audio_clips::confirm_path /
        // QStringLiteral("state-commit-preview-no-op-and-wrong-type")

        const auto derived = runtime.project().setAudioClipPathStatus(
            harness.context(), harness.audioClipId(), Automation::audioAssetSnapshotDto(*audio),
            AudioClip::PathStatus::Missing);
        const auto base = runtime.documentVersion();
        const auto preview =
            runtime.project().confirmAudioClipPath(harness.context(true), harness.audioClipId());
        const auto commit =
            runtime.project().confirmAudioClipPath(harness.context(), harness.audioClipId());
        const auto noOp =
            runtime.project().confirmAudioClipPath(harness.context(), harness.audioClipId());
        const auto wrongType =
            runtime.project().confirmAudioClipPath(harness.context(), harness.singingClipId());
        QVERIFY2(
            (derived && preview && preview.get().validatedOnly && commit &&
             commit.get().current.revision == base.revision + 1 && noOp && !noOp.get().changed &&
             audio->pathStatus() == AudioClip::PathStatus::Normal &&
             isError(wrongType, Automation::AutomationErrorCode::WrongObjectType,
                     Automation::OperationIds::audio_clips::confirm_path)),
            qPrintable(QStringLiteral("path confirmation must commit once and type-check clips")));
    };
}

void ApplicationServicesTests::documentAndImportDomains() {
    {
        RuntimeHarness harness;
        QVERIFY2((harness.isReady()),
                 qPrintable(QStringLiteral("document import harness must initialize")));
        auto &runtime = harness.runtime();
        {
            // Automation::OperationIds::documents::commit_import /
            // QStringLiteral("validate-commit-no-op-invalid")

            Automation::DocumentDraftDto draft;
            draft.timeline = harness.model().timeline();
            draft.tracks.append(importedTrack(QStringLiteral("document-import")));
            const auto tracksBefore = harness.model().tracks().size();
            const auto base = runtime.documentVersion();
            const auto preview = runtime.documents().commitImportedDocument(harness.context(true),
                                                                            draft, false, false);
            const auto commit =
                runtime.documents().commitImportedDocument(harness.context(), draft, false, false);
            Automation::DocumentDraftDto emptyImport;
            emptyImport.timeline = harness.model().timeline();
            const auto noOp = runtime.documents().commitImportedDocument(harness.context(),
                                                                         emptyImport, false, false);
            Automation::DocumentDraftDto invalid;
            invalid.timeline = harness.model().timeline();
            auto invalidTrack = importedTrack(QStringLiteral("invalid-import"));
            invalidTrack.gain = std::numeric_limits<double>::quiet_NaN();
            invalid.tracks.append(invalidTrack);
            const auto rejected = runtime.documents().commitImportedDocument(harness.context(),
                                                                             invalid, false, false);
            QVERIFY2(
                (preview && preview.get().validatedOnly && preview.get().createdObjects.isEmpty()),
                qPrintable(QStringLiteral("document import preview must not allocate")));
            QVERIFY2((bool(commit)),
                     qPrintable(commit ? QStringLiteral("document import must return success")
                                       : QStringLiteral("document import failed: %1/%2")
                                             .arg(Automation::errorCodeName(commit.getError().code),
                                                  commit.getError().message)));
            if (commit) {
                QVERIFY2((commit.get().current.revision == base.revision + 1),
                         qPrintable(QStringLiteral("document import must advance one revision")));
                QVERIFY2((commit.get().createdObjects.size() == 2),
                         qPrintable(QStringLiteral("document import created %1 objects, expected 2")
                                        .arg(commit.get().createdObjects.size())));
                QVERIFY2((harness.model().tracks().size() == tracksBefore + 1),
                         qPrintable(QStringLiteral("document import must add one track")));
            }
            QVERIFY2((bool(noOp)),
                     qPrintable(noOp ? QStringLiteral("empty import must return success")
                                     : QStringLiteral("empty import failed: %1/%2")
                                           .arg(Automation::errorCodeName(noOp.getError().code),
                                                noOp.getError().message)));
            if (noOp) {
                QVERIFY2(
                    (!noOp.get().changed),
                    qPrintable(QStringLiteral("empty disabled-timeline import changed state")));
            }
            QVERIFY2((isError(rejected, Automation::AutomationErrorCode::InvalidArgument,
                              Automation::OperationIds::documents::commit_import)),
                     qPrintable(QStringLiteral("invalid document import must fail precommit")));
        };
    }

    {
        RuntimeHarness harness;
        QVERIFY2((harness.isReady()),
                 qPrintable(QStringLiteral("batch import harness must initialize")));
        auto &runtime = harness.runtime();
        {
            // Automation::OperationIds::imports::commit_batch /
            // QStringLiteral("validate-atomic-commit-and-duplicate-ref")

            Automation::BatchImportDraftDto batch;
            batch.timeline = harness.model().timeline();
            Automation::BatchImportItemDraftDto item;
            item.existingTrackId = harness.trackId();
            Automation::ClipDraftDto clip;
            clip.clientRef = QStringLiteral("batch-audio");
            clip.type = Automation::ClipDraftDto::Type::Audio;
            clip.properties.name = QStringLiteral("batch.wav");
            clip.properties.length = 480;
            clip.properties.clipLen = 480;
            clip.properties.gain = 1.0;
            clip.audioPath = QStringLiteral("batch.wav");
            item.clips.append(clip);
            batch.items.append(item);
            const auto base = runtime.documentVersion();
            const auto preview = runtime.project().commitBatchImport(harness.context(true), batch);
            const auto commit = runtime.project().commitBatchImport(harness.context(), batch);

            auto duplicate = batch;
            duplicate.items.first().clips.append(clip);
            const auto rejected = runtime.project().commitBatchImport(harness.context(), duplicate);
            QVERIFY2(
                (preview && preview.get().validatedOnly && preview.get().createdObjects.isEmpty() &&
                 commit && commit.get().current.revision == base.revision + 1 &&
                 commit.get().createdObjects.size() == 1 &&
                 isError(rejected, Automation::AutomationErrorCode::InvalidArgument,
                         Automation::OperationIds::imports::commit_batch)),
                qPrintable(
                    QStringLiteral("batch import must bind once and reject duplicates precommit")));
        };
    }

    {
        RuntimeHarness harness;
        QVERIFY2((harness.isReady()), qPrintable(QStringLiteral("save harness must initialize")));
        auto &runtime = harness.runtime();
        {
            // Automation::OperationIds::documents::save /
            // QStringLiteral("validate-save-failure-and-unavailable")

            const auto path = harness.temporaryPath(QStringLiteral("project.dspx"));
            const auto base = runtime.documentVersion();
            const auto preview = runtime.documents().saveDocument(harness.context(true), path);
            const auto commit = runtime.documents().saveDocument(harness.context(), path);
            const auto snapshot = runtime.documents().getDocument(base.documentId);
            harness.saveSucceeds = false;
            const auto failed = runtime.documents().saveDocument(
                harness.context(), harness.temporaryPath(QStringLiteral("failed.dspx")));
            const auto empty = runtime.documents().saveDocument(harness.context(), {});
            QVERIFY2(
                (preview && preview.get().validatedOnly && harness.saveCount == 2 && commit &&
                 snapshot && snapshot.get().path == path && runtime.documentVersion() == base &&
                 isError(failed, Automation::AutomationErrorCode::IoError,
                         Automation::OperationIds::documents::save) &&
                 isError(empty, Automation::AutomationErrorCode::InvalidArgument,
                         Automation::OperationIds::documents::save)),
                qPrintable(QStringLiteral("save must preserve revision and surface IO failures")));

            RuntimeHarness unavailable({.documentServices = false});
            const auto unavailableSave = unavailable.runtime().documents().saveDocument(
                unavailable.context(),
                unavailable.temporaryPath(QStringLiteral("unavailable.dspx")));
            QVERIFY2((isError(unavailableSave,
                              Automation::AutomationErrorCode::HostCapabilityUnavailable,
                              Automation::OperationIds::documents::save)),
                     qPrintable(QStringLiteral("missing save service must be explicit")));
        };
    }
}

void ApplicationServicesTests::formatsAndMidiExport() {
    using namespace Automation;
    RuntimeHarness harness;
    QVERIFY(harness.isReady());
    auto &runtime = harness.runtime();
    const auto formats = runtime.files().listFormats();
    QVERIFY(formats);
    QCOMPARE(formats.get(), harness.formats);

    RuntimeHarness unavailable({.fileServices = false});
    const auto missingFormats = unavailable.runtime().files().listFormats();
    QVERIFY(
        isError(missingFormats, AutomationErrorCode::ModuleNotReady, OperationIds::formats::list));
    const auto missingExport = unavailable.runtime().files().exportMidi(
        unavailable.context(), unavailable.temporaryPath(QStringLiteral("missing.mid")), false);
    QVERIFY(isError(missingExport, AutomationErrorCode::ModuleNotReady,
                    OperationIds::exports::midi::start));

    const auto path = harness.temporaryPath(QStringLiteral("export.mid"));
    const MidiExportOptionsDto options{.includeTempo = false, .includeTimeSignatures = false};
    const auto before = runtime.documentVersion();
    const auto preview = runtime.files().exportMidi(harness.context(true), path, false, options);
    QVERIFY(preview);
    QVERIFY(preview.get().validatedOnly);
    QVERIFY(!preview.get().wroteFile);
    QVERIFY(!QFileInfo::exists(path));
    QCOMPARE(harness.midiExportCount, 0);

    const auto exported = runtime.files().exportMidi(harness.context(), path, false, options);
    QVERIFY(exported);
    QVERIFY(exported.get().wroteFile);
    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly));
    QCOMPARE(file.readAll(), QByteArray("midi"));
    file.close();
    QCOMPARE(harness.midiExportCount, 1);
    QCOMPARE(harness.lastMidiExportOptions, options);
    QCOMPARE(runtime.documentVersion(), before);

    const auto directPreview = runtime.files().previewMidiExport(before.documentId, path, options);
    QVERIFY(directPreview);
    QVERIFY(directPreview.get().validatedOnly);
    QVERIFY(directPreview.get().modelSnapshot.tracks.isEmpty());
    QCOMPARE(harness.midiExportCount, 1);
    QVERIFY(isError(
        runtime.files().previewMidiExport(before.documentId, QStringLiteral("relative.mid")),
        AutomationErrorCode::InvalidArgument, OperationIds::exports::midi::preview));
    QVERIFY(isError(
        runtime.files().exportMidi(harness.context(), QStringLiteral("relative.mid"), false),
        AutomationErrorCode::InvalidArgument, OperationIds::exports::midi::start));
    QVERIFY(isError(runtime.files().exportMidi(harness.context(),
                                               harness.temporaryPath(QStringLiteral("wrong.wav")),
                                               false),
                    AutomationErrorCode::FormatUnsupported, OperationIds::exports::midi::start));
    QVERIFY(isError(runtime.files().exportMidi(harness.context(), path, false),
                    AutomationErrorCode::OverwriteDenied, OperationIds::exports::midi::start));

    harness.midiExportSucceeds = false;
    QVERIFY(isError(runtime.files().exportMidi(harness.context(),
                                               harness.temporaryPath(QStringLiteral("failed.mid")),
                                               false),
                    AutomationErrorCode::IoError, OperationIds::exports::midi::start));
    QCOMPARE(runtime.documentVersion(), before);
}

void ApplicationServicesTests::preparedMidiPublication_data() {
    QTest::addColumn<bool>("existing");
    QTest::addColumn<bool>("allowOverwrite");
    QTest::addColumn<bool>("validateOnly");
    QTest::addColumn<QString>("publication");
    QTest::newRow("create") << false << false << false << QStringLiteral("write");
    QTest::newRow("replace-existing") << true << true << false << QStringLiteral("write");
    QTest::newRow("validate-without-export") << false << false << true << QStringLiteral("write");
    QTest::newRow("authorization-denies-creation")
        << false << false << false << QStringLiteral("deny");
    QTest::newRow("authorization-preserves-existing")
        << true << true << false << QStringLiteral("deny");
    QTest::newRow("cancel-before-publish") << true << true << false << QStringLiteral("cancel");
    QTest::newRow("target-appears-before-publish")
        << false << false << false << QStringLiteral("race");
    QTest::newRow("backend-failure-preserves-existing")
        << true << true << false << QStringLiteral("fail");
}

void ApplicationServicesTests::preparedMidiPublication() {
    using namespace Automation;
    QFETCH(bool, existing);
    QFETCH(bool, allowOverwrite);
    QFETCH(bool, validateOnly);
    QFETCH(QString, publication);
    RuntimeHarness harness;
    QVERIFY(harness.isReady());
    auto &runtime = harness.runtime();
    const auto before = runtime.documentVersion();
    const auto path = harness.temporaryPath(QStringLiteral("published.mid"));
    QFile target(path);
    if (existing) {
        QVERIFY(target.open(QIODevice::WriteOnly));
        QCOMPARE(target.write("original"), qint64{8});
        target.close();
    }
    const MidiExportOptionsDto options{
        .includeTempo = true,
        .includeTimeSignatures = false,
        .includeLyrics = false,
        .clipIds = {harness.singingClipId()},
    };
    const auto prepared = runtime.files().prepareMidiExport(harness.context(validateOnly), path,
                                                            allowOverwrite, options);
    QVERIFY(prepared);
    if (!validateOnly) {
        QCOMPARE(prepared.get().modelSnapshot.tracks.size(), 1);
        QCOMPARE(prepared.get().modelSnapshot.tracks.first().clips.size(), 1);
        QCOMPARE(prepared.get().modelSnapshot.tracks.first().clips.first().type,
                 ClipDraftDto::Type::Singing);
    }
    harness.midiExportSucceeds = publication != QStringLiteral("fail");
    int authorizationCalls = 0;
    const auto result =
        runtime.files().writePreparedMidiExport(prepared.get(), [&]() -> AutomationResult<bool> {
            ++authorizationCalls;
            if (publication == QStringLiteral("deny")) {
                AutomationError error;
                error.code = AutomationErrorCode::PermissionDenied;
                error.fieldPath = QStringLiteral("path");
                return error;
            }
            if (publication == QStringLiteral("race")) {
                QFile external(path);
                if (!external.open(QIODevice::WriteOnly) || external.write("external") != 8)
                    return AutomationError::invalidArgument(
                        QStringLiteral("path"),
                        QStringLiteral("Failed to create concurrent output"));
            }
            return publication != QStringLiteral("cancel");
        });
    const auto expectedError =
        publication == QStringLiteral("deny")   ? AutomationErrorCode::PermissionDenied
        : publication == QStringLiteral("race") ? AutomationErrorCode::OverwriteDenied
                                                : AutomationErrorCode::IoError;
    const bool failed = publication == QStringLiteral("deny") ||
                        publication == QStringLiteral("race") ||
                        publication == QStringLiteral("fail");
    if (failed) {
        QVERIFY(isError(result, expectedError));
    } else {
        QVERIFY(result);
        QCOMPARE(result.get().validatedOnly, validateOnly);
        QCOMPARE(result.get().wroteFile, !validateOnly && publication == QStringLiteral("write"));
    }
    QCOMPARE(authorizationCalls, validateOnly || publication == QStringLiteral("fail") ? 0 : 1);
    QCOMPARE(harness.midiExportCount, validateOnly ? 0 : 1);
    if (!validateOnly)
        QCOMPARE(harness.lastMidiExportOptions, options);
    const bool wrote = !failed && !validateOnly && publication == QStringLiteral("write");
    if (existing || wrote || publication == QStringLiteral("race")) {
        QVERIFY(target.open(QIODevice::ReadOnly));
        QCOMPARE(target.readAll(), wrote ? QByteArray("midi")
                                   : publication == QStringLiteral("race")
                                       ? QByteArray("external")
                                       : QByteArray("original"));
    } else {
        QVERIFY(!QFileInfo::exists(path));
    }
    QCOMPARE(runtime.documentVersion(), before);
    const QDir directory(QFileInfo(path).absolutePath());
    QVERIFY(
        directory.entryList({QStringLiteral(".ds-editor-lite-midi-*")}, QDir::Files | QDir::Hidden)
            .isEmpty());
}

void ApplicationServicesTests::audioExportRejectsInvalidRequestsAndAllowsCorrection_data() {
    QTest::addColumn<QString>("problem");
    QTest::addColumn<Automation::AutomationErrorCode>("errorCode");
    QTest::addColumn<QString>("configField");
    QTest::newRow("overwrite-needs-consent")
        << QStringLiteral("overwrite") << Automation::AutomationErrorCode::OverwriteDenied
        << QString{};
    QTest::newRow("filename-escapes-export-directory")
        << QStringLiteral("escape") << Automation::AutomationErrorCode::InvalidArgument
        << QString{};
    QTest::newRow("export-directory-is-missing")
        << QStringLiteral("directory") << Automation::AutomationErrorCode::FileNotFound
        << QString{};
    QTest::newRow("nested-output-directory-is-missing")
        << QStringLiteral("nested") << Automation::AutomationErrorCode::FileNotFound << QString{};
    QTest::newRow("output-is-a-directory")
        << QStringLiteral("target") << Automation::AutomationErrorCode::InvalidArgument
        << QString{};
    QTest::newRow("empty-export-filename")
        << QStringLiteral("empty-name") << Automation::AutomationErrorCode::PathRequired
        << QStringLiteral("config.file_name");
    QTest::newRow("invalid-sampling-rate")
        << QStringLiteral("sample-rate") << Automation::AutomationErrorCode::InvalidArgument
        << QStringLiteral("config.sample_rate");
    QTest::newRow("duplicate-export-sources")
        << QStringLiteral("duplicate-sources") << Automation::AutomationErrorCode::InvalidArgument
        << QStringLiteral("config.sources");
}

void ApplicationServicesTests::audioExportRejectsInvalidRequestsAndAllowsCorrection() {
    using namespace Automation;
    QFETCH(QString, problem);
    QFETCH(AutomationErrorCode, errorCode);
    QFETCH(QString, configField);
    RuntimeHarness harness;
    QVERIFY(harness.isReady());
    auto &runtime = harness.runtime();
    const auto before = runtime.documentVersion();
    const auto tasksBefore = runtime.automationTasks().size();
    auto state = harness.audioExportState();
    auto config = audioConfig(harness, QStringLiteral("mix.wav"));
    const auto originalConfig = config;
    if (problem == QStringLiteral("overwrite")) {
        QFile existing(QDir(config.fileDirectory).filePath(config.fileName));
        QVERIFY(existing.open(QIODevice::WriteOnly));
        QCOMPARE(existing.write("original"), qint64{8});
        state->warningFlags = AudioExportWillOverwrite;
    } else if (problem == QStringLiteral("escape")) {
        config.fileName = QStringLiteral("../escaped.wav");
    } else if (problem == QStringLiteral("directory")) {
        config.fileDirectory = QDir(config.fileDirectory).filePath(QStringLiteral("missing"));
    } else if (problem == QStringLiteral("nested")) {
        config.fileName = QStringLiteral("missing/mix.wav");
    } else if (problem == QStringLiteral("empty-name")) {
        config.fileName.clear();
    } else if (problem == QStringLiteral("sample-rate")) {
        config.sampleRate = 0;
    } else if (problem == QStringLiteral("duplicate-sources")) {
        config.sourceOption = 2;
        config.sources = {0, 0};
    } else {
        QVERIFY(QDir().mkdir(QDir(config.fileDirectory).filePath(config.fileName)));
    }
    const auto rejected = runtime.audioExports().start(harness.context(), config, {});
    QVERIFY(isError(rejected, errorCode, OperationIds::exports::audio::start));
    if (!configField.isEmpty()) {
        QCOMPARE(rejected.getError().fieldPath, configField);
        QCOMPARE(state->createCount, 0);
    }
    QCOMPARE(runtime.automationTasks().size(), tasksBefore);
    QCOMPARE(harness.audioScheduler.pendingCount(), 0);
    QCOMPARE(state->executeCount, 0);
    QCOMPARE(state->publishCount, 0);
    QCOMPARE(runtime.documentVersion(), before);
    if (problem == QStringLiteral("target"))
        QVERIFY(QDir().rmdir(QDir(originalConfig.fileDirectory).filePath(originalConfig.fileName)));
    AudioExportPolicyDto policy;
    policy.allowOverwrite = problem == QStringLiteral("overwrite");
    const auto corrected = runtime.audioExports().start(harness.context(), originalConfig, policy);
    QVERIFY(corrected);
    QVERIFY(harness.audioScheduler.runNext());
    const auto completed = runtime.tasks().getTask(before.documentId, corrected.get().taskId);
    QVERIFY(completed);
    QCOMPARE(completed.get().state, AutomationTaskState::Succeeded);
    QCOMPARE(state->executeCount, 1);
    QCOMPARE(state->publishCount, 1);
    QCOMPARE(state->publishAllowOverwrite, std::optional<bool>{policy.allowOverwrite});
    QCOMPARE(runtime.documentVersion(), before);
}

void ApplicationServicesTests::audioExportStageFailuresReleaseResourcesAndAllowRetry_data() {
    QTest::addColumn<bool>("preparing");
    QTest::newRow("preparation-failed") << true;
    QTest::newRow("publication-failed") << false;
}

void ApplicationServicesTests::audioExportStageFailuresReleaseResourcesAndAllowRetry() {
    QFETCH(bool, preparing);
    RuntimeHarness harness;
    QVERIFY(harness.isReady());
    auto &runtime = harness.runtime();
    auto state = harness.audioExportState();
    if (preparing)
        state->readinessState = Automation::AudioExportBackendState::Failed;
    else
        state->publicationState = Automation::AudioExportBackendState::Failed;
    const auto before = runtime.documentVersion();
    const auto config = audioConfig(harness, QStringLiteral("retry.wav"));
    const auto accepted = runtime.audioExports().start(harness.context(), config, {});
    QVERIFY(accepted);
    QVERIFY(harness.audioScheduler.runNext());
    const auto failed = runtime.tasks().getTask(before.documentId, accepted.get().taskId);
    QVERIFY(failed);
    QCOMPARE(failed.get().state, Automation::AutomationTaskState::Failed);
    QVERIFY(failed.get().error);
    QCOMPARE(failed.get().error->code, Automation::AutomationErrorCode::IoError);
    QCOMPARE(failed.get().error->message, state->backendError);
    QCOMPARE(state->waitUntilReadyCount, 1);
    QCOMPARE(state->executeCount, preparing ? 0 : 1);
    QCOMPARE(state->publishCount, preparing ? 0 : 1);
    QCOMPARE(state->cleanupCount, 1);
    QCOMPARE(runtime.documentVersion(), before);
    QVERIFY(runtime.audioExports().cleanup(harness.context(), accepted.get().taskId));
    QCOMPARE(state->cleanupCount, 1);

    state->readinessState = Automation::AudioExportBackendState::Succeeded;
    state->publicationState = Automation::AudioExportBackendState::Succeeded;
    const auto retry = runtime.audioExports().start(harness.context(), config, {});
    QVERIFY(retry);
    QVERIFY(harness.audioScheduler.runNext());
    const auto finished = runtime.tasks().getTask(before.documentId, retry.get().taskId);
    QVERIFY(finished);
    QCOMPARE(finished.get().state, Automation::AutomationTaskState::Succeeded);
    QCOMPARE(state->cleanupCount, 1);
    QCOMPARE(state->publishCount, preparing ? 1 : 2);
    QCOMPARE(runtime.documentVersion(), before);
    QCOMPARE(runtime.tasks().getTask(before.documentId, accepted.get().taskId).get().state,
             Automation::AutomationTaskState::Failed);
}

void ApplicationServicesTests::audioExportAndTaskList() {
    RuntimeHarness harness;
    QVERIFY2((harness.isReady()),
             qPrintable(QStringLiteral("audio export harness must initialize")));
    auto &runtime = harness.runtime();
    const auto config = audioConfig(harness, QStringLiteral("mix.wav"));

    {
        // Automation::OperationIds::exports::audio::preview /
        // QStringLiteral("typed-preview-document-and-service-errors")

        const auto preview =
            runtime.audioExports().preview(runtime.documentVersion().documentId, config);
        const auto wrongDocument =
            runtime.audioExports().preview(Automation::DocumentId::create(), config);
        QVERIFY2((preview && preview.get().filePaths.size() == 1 &&
                  isError(wrongDocument, Automation::AutomationErrorCode::DocumentChanged,
                          Automation::OperationIds::exports::audio::preview)),
                 qPrintable(QStringLiteral("audio preview must route by explicit document")));
        RuntimeHarness unavailable({.audioExportServices = false});
        const auto missing = unavailable.runtime().audioExports().preview(
            unavailable.runtime().documentVersion().documentId,
            audioConfig(unavailable, QStringLiteral("missing.wav")));
        QVERIFY2((!missing),
                 qPrintable(QStringLiteral("missing audio service must return an error result")));
        QVERIFY2((!missing &&
                  missing.getError().code == Automation::AutomationErrorCode::ModuleNotReady),
                 qPrintable(QStringLiteral("missing audio service must use module_not_ready")));
        QVERIFY2((!missing && missing.getError().operationId ==
                                  Automation::OperationIds::exports::audio::preview),
                 qPrintable(QStringLiteral("missing audio preview operation ID is '%1'")
                                .arg(missing ? QStringLiteral("<success>")
                                             : missing.getError().operationId)));
    };

    Automation::TaskId acceptedTask;
    {
        // Automation::OperationIds::exports::audio::start /
        // QStringLiteral("validate-queue-success-warning-and-failure")

        const auto tasksBefore = runtime.automationTasks().size();
        const auto validation = runtime.audioExports().start(harness.context(true), config, {});
        const auto accepted = runtime.audioExports().start(harness.context(), config, {});
        if (accepted)
            acceptedTask = accepted.get().taskId;
        QVERIFY2(
            (validation && validation.get().validatedOnly && validation.get().taskId.isNull() &&
             accepted && !acceptedTask.isNull() &&
             runtime.automationTasks().size() == tasksBefore + 1 &&
             harness.audioScheduler.pendingCount() == 1),
            qPrintable(QStringLiteral("validation must not allocate; start must queue one task")));
        const auto base = runtime.documentVersion();
        const auto activeCleanup = runtime.audioExports().cleanup(harness.context(), acceptedTask);
        const auto queuedTask = runtime.tasks().getTask(base.documentId, acceptedTask);
        QVERIFY2((isError(activeCleanup, Automation::AutomationErrorCode::Busy,
                          Automation::OperationIds::exports::audio::cleanup) &&
                  queuedTask && queuedTask.get().state == Automation::AutomationTaskState::Queued &&
                  harness.audioScheduler.pendingCount() == 1 &&
                  harness.audioExportState()->cleanupCount == 0),
                 "An active export cannot be cleaned up before its worker finishes");
        const auto ran = harness.audioScheduler.runNext();
        const auto terminal = runtime.tasks().getTask(base.documentId, acceptedTask);
        QVERIFY2(
            (ran && terminal &&
             terminal.get().state == Automation::AutomationTaskState::Succeeded &&
             terminal.get().progress.value == 75 && harness.audioExportState()->executeCount == 1 &&
             harness.audioExportState()->deferPublish &&
             harness.audioExportState()->publishAllowOverwrite == false &&
             harness.audioExportState()->cleanupCount == 0 && runtime.documentVersion() == base),
            qPrintable(QStringLiteral("audio export success must retain progress and release "
                                      "its job without cleanup")));

        harness.audioExportState()->warningFlags = Automation::AudioExportLossyFormat;
        auto lossy = audioConfig(harness, QStringLiteral("lossy.ogg"));
        lossy.fileType = 2;
        const auto rejected = runtime.audioExports().start(harness.context(), lossy, {});
        Automation::AudioExportPolicyDto allowLossy;
        allowLossy.allowLossyFormat = true;
        const auto allowed = runtime.audioExports().start(harness.context(), lossy, allowLossy);
        QVERIFY2((isError(rejected, Automation::AutomationErrorCode::InvalidArgument,
                          Automation::OperationIds::exports::audio::start) &&
                  allowed && harness.audioScheduler.pendingCount() == 1),
                 qPrintable(QStringLiteral("warning policy must be explicit before acceptance")));
        if (allowed) {
            runtime.tasks().cancelTask(harness.context(), allowed.get().taskId);
            harness.audioScheduler.runNext();
        }
        QVERIFY2(
            (harness.audioExportState()->cleanupCount == 1),
            qPrintable(QStringLiteral("canceled audio export must force cleanup exactly once")));

        harness.audioExportState()->warningFlags = 0;
        harness.audioExportState()->backendState = Automation::AudioExportBackendState::Failed;
        const auto failedAccepted = runtime.audioExports().start(
            harness.context(), audioConfig(harness, QStringLiteral("failed.wav")), {});
        if (failedAccepted)
            harness.audioScheduler.runNext();
        const auto failedTask =
            failedAccepted ? runtime.tasks().getTask(runtime.documentVersion().documentId,
                                                     failedAccepted.get().taskId)
                           : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                 Automation::AutomationError{});
        QVERIFY2((failedAccepted && failedTask &&
                  failedTask.get().state == Automation::AutomationTaskState::Failed &&
                  failedTask.get().error &&
                  failedTask.get().error->code == Automation::AutomationErrorCode::IoError &&
                  harness.audioExportState()->cleanupCount == 2),
                 qPrintable(
                     QStringLiteral("audio backend failure must remain queryable and force cleanup "
                                    "exactly once")));
    };

    {
        // Automation::OperationIds::exports::audio::start /
        // QStringLiteral("deferred-publication-after-final-authorization")

        auto state = harness.audioExportState();
        state->backendState = Automation::AudioExportBackendState::Succeeded;
        const auto executeBefore = state->executeCount;
        const auto publishBefore = state->publishCount;
        const auto cleanupBefore = state->cleanupCount;
        bool renderFinished = false;
        state->executeHook = [&] { renderFinished = true; };
        const auto denied = runtime.audioExports().start(
            harness.context(), audioConfig(harness, QStringLiteral("denied.wav")), {}, {},
            [&]() -> Automation::AutomationResult<Automation::AutomationUnit> {
                if (!renderFinished)
                    return Automation::AutomationUnit{};
                Automation::AutomationError error;
                error.code = Automation::AutomationErrorCode::PermissionDenied;
                error.message = QStringLiteral("controlled final authorization failure");
                return error;
            });
        const auto deniedRan = harness.audioScheduler.runNext();
        const auto deniedTask =
            denied
                ? runtime.tasks().getTask(runtime.documentVersion().documentId, denied.get().taskId)
                : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                      Automation::AutomationError{});
        QVERIFY2(
            (denied && deniedRan && deniedTask &&
             deniedTask.get().state == Automation::AutomationTaskState::Failed &&
             deniedTask.get().error &&
             deniedTask.get().error->code == Automation::AutomationErrorCode::PermissionDenied &&
             state->executeCount == executeBefore + 1 && state->deferPublish &&
             state->publishCount == publishBefore && state->cleanupCount == cleanupBefore + 1),
            qPrintable(
                QStringLiteral("failed final authorization must discard staged audio without "
                               "publishing")));

        state->executeHook = {};
        state->publishWarning = QStringLiteral("controlled publication warning");
        Automation::AudioExportPolicyDto overwritePolicy;
        overwritePolicy.allowOverwrite = true;
        const auto published = runtime.audioExports().start(
            harness.context(), audioConfig(harness, QStringLiteral("published.wav")),
            overwritePolicy, {}, [] {
                return Automation::AutomationResult<Automation::AutomationUnit>(
                    Automation::AutomationUnit{});
            });
        const auto publishedRan = harness.audioScheduler.runNext();
        const auto publishedTask =
            published ? runtime.tasks().getTask(runtime.documentVersion().documentId,
                                                published.get().taskId)
                      : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                            Automation::AutomationError{});
        QVERIFY2((published && publishedRan && publishedTask &&
                  publishedTask.get().state == Automation::AutomationTaskState::Succeeded &&
                  publishedTask.get().mutation &&
                  publishedTask.get().mutation->warnings.contains(state->publishWarning) &&
                  state->publishCount == publishBefore + 1 &&
                  state->publishAllowOverwrite == true && state->cleanupCount == cleanupBefore + 1),
                 qPrintable(QStringLiteral(
                     "successful final authorization must publish staged audio once")));
    };

    {
        // Automation::OperationIds::exports::audio::start /
        // QStringLiteral("document-edit-during-publication")

        auto state = harness.audioExportState();
        state->backendState = Automation::AudioExportBackendState::Succeeded;
        state->publishWarning.clear();
        const auto executeBefore = state->executeCount;
        const auto publishBefore = state->publishCount;
        const auto cleanupBefore = state->cleanupCount;
        const auto base = runtime.documentVersion();
        bool editedDuringRender = false;
        state->executeHook = [&] {
            const auto edited = runtime.project().renameTrack(
                harness.context(), harness.trackId(), QStringLiteral("Edited During Export"));
            editedDuringRender = edited && edited.get().changed;
        };
        Automation::AudioExportPolicyDto overwritePolicy;
        overwritePolicy.allowOverwrite = true;
        const auto accepted = runtime.audioExports().start(
            RuntimeHarness::contextFor(base),
            audioConfig(harness, QStringLiteral("edited-during-render.wav")), overwritePolicy);
        const auto ran = harness.audioScheduler.runNext();
        state->executeHook = {};
        const auto terminal =
            accepted ? runtime.tasks().getTask(base.documentId, accepted.get().taskId)
                     : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                           Automation::AutomationError{});
        QVERIFY2(
            (accepted && ran && editedDuringRender && terminal &&
             terminal.get().state == Automation::AutomationTaskState::Succeeded &&
             state->executeCount == executeBefore + 1 && state->deferPublish &&
             state->publishCount == publishBefore + 1 && state->cleanupCount == cleanupBefore &&
             runtime.documentVersion().revision == base.revision + 1),
            qPrintable(QStringLiteral("document edits during audio rendering must not discard the "
                                      "completed export")));
    };

    {
        // Automation::OperationIds::tasks::list /
        // QStringLiteral("queued-terminal-and-wrong-document")

        const auto listed = runtime.tasks().listTasks(runtime.documentVersion().documentId);
        const auto wrong = runtime.tasks().listTasks(Automation::DocumentId::create());
        bool containsSucceeded = false;
        if (listed) {
            for (const auto &task : listed.get()) {
                containsSucceeded =
                    containsSucceeded || (task.taskId == acceptedTask &&
                                          task.state == Automation::AutomationTaskState::Succeeded);
            }
        }
        QVERIFY2((listed && containsSucceeded &&
                  isError(wrong, Automation::AutomationErrorCode::DocumentChanged,
                          Automation::OperationIds::tasks::list)),
                 qPrintable(QStringLiteral("task listing must be generation-scoped value data")));
    };

    {
        // Automation::OperationIds::exports::audio::cleanup /
        // QStringLiteral("cleanup-once-and-unknown-task")

        const auto cleanupBefore = harness.audioExportState()->cleanupCount;
        const auto first = runtime.audioExports().cleanup(harness.context(), acceptedTask);
        const auto repeated = runtime.audioExports().cleanup(harness.context(), acceptedTask);
        const auto unknown =
            runtime.audioExports().cleanup(harness.context(), Automation::TaskId::create());
        QVERIFY2((first && !first.get().changed && repeated && !repeated.get().changed &&
                  harness.audioExportState()->cleanupCount == cleanupBefore &&
                  isError(unknown, Automation::AutomationErrorCode::NotFound,
                          Automation::OperationIds::exports::audio::cleanup)),
                 qPrintable(QStringLiteral(
                     "completed jobs must already be released; cleanup remains TaskId-scoped")));
    };
}

void ApplicationServicesTests::extractionDomains() {
    RuntimeHarness harness;
    QVERIFY2((harness.isReady()), qPrintable(QStringLiteral("extraction harness must initialize")));
    auto &runtime = harness.runtime();

    {
        // Automation::OperationIds::extract::pitch::start /
        // QStringLiteral("validate-success-typed-and-service-errors")

        const auto tasksBefore = runtime.automationTasks().size();
        const auto validation = runtime.extractions().startPitch(
            harness.context(true), harness.audioClipId(), harness.singingClipId());
        QVERIFY2(
            (validation && validation.get().validatedOnly && validation.get().taskId.isNull() &&
             runtime.automationTasks().size() == tasksBefore &&
             harness.extractionScheduler.pendingCount() == 0 && !harness.pitchStates.isEmpty() &&
             harness.pitchStates.last()->startCount == 0),
            qPrintable(QStringLiteral("pitch validation must prepare but not allocate or start")));

        const auto base = runtime.documentVersion();
        const auto original = TestSupport::projectSnapshot(harness.model());
        auto pitchContext = harness.context();
        pitchContext.idempotencyKey = QStringLiteral("pitch-extraction");
        pitchContext.clientId = QStringLiteral("extraction-test");
        const auto failedAccepted = runtime.extractions().startPitch(
            pitchContext, harness.audioClipId(), harness.singingClipId());
        QVERIFY(failedAccepted);
        const auto failedState = harness.pitchStates.last();
        QVERIFY(harness.extractionScheduler.runNext());
        failedState->complete({
            .state = Automation::ExtractionBackendState::Failed,
            .errorCode = Automation::AutomationErrorCode::InferenceError,
            .errorMessage = QStringLiteral("controlled pitch inference failure"),
        });
        const auto failedTask =
            runtime.tasks().getTask(base.documentId, failedAccepted.get().taskId);
        QVERIFY(failedTask);
        QCOMPARE(failedTask.get().state, Automation::AutomationTaskState::Failed);
        QVERIFY(failedTask.get().error);
        QCOMPARE(failedTask.get().error->code, Automation::AutomationErrorCode::InferenceError);
        QCOMPARE(runtime.documentVersion(), base);
        QCOMPARE(TestSupport::projectSnapshot(harness.model()), original);

        const auto accepted = runtime.extractions().startPitch(pitchContext, harness.audioClipId(),
                                                               harness.singingClipId());
        QVERIFY(accepted);
        QVERIFY(accepted.get().taskId != failedAccepted.get().taskId);
        const auto prepareCount = harness.pitchPrepareCount;
        const auto repeated = runtime.extractions().startPitch(pitchContext, harness.audioClipId(),
                                                               harness.singingClipId());
        QVERIFY(repeated);
        QCOMPARE(repeated.get(), accepted.get());
        QCOMPARE(harness.pitchPrepareCount, prepareCount);
        const auto state = harness.pitchStates.last();
        const auto ran = harness.extractionScheduler.runNext();
        state->complete({
            .state = Automation::ExtractionBackendState::Succeeded,
            .segments = {{.globalStartTick = 20, .values = {60.0, 60.5}}},
        });
        const auto terminal =
            accepted ? runtime.tasks().getTask(base.documentId, accepted.get().taskId)
                     : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                           Automation::AutomationError{});
        QVERIFY2((accepted && ran && state->startCount == 1 && terminal &&
                  terminal.get().state == Automation::AutomationTaskState::Succeeded &&
                  terminal.get().progress.value == 30 &&
                  runtime.documentVersion().revision == base.revision + 1),
                 qPrintable(QStringLiteral("pitch completion must commit one parameter revision")));
        const auto committed = TestSupport::projectSnapshot(harness.model());
        const auto replay = runtime.extractions().startPitch(pitchContext, harness.audioClipId(),
                                                             harness.singingClipId());
        QVERIFY(replay);
        QCOMPARE(replay.get(), accepted.get());
        QCOMPARE(harness.pitchPrepareCount, prepareCount);
        QCOMPARE(harness.extractionScheduler.pendingCount(), 0);
        QCOMPARE(TestSupport::projectSnapshot(harness.model()), committed);
        QCOMPARE(runtime.documentVersion().revision, base.revision + 1);

        const auto wrongType = runtime.extractions().startPitch(
            harness.context(), harness.singingClipId(), harness.singingClipId());
        QVERIFY2((isError(wrongType, Automation::AutomationErrorCode::WrongObjectType,
                          Automation::OperationIds::extract::pitch::start)),
                 qPrintable(QStringLiteral("pitch source must be an audio clip")));

        Automation::AutomationError prepareError;
        prepareError.code = Automation::AutomationErrorCode::ModuleNotReady;
        prepareError.message = QStringLiteral("controlled pitch unavailable");
        harness.pitchPrepareError = prepareError;
        const auto rejected = runtime.extractions().startPitch(
            harness.context(), harness.audioClipId(), harness.singingClipId());
        harness.pitchPrepareError.reset();
        QVERIFY2((isError(rejected, Automation::AutomationErrorCode::ModuleNotReady,
                          Automation::OperationIds::extract::pitch::start)),
                 qPrintable(QStringLiteral("pitch preparation errors must remain stable")));
    };

    {
        Automation::MidiExtractionOptionsDto overflowingMerge;
        overflowingMerge.destinationMode = QStringLiteral("merge_into_clip");
        overflowingMerge.targetTrackId = harness.trackId();
        overflowingMerge.targetClipId = harness.singingClipId();
        overflowingMerge.targetStart = std::numeric_limits<int>::max();
        const auto mergeBase = runtime.documentVersion();
        const auto mergeAccepted = runtime.extractions().startMidi(
            harness.context(), harness.audioClipId(), overflowingMerge);
        const auto mergeState = harness.midiStates.last();
        const auto mergeRan = harness.extractionScheduler.runNext();
        mergeState->complete({
            .state = Automation::ExtractionBackendState::Succeeded,
            .notes = {{.keyIndex = 62, .localStart = 1, .length = 240}},
        });
        const auto mergeTerminal =
            mergeAccepted
                ? runtime.tasks().getTask(mergeBase.documentId, mergeAccepted.get().taskId)
                : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                      Automation::AutomationError{});
        QVERIFY2(
            (mergeAccepted && mergeRan && mergeTerminal &&
             mergeTerminal.get().state == Automation::AutomationTaskState::Failed &&
             mergeTerminal.get().error &&
             mergeTerminal.get().error->code == Automation::AutomationErrorCode::InvalidArgument &&
             mergeTerminal.get().error->fieldPath == QStringLiteral("destination.start") &&
             runtime.documentVersion() == mergeBase),
            qPrintable(QStringLiteral("MIDI merge must reject translated note ranges that exceed "
                                      "the model tick type")));

        const auto failedAccepted =
            runtime.extractions().startMidi(harness.context(), harness.audioClipId());
        const auto failedState = harness.midiStates.last();
        harness.extractionScheduler.runNext();
        failedState->complete({
            .state = Automation::ExtractionBackendState::Failed,
            .errorCode = Automation::AutomationErrorCode::InferenceError,
            .errorMessage = QStringLiteral("controlled MIDI inference failure"),
        });
        const auto failedTask =
            failedAccepted ? runtime.tasks().getTask(runtime.documentVersion().documentId,
                                                     failedAccepted.get().taskId)
                           : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                 Automation::AutomationError{});
        const auto wrongType =
            runtime.extractions().startMidi(harness.context(), harness.singingClipId());
        QVERIFY2((failedAccepted && failedTask &&
                  failedTask.get().state == Automation::AutomationTaskState::Failed &&
                  failedTask.get().error &&
                  failedTask.get().error->code == Automation::AutomationErrorCode::InferenceError &&
                  isError(wrongType, Automation::AutomationErrorCode::WrongObjectType,
                          Automation::OperationIds::extract::midi::start)),
                 qPrintable(QStringLiteral("MIDI backend and input type errors must be stable")));
    };

    {
        // Automation::OperationIds::extract::pitch::start /
        // QStringLiteral("unavailable-extraction-service")

        RuntimeHarness unavailable({.extractionServices = false});
        const auto result = unavailable.runtime().extractions().startPitch(
            unavailable.context(), unavailable.audioClipId(), unavailable.singingClipId());
        QVERIFY2((isError(result, Automation::AutomationErrorCode::ModuleNotReady,
                          Automation::OperationIds::extract::pitch::start)),
                 qPrintable(QStringLiteral("missing extraction service must be explicit")));
    };
}

void ApplicationServicesTests::midiExtractionDestinations_data() {
    QTest::addColumn<QString>("destinationMode");
    QTest::addColumn<bool>("cancelWhileRunning");
    QTest::addColumn<bool>("backendRequestsCancel");
    QTest::newRow("new-track-queued-cancel") << QString() << false << false;
    QTest::newRow("existing-track-progress-cancel")
        << QStringLiteral("create_clip") << true << true;
    QTest::newRow("existing-clip-running-cancel")
        << QStringLiteral("merge_into_clip") << true << false;
}

void ApplicationServicesTests::midiExtractionDestinations() {
    QFETCH(QString, destinationMode);
    QFETCH(bool, cancelWhileRunning);
    QFETCH(bool, backendRequestsCancel);
    RuntimeHarness harness;
    QVERIFY(harness.isReady());
    auto &runtime = harness.runtime();
    const bool merging = destinationMode == QStringLiteral("merge_into_clip");
    if (merging) {
        QVERIFY(runtime.project().moveClips(
            harness.context(), {
                                   {harness.singingClipId(), harness.trackId(), 120}
        }));
    }
    Automation::MidiExtractionOptionsDto options;
    options.destinationMode = destinationMode;
    options.clientRef = QStringLiteral("extracted");
    options.minimumNoteLength = 120;
    options.targetStart = 720;
    if (!destinationMode.isEmpty())
        options.targetTrackId = harness.trackId();
    if (merging)
        options.targetClipId = harness.singingClipId();

    const auto original = TestSupport::projectSnapshot(harness.model());
    const auto base = runtime.documentVersion();
    const auto tracksBefore = harness.model().tracks().size();
    const auto tasksBefore = runtime.automationTasks().size();
    auto context = harness.context();
    context.idempotencyKey = QStringLiteral("midi-destination");
    context.clientId = QStringLiteral("destination-test");
    auto previewContext = context;
    previewContext.validateOnly = true;
    const auto preview =
        runtime.extractions().startMidi(previewContext, harness.audioClipId(), options);
    QVERIFY(preview);
    QVERIFY(preview.get().validatedOnly);
    QVERIFY(preview.get().taskId.isNull());
    QCOMPARE(runtime.automationTasks().size(), tasksBefore);
    QCOMPARE(harness.extractionScheduler.pendingCount(), 0);
    QCOMPARE(TestSupport::projectSnapshot(harness.model()), original);

    const auto accepted = runtime.extractions().startMidi(context, harness.audioClipId(), options);
    QVERIFY(accepted);
    const auto canceledState = harness.midiStates.last();
    const auto preparedCount = harness.midiPrepareCount;
    const auto repeated = runtime.extractions().startMidi(context, harness.audioClipId(), options);
    QVERIFY(repeated);
    QCOMPARE(repeated.get(), accepted.get());
    QCOMPARE(harness.midiPrepareCount, preparedCount);
    QCOMPARE(harness.extractionScheduler.pendingCount(), 1);
    if (cancelWhileRunning) {
        QVERIFY(harness.extractionScheduler.runNext());
        const auto running = runtime.tasks().getTask(base.documentId, accepted.get().taskId);
        QVERIFY(running);
        QCOMPARE(running.get().state, Automation::AutomationTaskState::Running);
    }
    if (backendRequestsCancel) {
        QVERIFY(canceledState->callbacks.cancelRequested);
        canceledState->callbacks.cancelRequested();
    } else {
        QVERIFY(runtime.tasks().cancelTask(harness.context(), accepted.get().taskId));
    }
    QVERIFY(runtime.tasks().cancelTask(harness.context(), accepted.get().taskId));
    QCOMPARE(canceledState->cancelCount, 1);
    if (cancelWhileRunning) {
        canceledState->complete({
            .state = Automation::ExtractionBackendState::Succeeded,
            .notes = {{.keyIndex = 62, .localStart = 480, .length = 240}},
        });
    } else {
        QVERIFY(harness.extractionScheduler.runNext());
    }
    const auto canceled = runtime.tasks().getTask(base.documentId, accepted.get().taskId);
    QVERIFY(canceled);
    QCOMPARE(canceled.get().state, Automation::AutomationTaskState::Canceled);
    QCOMPARE(canceledState->startCount, cancelWhileRunning ? 1 : 0);
    QCOMPARE(canceledState->destroyCount, 1);
    QCOMPARE(runtime.documentVersion(), base);
    QCOMPARE(TestSupport::projectSnapshot(harness.model()), original);

    const auto retry = runtime.extractions().startMidi(context, harness.audioClipId(), options);
    QVERIFY(retry);
    QVERIFY(retry.get().taskId != accepted.get().taskId);
    QCOMPARE(harness.midiPrepareCount, preparedCount + 1);
    const auto state = harness.midiStates.last();
    QVERIFY(harness.extractionScheduler.runNext());
    QCOMPARE(state->startCount, 1);
    state->complete({
        .state = Automation::ExtractionBackendState::Succeeded,
        .notes = {{.keyIndex = 60, .localStart = -40, .length = 240},
                  {.keyIndex = 60, .localStart = 240, .length = 60},
                  {.keyIndex = 62, .localStart = 480, .length = 240},
                  {.keyIndex = 67, .localStart = 960, .length = 360}},
    });
    const auto terminal = runtime.tasks().getTask(base.documentId, retry.get().taskId);
    QVERIFY(terminal);
    QCOMPARE(terminal.get().state, Automation::AutomationTaskState::Succeeded);
    QCOMPARE(terminal.get().progress.value, 40);
    QVERIFY(terminal.get().mutation);
    QVERIFY(terminal.get().mutation->changed);
    QCOMPARE(runtime.documentVersion().revision, base.revision + 1);
    QCOMPARE(harness.model().tracks().size(), tracksBefore + (destinationMode.isEmpty() ? 1 : 0));

    Automation::ClipId targetClip = harness.singingClipId();
    QMap<QString, Automation::ObjectRef> created;
    for (const auto &object : terminal.get().mutation->createdObjects)
        created.insert(object.clientRef, object.object);
    if (!merging) {
        QVERIFY(created.contains(options.clientRef));
        QCOMPARE(created.value(options.clientRef).kind, Automation::ObjectKind::Clip);
        targetClip = Automation::ClipId(created.value(options.clientRef).value);
    }
    QVERIFY(created.contains(QStringLiteral("extracted/notes/0")));
    QVERIFY(created.contains(QStringLiteral("extracted/notes/1")));
    const auto project = runtime.project().getProject(base.documentId);
    QVERIFY(project);
    const Automation::ClipSnapshotDto *destination = nullptr;
    for (const auto &track : project.get().tracks) {
        for (const auto &clip : track.clips) {
            if (clip.id == targetClip)
                destination = &clip;
        }
    }
    QVERIFY(destination);
    if (!destinationMode.isEmpty())
        QCOMPARE(destination->trackId, harness.trackId());
    QCOMPARE(destination->data.properties.start,
             merging ? 120 : (destinationMode.isEmpty() ? 0 : options.targetStart));
    const auto &notes = destination->data.notes;
    QCOMPARE(notes.size(), merging ? 3 : 2);
    const auto &first = notes.at(notes.size() - 2);
    const auto &second = notes.last();
    QCOMPARE(first.localStart, merging ? 1080 : 480);
    QCOMPARE(first.length, 240);
    QCOMPARE(first.keyIndex, 62);
    QCOMPARE(second.localStart, merging ? 1560 : 960);
    QCOMPARE(second.length, 360);
    QCOMPARE(second.keyIndex, 67);
    QCOMPARE(first.lyric, QStringLiteral("la"));
    QCOMPARE(first.language, QStringLiteral("en"));
    const auto committed = TestSupport::projectSnapshot(harness.model());
    const auto replay = runtime.extractions().startMidi(context, harness.audioClipId(), options);
    QVERIFY(replay);
    QCOMPARE(replay.get(), retry.get());
    QCOMPARE(harness.midiPrepareCount, preparedCount + 1);
    QCOMPARE(harness.extractionScheduler.pendingCount(), 0);
    QCOMPARE(TestSupport::projectSnapshot(harness.model()), committed);
    QVERIFY(runtime.history().undo(harness.context()));
    QCOMPARE(TestSupport::projectSnapshot(harness.model()), original);
    QVERIFY(runtime.history().redo(harness.context()));
    QCOMPARE(TestSupport::projectSnapshot(harness.model()), committed);
}

void ApplicationServicesTests::extractionSourceChangesRejectOldResultsAndAllowRetry_data() {
    QTest::addColumn<bool>("pitch");
    QTest::newRow("pitch-source") << true;
    QTest::newRow("midi-source") << false;
}

void ApplicationServicesTests::extractionSourceChangesRejectOldResultsAndAllowRetry() {
    QFETCH(bool, pitch);
    RuntimeHarness harness;
    QVERIFY(harness.isReady());
    auto &runtime = harness.runtime();
    auto *audio =
        dynamic_cast<AudioClip *>(harness.model().findClipById(harness.audioClipId().value()));
    QVERIFY(audio);
    const auto base = runtime.documentVersion();
    const auto original = TestSupport::projectSnapshot(harness.model());
    const auto sourceBefore = Automation::audioAssetSnapshotDto(*audio);
    const auto *beforeUndo = HistoryManager::instance()->nextUndoEntry();
    const auto tasksBefore = runtime.automationTasks().size();
    bool allowed = false;
    QString authorizedPath;
    const auto authorize =
        [&](const QString &path) -> Automation::AutomationResult<Automation::AutomationUnit> {
        authorizedPath = path;
        if (!allowed) {
            Automation::AutomationError error;
            error.code = Automation::AutomationErrorCode::PermissionDenied;
            error.fieldPath = QStringLiteral("path");
            error.message = QStringLiteral("Source access denied");
            return error;
        }
        return Automation::AutomationUnit{};
    };
    auto context = harness.context();
    context.idempotencyKey = QStringLiteral("source-extraction");
    context.clientId = QStringLiteral("source-test");
    const auto start = [&]() {
        if (pitch) {
            Automation::PitchExtractionOptionsDto options;
            options.authorizeSource = authorize;
            return runtime.extractions().startPitch(context, harness.audioClipId(),
                                                    harness.singingClipId(), options);
        }
        Automation::MidiExtractionOptionsDto options;
        options.authorizeSource = authorize;
        return runtime.extractions().startMidi(context, harness.audioClipId(), options);
    };
    const auto complete = [&] {
        if (pitch) {
            harness.pitchStates.last()->complete({
                .state = Automation::ExtractionBackendState::Succeeded,
                .segments = {{.globalStartTick = 20, .values = {60.0, 60.5}}},
            });
        } else {
            harness.midiStates.last()->complete({
                .state = Automation::ExtractionBackendState::Succeeded,
                .notes = {{.keyIndex = 62, .localStart = 480, .length = 240}},
            });
        }
    };
    const auto denied = start();
    QVERIFY(!denied);
    QCOMPARE(denied.getError().code, Automation::AutomationErrorCode::PermissionDenied);
    QCOMPARE(denied.getError().fieldPath, QStringLiteral("path"));
    QCOMPARE(authorizedPath, sourceBefore.path);
    QCOMPARE(harness.pitchPrepareCount + harness.midiPrepareCount, 0);
    QCOMPARE(harness.extractionScheduler.pendingCount(), 0);
    QCOMPARE(runtime.automationTasks().size(), tasksBefore);
    QCOMPARE(runtime.documentVersion(), base);
    QCOMPARE(TestSupport::projectSnapshot(harness.model()), original);

    allowed = true;
    const auto accepted = start();
    QVERIFY(accepted);
    QVERIFY(harness.extractionScheduler.runNext());
    QCOMPARE(pitch ? harness.pitchStates.last()->input.sourceAsset
                   : harness.midiStates.last()->input.sourceAsset,
             sourceBefore);
    QFile resolved(harness.temporaryPath(QStringLiteral("resolved-source.bin")));
    QVERIFY(resolved.open(QIODevice::WriteOnly));
    const auto bytes = QByteArrayLiteral("controlled extraction source");
    QCOMPARE(resolved.write(bytes), static_cast<qint64>(bytes.size()));
    resolved.close();
    const auto changed = runtime.project().applyResolvedAudioPath(
        harness.context(), harness.audioClipId(), sourceBefore, resolved.fileName(),
        AudioClip::PathStatus::Normal);
    QVERIFY(changed);
    QVERIFY(changed.get().changed);
    QCOMPARE(runtime.documentVersion(), base);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
    const auto sourceAfter = Automation::audioAssetSnapshotDto(*audio);
    QVERIFY(sourceAfter != sourceBefore);
    const auto relocated = TestSupport::projectSnapshot(harness.model());
    complete();
    const auto failed = runtime.tasks().getTask(base.documentId, accepted.get().taskId);
    QVERIFY(failed);
    QCOMPARE(failed.get().state, Automation::AutomationTaskState::Failed);
    QVERIFY(failed.get().error);
    QCOMPARE(failed.get().error->code, Automation::AutomationErrorCode::InvalidArgument);
    QCOMPARE(failed.get().error->fieldPath, QStringLiteral("source_audio_clip_id"));
    QVERIFY(!failed.get().mutation);
    QCOMPARE(runtime.documentVersion(), base);
    QCOMPARE(TestSupport::projectSnapshot(harness.model()), relocated);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);

    const auto retried = start();
    QVERIFY(retried);
    QVERIFY(retried.get().taskId != accepted.get().taskId);
    QCOMPARE(authorizedPath, sourceAfter.path);
    QVERIFY(harness.extractionScheduler.runNext());
    QCOMPARE(pitch ? harness.pitchStates.last()->input.sourceAsset
                   : harness.midiStates.last()->input.sourceAsset,
             sourceAfter);
    complete();
    const auto succeeded = runtime.tasks().getTask(base.documentId, retried.get().taskId);
    QVERIFY(succeeded);
    QCOMPARE(succeeded.get().state, Automation::AutomationTaskState::Succeeded);
    QCOMPARE(runtime.documentVersion().revision, base.revision + 1);
    QVERIFY(runtime.history().undo(harness.context()));
    QCOMPARE(TestSupport::projectSnapshot(harness.model()), relocated);
    QCOMPARE(HistoryManager::instance()->nextUndoEntry(), beforeUndo);
}
