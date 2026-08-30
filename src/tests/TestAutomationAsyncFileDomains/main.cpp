#include "AsyncFileDomainSupport.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include <limits>

namespace {
    using namespace AutomationAsyncFileTests;

    struct InferenceCase {
        Automation::InferenceMutationKind kind;
        Automation::OperationId operationId;
        bool advancesRevision = false;
    };

    [[nodiscard]] QList<InferenceCase> inferenceCases() {
        using Kind = Automation::InferenceMutationKind;
        return {
            {Kind::ApplyPronunciations,   Automation::OperationIds::inference::apply_pronunciations,
             true                                                                                         },
            {Kind::ApplyPhonemeNames,     Automation::OperationIds::inference::apply_phoneme_names,
             true                                                                                         },
            {Kind::ApplyDuration,         Automation::OperationIds::inference::apply_duration,       true },
            {Kind::ApplyPitch,            Automation::OperationIds::inference::apply_pitch,          true },
            {Kind::ApplyVariance,         Automation::OperationIds::inference::apply_variance,       true },
            {Kind::ApplyAcoustic,         Automation::OperationIds::inference::apply_acoustic,       false},
            {Kind::ResetStage,            Automation::OperationIds::inference::reset_stage,          true },
            {Kind::InvalidateClip,        Automation::OperationIds::inference::invalidate_clip,      false},
            {Kind::ResegmentClip,         Automation::OperationIds::inference::resegment_clip,       false},
            {Kind::RefreshSpeakerMix,     Automation::OperationIds::inference::refresh_speaker_mix,
             true                                                                                         },
            {Kind::RefreshParamInput,     Automation::OperationIds::inference::refresh_param_input,
             false                                                                                        },
            {Kind::RebuildOriginalParams,
             Automation::OperationIds::inference::rebuild_original_params,                           true },
        };
    }

    [[nodiscard]] Automation::InferenceMutationRequest
        inferenceRequest(const RuntimeHarness &harness, const InferenceCase &testCase,
                         const int discriminator = 0) {
        Automation::InferenceMutationRequest request;
        request.kind = testCase.kind;
        request.clipId = harness.singingClipId();
        request.pieceId = Automation::PieceId(1000 + discriminator);
        request.pieceIds = {request.pieceId};
        request.noteIds = {harness.noteId()};
        request.parameterName = ParamInfo::Pitch;
        request.pitchSmoothKernelSize = 1;
        request.acousticPath = QStringLiteral("controlled-acoustic-%1.wav").arg(discriminator);
        return request;
    }

    void testInferenceMatrix(Suite &suite) {
        RuntimeHarness harness;
        suite.expect(harness.isReady(), QStringLiteral("inference harness must initialize"));
        auto &runtime = harness.runtime();

        int discriminator = 0;
        for (const auto &testCase : inferenceCases()) {
            suite.run(testCase.operationId, QStringLiteral("validate-success-no-op"), [&] {
                suite.expect(Automation::InferenceAutomationFacade::operationId(testCase.kind) ==
                                 testCase.operationId,
                             QStringLiteral("kind and centralized operation ID must agree"));

                harness.inferenceChanged = true;
                harness.inferenceAdvancesRevision = testCase.advancesRevision;
                const auto request = inferenceRequest(harness, testCase, ++discriminator);
                const auto base = runtime.documentVersion();
                const auto prepareBefore = harness.inferencePrepareCount;
                const auto applyBefore = harness.inferenceApplyCount;

                const auto preview =
                    runtime.inference().applyMutation(harness.context(true), request);
                suite.expect(
                    preview && preview.get().mutation.validatedOnly &&
                        preview.get().mutation.changed && preview.get().mutation.previous == base &&
                        preview.get().mutation.current.documentId == base.documentId &&
                        preview.get().mutation.current.revision ==
                            base.revision + (testCase.advancesRevision ? 1 : 0) &&
                        runtime.documentVersion() == base &&
                        harness.inferencePrepareCount == prepareBefore + 1 &&
                        harness.inferenceApplyCount == applyBefore,
                    QStringLiteral("validate-only must predict without applying or mutating"));

                const auto committed =
                    runtime.inference().applyMutation(harness.context(), request);
                const auto expectedRevision = base.revision + (testCase.advancesRevision ? 1 : 0);
                suite.expect(committed && committed.get().mutation.changed &&
                                 !committed.get().mutation.validatedOnly &&
                                 committed.get().mutation.current.revision == expectedRevision &&
                                 runtime.documentVersion().revision == expectedRevision &&
                                 harness.inferencePrepareCount == prepareBefore + 2 &&
                                 harness.inferenceApplyCount == applyBefore + 1 &&
                                 harness.lastPreparedInferenceKind == testCase.kind &&
                                 harness.lastAppliedInferenceKind == testCase.kind &&
                                 committed.get().sideEffects.changedPieces ==
                                     QList<Automation::PieceId>{request.pieceId},
                             QStringLiteral(
                                 "successful writeback must apply once under its revision policy"));

                harness.inferenceChanged = false;
                const auto noOpBase = runtime.documentVersion();
                const auto noOp = runtime.inference().applyMutation(harness.context(), request);
                suite.expect(noOp && !noOp.get().mutation.changed &&
                                 runtime.documentVersion() == noOpBase &&
                                 harness.inferenceApplyCount == applyBefore + 1,
                             QStringLiteral("legal no-op must not apply or advance revision"));
            });
        }
    }

    void testInferenceValidationBoundaries(Suite &suite) {
        RuntimeHarness harness;
        suite.expect(harness.isReady(),
                     QStringLiteral("inference boundary harness must initialize"));
        auto &runtime = harness.runtime();
        const auto testCase = inferenceCases().at(3);
        auto request = inferenceRequest(harness, testCase, 81);
        const auto initialPrepareCount = harness.inferencePrepareCount;

        suite.run(testCase.operationId, QStringLiteral("document-revision-object-priority"), [&] {
            auto wrongDocument = harness.context();
            wrongDocument.expected.documentId = Automation::DocumentId::create();
            wrongDocument.expected.revision += 10;
            request.clipId = Automation::ClipId(900001);
            const auto rejectedDocument = runtime.inference().applyMutation(wrongDocument, request);
            suite.expect(isError(rejectedDocument, Automation::AutomationErrorCode::DocumentChanged,
                                 testCase.operationId) &&
                             harness.inferencePrepareCount == initialPrepareCount,
                         QStringLiteral("DocumentId must be checked before revision and objects"));

            auto stale = harness.context();
            ++stale.expected.revision;
            const auto rejectedRevision = runtime.inference().applyMutation(stale, request);
            suite.expect(
                isError(rejectedRevision, Automation::AutomationErrorCode::RevisionConflict,
                        testCase.operationId) &&
                    harness.inferencePrepareCount == initialPrepareCount,
                QStringLiteral("revision must be checked before the service resolves objects"));

            request.clipId = Automation::ClipId();
            const auto invalidClip = runtime.inference().applyMutation(harness.context(), request);
            request.clipId = Automation::ClipId(900001);
            const auto missingClip = runtime.inference().applyMutation(harness.context(), request);
            request.clipId = harness.audioClipId();
            const auto wrongType = runtime.inference().applyMutation(harness.context(), request);
            suite.expect(isError(invalidClip, Automation::AutomationErrorCode::InvalidArgument,
                                 testCase.operationId) &&
                             isError(missingClip, Automation::AutomationErrorCode::NotFound,
                                     testCase.operationId) &&
                             isError(wrongType, Automation::AutomationErrorCode::WrongObjectType,
                                     testCase.operationId),
                         QStringLiteral(
                             "service object validation errors must retain stable operation IDs"));

            request.clipId = harness.singingClipId();
            request.pieceId = Automation::PieceId(900002);
            const auto missingPiece = runtime.inference().applyMutation(harness.context(), request);
            request.pieceId = Automation::PieceId(81);
            request.noteIds = {Automation::NoteId(900003)};
            const auto missingNote = runtime.inference().applyMutation(harness.context(), request);
            suite.expect(
                isError(missingPiece, Automation::AutomationErrorCode::NotFound,
                        testCase.operationId) &&
                    isError(missingNote, Automation::AutomationErrorCode::NotFound,
                            testCase.operationId),
                QStringLiteral("piece and note failures must be typed and operation-scoped"));

            Automation::AutomationError backendError;
            backendError.code = Automation::AutomationErrorCode::InferenceError;
            backendError.message = QStringLiteral("controlled inference rejection");
            harness.inferenceError = backendError;
            request.noteIds = {harness.noteId()};
            const auto rejectedBackend =
                runtime.inference().applyMutation(harness.context(), request);
            harness.inferenceError.reset();
            suite.expect(isError(rejectedBackend, Automation::AutomationErrorCode::InferenceError,
                                 testCase.operationId),
                         QStringLiteral("inference backend errors must be preserved"));
        });

        suite.run(testCase.operationId, QStringLiteral("generation-and-sibling-writeback"), [&] {
            request = inferenceRequest(harness, testCase, 91);
            harness.inferenceChanged = true;
            harness.inferenceAdvancesRevision = true;
            const auto sharedBase = runtime.documentVersion();
            const auto first =
                runtime.inference().applyMutation(RuntimeHarness::contextFor(sharedBase), request);
            const auto rebased = Automation::rebaseTaskVersionWithinGeneration(
                sharedBase, runtime.documentVersion());
            auto siblingRequest = request;
            siblingRequest.kind = Automation::InferenceMutationKind::ApplyVariance;
            const auto sibling =
                rebased ? runtime.inference().applyMutation(
                              RuntimeHarness::contextFor(rebased.get()), siblingRequest)
                        : Automation::AutomationResult<Automation::InferenceMutationResultDto>(
                              rebased.getError());
            suite.expect(
                first && rebased && sibling &&
                    runtime.documentVersion().revision == sharedBase.revision + 2,
                QStringLiteral("validated siblings must rebase and commit one revision each"));

            const auto staleGeneration =
                Automation::DocumentVersion{Automation::DocumentId::create(), sharedBase.revision};
            const auto rejectedRebase = Automation::rebaseTaskVersionWithinGeneration(
                staleGeneration, runtime.documentVersion());
            suite.expect(isError(rejectedRebase, Automation::AutomationErrorCode::DocumentChanged),
                         QStringLiteral("sibling rebasing must never cross a document generation"));

            const auto staleContext = RuntimeHarness::contextFor(runtime.documentVersion());
            const auto preparesBeforeReplacement = harness.inferencePrepareCount;
            const auto replacement = runtime.documents().commitNewDocument(
                harness.context(), RuntimeHarness::emptyDocument());
            const auto staleWriteback = runtime.inference().applyMutation(staleContext, request);
            suite.expect(
                replacement &&
                    isError(staleWriteback, Automation::AutomationErrorCode::DocumentChanged,
                            testCase.operationId) &&
                    harness.inferencePrepareCount == preparesBeforeReplacement,
                QStringLiteral("late writeback must not enter services after replacement"));
        });

        suite.run(testCase.operationId, QStringLiteral("unavailable-service"), [&] {
            RuntimeHarness unavailable({.inferenceServices = false});
            suite.expect(unavailable.isReady(),
                         QStringLiteral("unavailable inference harness must initialize"));
            auto unavailableRequest = inferenceRequest(unavailable, testCase, 101);
            const auto result = unavailable.runtime().inference().applyMutation(
                unavailable.context(), unavailableRequest);
            suite.expect(isError(result, Automation::AutomationErrorCode::ModuleNotReady,
                                 testCase.operationId),
                         QStringLiteral("missing inference services must fail deterministically"));
        });
    }

    void testAudioClipDomain(Suite &suite) {
        RuntimeHarness harness;
        suite.expect(harness.isReady(), QStringLiteral("audio clip harness must initialize"));
        auto &runtime = harness.runtime();
        auto *audio =
            dynamic_cast<AudioClip *>(harness.model().findClipById(harness.audioClipId().value()));
        suite.expect(audio != nullptr, QStringLiteral("fixture audio clip must be addressable"));
        if (!audio)
            return;

        suite.run(
            Automation::OperationIds::audio_clips::apply_decode_cache,
            QStringLiteral("derived-preview-commit-no-op-and-stale-path"), [&] {
                AudioInfoModel info;
                info.sampleRate = 48000;
                info.channels = 2;
                info.frames = 96000;
                info.peakCache.append({-20, 20});
                const auto base = runtime.documentVersion();
                const auto asset = Automation::audioAssetSnapshotDto(*audio);
                const auto preview = runtime.project().applyAudioDecodeCache(
                    harness.context(true), harness.audioClipId(), asset, info);
                suite.expect(preview && preview.get().validatedOnly && preview.get().changed &&
                                 runtime.documentVersion() == base &&
                                 audio->audioInfo().sampleRate != 48000,
                             QStringLiteral("decode preview must not install cache data"));
                const auto commit = runtime.project().applyAudioDecodeCache(
                    harness.context(), harness.audioClipId(), asset, info);
                const auto noOp = runtime.project().applyAudioDecodeCache(
                    harness.context(), harness.audioClipId(), asset, info);
                auto staleAsset = asset;
                staleAsset.path = QStringLiteral("stale.wav");
                const auto stale = runtime.project().applyAudioDecodeCache(
                    harness.context(), harness.audioClipId(), staleAsset, info);
                suite.expect(
                    commit && commit.get().changed && noOp && !noOp.get().changed &&
                        runtime.documentVersion() == base &&
                        audio->audioInfo().sampleRate == 48000 &&
                        audio->pathStatus() == AudioClip::PathStatus::Normal &&
                        isError(stale, Automation::AutomationErrorCode::InvalidArgument,
                                Automation::OperationIds::audio_clips::apply_decode_cache),
                    QStringLiteral("decode cache must be derived, no-op aware, and path guarded"));
            });

        suite.run(Automation::OperationIds::audio_clips::set_path_status,
                  QStringLiteral("derived-status-and-validation"), [&] {
                      const auto base = runtime.documentVersion();
                      const auto asset = Automation::audioAssetSnapshotDto(*audio);
                      const auto preview = runtime.project().setAudioClipPathStatus(
                          harness.context(true), harness.audioClipId(), asset,
                          AudioClip::PathStatus::Unconfirmed);
                      const auto commit = runtime.project().setAudioClipPathStatus(
                          harness.context(), harness.audioClipId(), asset,
                          AudioClip::PathStatus::Unconfirmed);
                      const auto noOp = runtime.project().setAudioClipPathStatus(
                          harness.context(), harness.audioClipId(), asset,
                          AudioClip::PathStatus::Unconfirmed);
                      auto staleAsset = asset;
                      staleAsset.path = QStringLiteral("stale.wav");
                      const auto stale = runtime.project().setAudioClipPathStatus(
                          harness.context(), harness.audioClipId(), staleAsset,
                          AudioClip::PathStatus::Missing);
                      suite.expect(
                          preview && preview.get().validatedOnly && commit &&
                              commit.get().changed && noOp && !noOp.get().changed &&
                              audio->pathStatus() == AudioClip::PathStatus::Unconfirmed &&
                              runtime.documentVersion() == base &&
                              isError(stale, Automation::AutomationErrorCode::InvalidArgument,
                                      Automation::OperationIds::audio_clips::set_path_status),
                          QStringLiteral(
                              "path status must update without revision and reject stale paths"));
                  });

        suite.run(
            Automation::OperationIds::audio_clips::set_hash,
            QStringLiteral("derived-hash-and-empty-input"), [&] {
                const auto base = runtime.documentVersion();
                const auto asset = Automation::audioAssetSnapshotDto(*audio);
                const auto preview =
                    runtime.project().setAudioClipHash(harness.context(true), harness.audioClipId(),
                                                       asset, QStringLiteral("sha512-a"));
                const auto commit = runtime.project().setAudioClipHash(
                    harness.context(), harness.audioClipId(), asset, QStringLiteral("sha512-a"));
                const auto updatedAsset = Automation::audioAssetSnapshotDto(*audio);
                const auto noOp =
                    runtime.project().setAudioClipHash(harness.context(), harness.audioClipId(),
                                                       updatedAsset, QStringLiteral("sha512-a"));
                const auto empty = runtime.project().setAudioClipHash(
                    harness.context(), harness.audioClipId(), updatedAsset, {});
                suite.expect(
                    preview && preview.get().validatedOnly && commit && commit.get().changed &&
                        noOp && !noOp.get().changed &&
                        audio->pathInfo().sha512 == QStringLiteral("sha512-a") &&
                        runtime.documentVersion() == base &&
                        isError(empty, Automation::AutomationErrorCode::InvalidArgument,
                                Automation::OperationIds::audio_clips::set_hash),
                    QStringLiteral("hash writeback must be derived and reject empty hashes"));
            });

        suite.run(Automation::OperationIds::audio_clips::apply_resolved_path,
                  QStringLiteral("derived-path-resolution-and-empty-target"), [&] {
                      QTemporaryDir resolvedFiles;
                      const auto resolvedPath =
                          QDir(resolvedFiles.path()).filePath(QStringLiteral("resolved.wav"));
                      QFile resolvedFile(resolvedPath);
                      const bool fixtureReady =
                          resolvedFiles.isValid() &&
                          resolvedFile.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
                          resolvedFile.write("resolved-audio") == 14;
                      suite.expect(fixtureReady,
                                   QStringLiteral("resolved audio fixture must be available"));
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
                      suite.expect(
                          preview && preview.get().validatedOnly && commit &&
                              commit.get().changed && noOp && !noOp.get().changed &&
                              audio->path() == resolvedPath && runtime.documentVersion() == base &&
                              isError(empty, Automation::AutomationErrorCode::InvalidArgument,
                                      Automation::OperationIds::audio_clips::apply_resolved_path),
                          QStringLiteral("resolved path must be snapshot-guarded derived state"));
                  });

        suite.run(
            Automation::OperationIds::audio_clips::relocate,
            QStringLiteral("history-commit-preview-no-op-and-invalid"), [&] {
                AudioPathInfo info;
                info.relativeDir = QStringLiteral("media");
                info.sha512 = QStringLiteral("sha512-b");
                const QJsonObject format{
                    {QStringLiteral("codec"), QStringLiteral("pcm")}
                };
                const auto base = runtime.documentVersion();
                const auto preview = runtime.project().relocateAudioClip(
                    harness.context(true), harness.audioClipId(), QStringLiteral("relocated.wav"),
                    info, format);
                const auto commit = runtime.project().relocateAudioClip(
                    harness.context(), harness.audioClipId(), QStringLiteral("relocated.wav"), info,
                    format);
                const auto noOp = runtime.project().relocateAudioClip(
                    harness.context(), harness.audioClipId(), QStringLiteral("relocated.wav"), info,
                    format);
                const auto empty = runtime.project().relocateAudioClip(
                    harness.context(), harness.audioClipId(), {}, info, format);
                const auto wrongType =
                    runtime.project().relocateAudioClip(harness.context(), harness.singingClipId(),
                                                        QStringLiteral("wrong.wav"), info, format);
                suite.expect(
                    preview && preview.get().validatedOnly && commit &&
                        commit.get().current.revision == base.revision + 1 && noOp &&
                        !noOp.get().changed && audio->path() == QStringLiteral("relocated.wav") &&
                        isError(empty, Automation::AutomationErrorCode::InvalidArgument,
                                Automation::OperationIds::audio_clips::relocate) &&
                        isError(wrongType, Automation::AutomationErrorCode::WrongObjectType,
                                Automation::OperationIds::audio_clips::relocate),
                    QStringLiteral("relocation must be one history revision with typed errors"));
            });

        suite.run(
            Automation::OperationIds::audio_clips::confirm_path,
            QStringLiteral("prepared-path-preview-single-history-commit-and-undo"), [&] {
                const auto previous = Automation::audioAssetSnapshotDto(*audio);
                const auto preparedPath = harness.temporaryPath(QStringLiteral("confirmed.wav"));
                const AudioPathInfo preparedInfo{{}, QStringLiteral("sha512-confirmed")};
                const QJsonObject preparedFormat{
                    {QStringLiteral("entryClassName"), QStringLiteral("PreparedFormatEntry")},
                    {QStringLiteral("userData"),       QStringLiteral("prepared-data")      },
                };
                const auto base = runtime.documentVersion();
                const auto preview = runtime.project().confirmAudioClipPath(
                    harness.context(true), harness.audioClipId(), preparedPath, preparedInfo,
                    preparedFormat);
                const auto afterPreview = Automation::audioAssetSnapshotDto(*audio);
                const auto commit = runtime.project().confirmAudioClipPath(
                    harness.context(), harness.audioClipId(), preparedPath, preparedInfo,
                    preparedFormat);
                const auto committed = Automation::audioAssetSnapshotDto(*audio);
                const auto undo = runtime.history().undo(harness.context());
                const auto restored = Automation::audioAssetSnapshotDto(*audio);
                suite.expect(preview && preview.get().validatedOnly && preview.get().changed &&
                                 afterPreview == previous && commit && commit.get().changed &&
                                 commit.get().current.revision == base.revision + 1 &&
                                 committed.path == preparedPath &&
                                 committed.pathInfo.sha512 == QStringLiteral("sha512-confirmed") &&
                                 committed.formatData == preparedFormat && undo &&
                                 undo.get().changed && restored.path == previous.path &&
                                 restored.pathInfo.relativeDir == previous.pathInfo.relativeDir &&
                                 restored.pathInfo.sha512 == previous.pathInfo.sha512 &&
                                 restored.formatData == previous.formatData,
                             QStringLiteral("prepared confirmation must preview without mutation "
                                            "and commit one undoable path triple"));
            });

        suite.run(
            Automation::OperationIds::audio_clips::confirm_path,
            QStringLiteral("state-commit-preview-no-op-and-wrong-type"), [&] {
                const auto derived = runtime.project().setAudioClipPathStatus(
                    harness.context(), harness.audioClipId(),
                    Automation::audioAssetSnapshotDto(*audio), AudioClip::PathStatus::Missing);
                const auto base = runtime.documentVersion();
                const auto preview = runtime.project().confirmAudioClipPath(harness.context(true),
                                                                            harness.audioClipId());
                const auto commit = runtime.project().confirmAudioClipPath(harness.context(),
                                                                           harness.audioClipId());
                const auto noOp = runtime.project().confirmAudioClipPath(harness.context(),
                                                                         harness.audioClipId());
                const auto wrongType = runtime.project().confirmAudioClipPath(
                    harness.context(), harness.singingClipId());
                suite.expect(
                    derived && preview && preview.get().validatedOnly && commit &&
                        commit.get().current.revision == base.revision + 1 && noOp &&
                        !noOp.get().changed &&
                        audio->pathStatus() == AudioClip::PathStatus::Normal &&
                        isError(wrongType, Automation::AutomationErrorCode::WrongObjectType,
                                Automation::OperationIds::audio_clips::confirm_path),
                    QStringLiteral("path confirmation must commit once and type-check clips"));
            });
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

    void testDocumentAndImportDomains(Suite &suite) {
        {
            RuntimeHarness harness;
            suite.expect(harness.isReady(),
                         QStringLiteral("document import harness must initialize"));
            auto &runtime = harness.runtime();
            suite.run(
                Automation::OperationIds::documents::commit_import,
                QStringLiteral("validate-commit-no-op-invalid"), [&] {
                    Automation::DocumentDraftDto draft;
                    draft.timeline = harness.model().timeline();
                    draft.tracks.append(importedTrack(QStringLiteral("document-import")));
                    const auto tracksBefore = harness.model().tracks().size();
                    const auto base = runtime.documentVersion();
                    const auto preview = runtime.documents().commitImportedDocument(
                        harness.context(true), draft, false, false);
                    const auto commit = runtime.documents().commitImportedDocument(
                        harness.context(), draft, false, false);
                    Automation::DocumentDraftDto emptyImport;
                    emptyImport.timeline = harness.model().timeline();
                    const auto noOp = runtime.documents().commitImportedDocument(
                        harness.context(), emptyImport, false, false);
                    Automation::DocumentDraftDto invalid;
                    invalid.timeline = harness.model().timeline();
                    auto invalidTrack = importedTrack(QStringLiteral("invalid-import"));
                    invalidTrack.gain = std::numeric_limits<double>::quiet_NaN();
                    invalid.tracks.append(invalidTrack);
                    const auto rejected = runtime.documents().commitImportedDocument(
                        harness.context(), invalid, false, false);
                    suite.expect(preview && preview.get().validatedOnly &&
                                     preview.get().createdObjects.isEmpty(),
                                 QStringLiteral("document import preview must not allocate"));
                    suite.expect(bool(commit),
                                 commit
                                     ? QStringLiteral("document import must return success")
                                     : QStringLiteral("document import failed: %1/%2")
                                           .arg(Automation::errorCodeName(commit.getError().code),
                                                commit.getError().message));
                    if (commit) {
                        suite.expect(commit.get().current.revision == base.revision + 1,
                                     QStringLiteral("document import must advance one revision"));
                        suite.expect(
                            commit.get().createdObjects.size() == 2,
                            QStringLiteral("document import created %1 objects, expected 2")
                                .arg(commit.get().createdObjects.size()));
                        suite.expect(harness.model().tracks().size() == tracksBefore + 1,
                                     QStringLiteral("document import must add one track"));
                    }
                    suite.expect(bool(noOp),
                                 noOp ? QStringLiteral("empty import must return success")
                                      : QStringLiteral("empty import failed: %1/%2")
                                            .arg(Automation::errorCodeName(noOp.getError().code),
                                                 noOp.getError().message));
                    if (noOp) {
                        suite.expect(
                            !noOp.get().changed,
                            QStringLiteral("empty disabled-timeline import changed state"));
                    }
                    suite.expect(isError(rejected, Automation::AutomationErrorCode::InvalidArgument,
                                         Automation::OperationIds::documents::commit_import),
                                 QStringLiteral("invalid document import must fail precommit"));
                });
        }

        {
            RuntimeHarness harness;
            suite.expect(harness.isReady(), QStringLiteral("batch import harness must initialize"));
            auto &runtime = harness.runtime();
            suite.run(Automation::OperationIds::imports::commit_batch,
                      QStringLiteral("validate-atomic-commit-and-duplicate-ref"), [&] {
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
                          const auto preview =
                              runtime.project().commitBatchImport(harness.context(true), batch);
                          const auto commit =
                              runtime.project().commitBatchImport(harness.context(), batch);

                          auto duplicate = batch;
                          duplicate.items.first().clips.append(clip);
                          const auto rejected =
                              runtime.project().commitBatchImport(harness.context(), duplicate);
                          suite.expect(
                              preview && preview.get().validatedOnly &&
                                  preview.get().createdObjects.isEmpty() && commit &&
                                  commit.get().current.revision == base.revision + 1 &&
                                  commit.get().createdObjects.size() == 1 &&
                                  isError(rejected,
                                          Automation::AutomationErrorCode::InvalidArgument,
                                          Automation::OperationIds::imports::commit_batch),
                              QStringLiteral(
                                  "batch import must bind once and reject duplicates precommit"));
                      });
        }

        {
            RuntimeHarness harness;
            suite.expect(harness.isReady(), QStringLiteral("save harness must initialize"));
            auto &runtime = harness.runtime();
            suite.run(
                Automation::OperationIds::documents::save,
                QStringLiteral("validate-save-failure-and-unavailable"), [&] {
                    const auto path = harness.temporaryPath(QStringLiteral("project.dspx"));
                    const auto base = runtime.documentVersion();
                    const auto preview =
                        runtime.documents().saveDocument(harness.context(true), path);
                    const auto commit = runtime.documents().saveDocument(harness.context(), path);
                    const auto snapshot = runtime.documents().getDocument(base.documentId);
                    harness.saveSucceeds = false;
                    const auto failed = runtime.documents().saveDocument(
                        harness.context(), harness.temporaryPath(QStringLiteral("failed.dspx")));
                    const auto empty = runtime.documents().saveDocument(harness.context(), {});
                    suite.expect(
                        preview && preview.get().validatedOnly && harness.saveCount == 2 &&
                            commit && snapshot && snapshot.get().path == path &&
                            runtime.documentVersion() == base &&
                            isError(failed, Automation::AutomationErrorCode::IoError,
                                    Automation::OperationIds::documents::save) &&
                            isError(empty, Automation::AutomationErrorCode::InvalidArgument,
                                    Automation::OperationIds::documents::save),
                        QStringLiteral("save must preserve revision and surface IO failures"));

                    RuntimeHarness unavailable({.documentServices = false});
                    const auto unavailableSave = unavailable.runtime().documents().saveDocument(
                        unavailable.context(),
                        unavailable.temporaryPath(QStringLiteral("unavailable.dspx")));
                    suite.expect(isError(unavailableSave,
                                         Automation::AutomationErrorCode::HostCapabilityUnavailable,
                                         Automation::OperationIds::documents::save),
                                 QStringLiteral("missing save service must be explicit"));
                });
        }
    }

    void testFormatsAndMidiExport(Suite &suite) {
        RuntimeHarness harness;
        suite.expect(harness.isReady(), QStringLiteral("file harness must initialize"));
        auto &runtime = harness.runtime();

        suite.run(
            Automation::OperationIds::formats::list, QStringLiteral("success-and-unavailable"),
            [&] {
                const auto formats = runtime.files().listFormats();
                suite.expect(formats && formats.get() == harness.formats &&
                                 formats.get().size() == 2,
                             QStringLiteral("format query must return a typed value snapshot"));
                RuntimeHarness unavailable({.fileServices = false});
                const auto missing = unavailable.runtime().files().listFormats();
                Automation::AutomationError directError;
                directError.code = Automation::AutomationErrorCode::ModuleNotReady;
                const auto direct =
                    unavailable.runtime()
                        .dispatcher()
                        .dispatchApplicationQuery<QList<Automation::ProjectFormatDto>>(
                            Automation::OperationIds::formats::list, [directError] {
                                return Automation::AutomationResult<
                                    QList<Automation::ProjectFormatDto>>(directError);
                            });
                suite.expect(!missing,
                             QStringLiteral("missing format service must return an error result"));
                suite.expect(!missing && missing.getError().code ==
                                             Automation::AutomationErrorCode::ModuleNotReady,
                             QStringLiteral("missing format service must use module_not_ready"));
                suite.expect(!missing && missing.getError().operationId ==
                                             Automation::OperationIds::formats::list,
                             QStringLiteral("missing format operation ID is '%1'")
                                 .arg(missing ? QStringLiteral("<success>")
                                              : missing.getError().operationId));
                suite.expect(
                    !direct &&
                        direct.getError().operationId == Automation::OperationIds::formats::list,
                    QStringLiteral("direct query operation ID is '%1'")
                        .arg(direct ? QStringLiteral("<success>") : direct.getError().operationId));
            });

        suite.run(
            Automation::OperationIds::exports::midi::start,
            QStringLiteral("validate-write-and-path-errors"), [&] {
                const auto path = harness.temporaryPath(QStringLiteral("export.mid"));
                auto previewContext = harness.context();
                previewContext.validateOnly = true;
                const Automation::MidiExportOptionsDto options{
                    .includeTempo = false,
                    .includeTimeSignatures = false,
                };
                const auto base = runtime.documentVersion();
                const auto preview =
                    runtime.files().exportMidi(previewContext, path, false, options);
                const auto commit =
                    runtime.files().exportMidi(harness.context(), path, false, options);
                suite.expect(
                    preview && preview.get().validatedOnly && !preview.get().wroteFile && commit &&
                        commit.get().wroteFile && harness.midiExportCount == 1 &&
                        harness.lastMidiExportOptions == options &&
                        runtime.documentVersion() == base,
                    QStringLiteral("MIDI export must validate, forward options, and write once"));

                const Automation::MidiExportOptionsDto preparedOptions{
                    .includeTempo = true,
                    .includeTimeSignatures = false,
                    .includeLyrics = false,
                    .clipIds = {harness.singingClipId()},
                };
                const auto directPreview = runtime.files().previewMidiExport(
                    runtime.documentVersion().documentId,
                    harness.temporaryPath(QStringLiteral("preview.mid")), preparedOptions);
                suite.expect(directPreview && directPreview.get().validatedOnly &&
                                 directPreview.get().modelSnapshot.tracks.isEmpty() &&
                                 harness.midiExportCount == 1,
                             QStringLiteral("MIDI preview must validate without writing a file"));
                const auto prepared = runtime.files().prepareMidiExport(
                    harness.context(), harness.temporaryPath(QStringLiteral("prepared.mid")), false,
                    preparedOptions);
                const auto preparedWrite =
                    prepared ? runtime.files().writePreparedMidiExport(prepared.get())
                             : Automation::AutomationResult<Automation::FileWriteResultDto>(
                                   prepared.getError());
                suite.expect(prepared && prepared.get().modelSnapshot.tracks.size() == 1 &&
                                 prepared.get().modelSnapshot.tracks.first().clips.size() == 1 &&
                                 prepared.get().modelSnapshot.tracks.first().clips.first().type ==
                                     Automation::ClipDraftDto::Type::Singing &&
                                 preparedWrite && preparedWrite.get().wroteFile &&
                                 harness.midiExportCount == 2 &&
                                 harness.lastMidiExportOptions == preparedOptions,
                             QStringLiteral("prepared MIDI export must freeze a restorable model "
                                            "and preserve options"));

                Automation::AutomationError publishDenied;
                publishDenied.code = Automation::AutomationErrorCode::PermissionDenied;
                publishDenied.fieldPath = QStringLiteral("path");
                publishDenied.message = QStringLiteral("controlled final authorization failure");
                const auto deniedNewPath =
                    harness.temporaryPath(QStringLiteral("denied-new.mid"));
                const auto deniedNewPrepared = runtime.files().prepareMidiExport(
                    harness.context(), deniedNewPath, false, preparedOptions);
                bool checkedNewPublish = false;
                const auto deniedNewWrite =
                    deniedNewPrepared
                        ? runtime.files().writePreparedMidiExport(
                              deniedNewPrepared.get(), [&] {
                                  checkedNewPublish = true;
                                  return Automation::AutomationResult<bool>(publishDenied);
                              })
                        : Automation::AutomationResult<Automation::FileWriteResultDto>(
                              deniedNewPrepared.getError());

                const auto preservedPath =
                    harness.temporaryPath(QStringLiteral("denied-overwrite.mid"));
                QFile preservedTarget(preservedPath);
                const auto preservedCreated = preservedTarget.open(QIODevice::WriteOnly) &&
                                              preservedTarget.write("original") == 8;
                preservedTarget.close();
                const auto deniedOverwritePrepared = runtime.files().prepareMidiExport(
                    harness.context(), preservedPath, true, preparedOptions);
                bool checkedOverwritePublish = false;
                const auto deniedOverwriteWrite =
                    deniedOverwritePrepared
                        ? runtime.files().writePreparedMidiExport(
                              deniedOverwritePrepared.get(), [&] {
                                  checkedOverwritePublish = true;
                                  return Automation::AutomationResult<bool>(publishDenied);
                              })
                        : Automation::AutomationResult<Automation::FileWriteResultDto>(
                              deniedOverwritePrepared.getError());
                const auto preservedReadable = preservedTarget.open(QIODevice::ReadOnly);
                const auto preservedContents = preservedTarget.readAll();
                preservedTarget.close();
                suite.expect(
                    deniedNewPrepared && checkedNewPublish &&
                        isError(deniedNewWrite,
                                Automation::AutomationErrorCode::PermissionDenied) &&
                        !QFileInfo::exists(deniedNewPath) && deniedOverwritePrepared &&
                        checkedOverwritePublish &&
                        isError(deniedOverwriteWrite,
                                Automation::AutomationErrorCode::PermissionDenied) &&
                        preservedCreated && preservedReadable && preservedContents == "original",
                    QStringLiteral("failed final authorization must neither create nor replace the "
                                   "MIDI target"));

                const auto racedPath = harness.temporaryPath(QStringLiteral("raced.mid"));
                const auto racedPrepared = runtime.files().prepareMidiExport(
                    harness.context(), racedPath, false, preparedOptions);
                const auto racedWrite =
                    racedPrepared
                        ? runtime.files().writePreparedMidiExport(racedPrepared.get(), [&] {
                              QFile racedTarget(racedPath);
                              if (!racedTarget.open(QIODevice::WriteOnly) ||
                                  racedTarget.write("external") != 8) {
                                  return Automation::AutomationResult<bool>(
                                      Automation::AutomationError::invalidArgument(
                                          QStringLiteral("path"),
                                          QStringLiteral("race fixture could not create target")));
                              }
                              return Automation::AutomationResult<bool>(true);
                          })
                        : Automation::AutomationResult<Automation::FileWriteResultDto>(
                              racedPrepared.getError());
                QFile racedTarget(racedPath);
                const auto racedReadable = racedTarget.open(QIODevice::ReadOnly);
                const auto racedContents = racedTarget.readAll();
                suite.expect(
                    racedPrepared &&
                        isError(racedWrite, Automation::AutomationErrorCode::OverwriteDenied) &&
                        racedReadable && racedContents == "external",
                    QStringLiteral("reject-overwrite publication must not replace a target created "
                                   "after staging"));

                const auto invalidPreview = runtime.files().previewMidiExport(
                    runtime.documentVersion().documentId, QStringLiteral("relative.mid"));
                suite.expect(isError(invalidPreview,
                                     Automation::AutomationErrorCode::InvalidArgument,
                                     Automation::OperationIds::exports::midi::preview),
                             QStringLiteral(
                                 "MIDI preview validation must retain its own operation identity"));

                harness.midiExportSucceeds = false;
                const auto backendFailure = runtime.files().exportMidi(
                    harness.context(), harness.temporaryPath(QStringLiteral("failure.mid")), false);
                const auto relative = runtime.files().exportMidi(
                    harness.context(), QStringLiteral("relative.mid"), false);
                const auto suffix = runtime.files().exportMidi(
                    harness.context(), harness.temporaryPath(QStringLiteral("wrong.wav")), false);
                QFile existing(harness.temporaryPath(QStringLiteral("existing.mid")));
                const auto created = existing.open(QIODevice::WriteOnly);
                existing.close();
                const auto overwrite =
                    runtime.files().exportMidi(harness.context(), existing.fileName(), false);
                suite.expect(isError(backendFailure, Automation::AutomationErrorCode::IoError,
                                     Automation::OperationIds::exports::midi::start) &&
                                 isError(relative, Automation::AutomationErrorCode::InvalidArgument,
                                         Automation::OperationIds::exports::midi::start) &&
                                 isError(suffix, Automation::AutomationErrorCode::FormatUnsupported,
                                         Automation::OperationIds::exports::midi::start) &&
                                 created &&
                                 isError(overwrite,
                                         Automation::AutomationErrorCode::OverwriteDenied,
                                         Automation::OperationIds::exports::midi::start),
                             QStringLiteral("MIDI export must preserve backend and path errors"));

                RuntimeHarness unavailable({.fileServices = false});
                const auto missing = unavailable.runtime().files().exportMidi(
                    unavailable.context(), unavailable.temporaryPath(QStringLiteral("missing.mid")),
                    false);
                suite.expect(isError(missing, Automation::AutomationErrorCode::ModuleNotReady,
                                     Automation::OperationIds::exports::midi::start),
                             QStringLiteral("missing MIDI service must fail before writing"));
            });
    }

    [[nodiscard]] Automation::AudioExportConfigDto audioConfig(const RuntimeHarness &harness,
                                                               const QString &name) {
        Automation::AudioExportConfigDto config;
        config.fileName = name;
        config.fileDirectory = harness.temporaryDirectoryPath();
        return config;
    }

    void testAudioExportAndTaskList(Suite &suite) {
        RuntimeHarness harness;
        suite.expect(harness.isReady(), QStringLiteral("audio export harness must initialize"));
        auto &runtime = harness.runtime();
        const auto config = audioConfig(harness, QStringLiteral("mix.wav"));

        suite.run(
            Automation::OperationIds::exports::audio::preview,
            QStringLiteral("typed-preview-document-and-service-errors"), [&] {
                const auto preview =
                    runtime.audioExports().preview(runtime.documentVersion().documentId, config);
                const auto wrongDocument =
                    runtime.audioExports().preview(Automation::DocumentId::create(), config);
                suite.expect(preview && preview.get().filePaths.size() == 1 &&
                                 isError(wrongDocument,
                                         Automation::AutomationErrorCode::DocumentChanged,
                                         Automation::OperationIds::exports::audio::preview),
                             QStringLiteral("audio preview must route by explicit document"));
                RuntimeHarness unavailable({.audioExportServices = false});
                const auto missing = unavailable.runtime().audioExports().preview(
                    unavailable.runtime().documentVersion().documentId,
                    audioConfig(unavailable, QStringLiteral("missing.wav")));
                suite.expect(!missing,
                             QStringLiteral("missing audio service must return an error result"));
                suite.expect(!missing && missing.getError().code ==
                                             Automation::AutomationErrorCode::ModuleNotReady,
                             QStringLiteral("missing audio service must use module_not_ready"));
                suite.expect(!missing && missing.getError().operationId ==
                                             Automation::OperationIds::exports::audio::preview,
                             QStringLiteral("missing audio preview operation ID is '%1'")
                                 .arg(missing ? QStringLiteral("<success>")
                                              : missing.getError().operationId));
            });

        Automation::TaskId acceptedTask;
        suite.run(
            Automation::OperationIds::exports::audio::start,
            QStringLiteral("validate-queue-success-warning-and-failure"), [&] {
                const auto tasksBefore = runtime.automationTasks().size();
                const auto validation =
                    runtime.audioExports().start(harness.context(true), config, {});
                const auto accepted = runtime.audioExports().start(harness.context(), config, {});
                if (accepted)
                    acceptedTask = accepted.get().taskId;
                suite.expect(
                    validation && validation.get().validatedOnly &&
                        validation.get().taskId.isNull() && accepted && !acceptedTask.isNull() &&
                        runtime.automationTasks().size() == tasksBefore + 1 &&
                        harness.audioScheduler.pendingCount() == 1,
                    QStringLiteral("validation must not allocate; start must queue one task"));
                const auto base = runtime.documentVersion();
                const auto ran = harness.audioScheduler.runNext();
                const auto terminal = runtime.tasks().getTask(base.documentId, acceptedTask);
                suite.expect(ran && terminal &&
                                 terminal.get().state ==
                                     Automation::AutomationTaskState::Succeeded &&
                                 terminal.get().progress.value == 75 &&
                                 harness.audioExportState()->executeCount == 1 &&
                                 harness.audioExportState()->deferPublish &&
                                 harness.audioExportState()->publishAllowOverwrite == false &&
                                 harness.audioExportState()->cleanupCount == 0 &&
                                 runtime.documentVersion() == base,
                             QStringLiteral("audio export success must retain progress and release "
                                            "its job without cleanup"));

                harness.audioExportState()->warningFlags = Automation::AudioExportLossyFormat;
                auto lossy = audioConfig(harness, QStringLiteral("lossy.ogg"));
                lossy.fileType = 2;
                const auto rejected = runtime.audioExports().start(harness.context(), lossy, {});
                Automation::AudioExportPolicyDto allowLossy;
                allowLossy.allowLossyFormat = true;
                const auto allowed =
                    runtime.audioExports().start(harness.context(), lossy, allowLossy);
                suite.expect(isError(rejected, Automation::AutomationErrorCode::InvalidArgument,
                                     Automation::OperationIds::exports::audio::start) &&
                                 allowed && harness.audioScheduler.pendingCount() == 1,
                             QStringLiteral("warning policy must be explicit before acceptance"));
                if (allowed) {
                    runtime.tasks().cancelTask(harness.context(), allowed.get().taskId);
                    harness.audioScheduler.runNext();
                }
                suite.expect(
                    harness.audioExportState()->cleanupCount == 1,
                    QStringLiteral("canceled audio export must force cleanup exactly once"));

                harness.audioExportState()->warningFlags = 0;
                harness.audioExportState()->backendState =
                    Automation::AudioExportBackendState::Failed;
                const auto failedAccepted = runtime.audioExports().start(
                    harness.context(), audioConfig(harness, QStringLiteral("failed.wav")), {});
                if (failedAccepted)
                    harness.audioScheduler.runNext();
                const auto failedTask =
                    failedAccepted
                        ? runtime.tasks().getTask(runtime.documentVersion().documentId,
                                                  failedAccepted.get().taskId)
                        : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                              Automation::AutomationError{});
                suite.expect(
                    failedAccepted && failedTask &&
                        failedTask.get().state == Automation::AutomationTaskState::Failed &&
                        failedTask.get().error &&
                        failedTask.get().error->code == Automation::AutomationErrorCode::IoError &&
                        harness.audioExportState()->cleanupCount == 2,
                    QStringLiteral("audio backend failure must remain queryable and force cleanup "
                                   "exactly once"));
            });

        suite.run(
            Automation::OperationIds::exports::audio::start,
            QStringLiteral("deferred-publication-after-final-authorization"), [&] {
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
                    denied ? runtime.tasks().getTask(runtime.documentVersion().documentId,
                                                     denied.get().taskId)
                           : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                 Automation::AutomationError{});
                suite.expect(
                    denied && deniedRan && deniedTask &&
                        deniedTask.get().state == Automation::AutomationTaskState::Failed &&
                        deniedTask.get().error &&
                        deniedTask.get().error->code ==
                            Automation::AutomationErrorCode::PermissionDenied &&
                        state->executeCount == executeBefore + 1 && state->deferPublish &&
                        state->publishCount == publishBefore &&
                        state->cleanupCount == cleanupBefore + 1,
                    QStringLiteral("failed final authorization must discard staged audio without "
                                   "publishing"));

                state->executeHook = {};
                Automation::AudioExportPolicyDto overwritePolicy;
                overwritePolicy.allowOverwrite = true;
                const auto published = runtime.audioExports().start(
                    harness.context(), audioConfig(harness, QStringLiteral("published.wav")),
                    overwritePolicy, {},
                    [] { return Automation::AutomationResult<Automation::AutomationUnit>(
                             Automation::AutomationUnit{}); });
                const auto publishedRan = harness.audioScheduler.runNext();
                const auto publishedTask =
                    published ? runtime.tasks().getTask(runtime.documentVersion().documentId,
                                                        published.get().taskId)
                              : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                    Automation::AutomationError{});
                suite.expect(
                    published && publishedRan && publishedTask &&
                        publishedTask.get().state == Automation::AutomationTaskState::Succeeded &&
                        state->publishCount == publishBefore + 1 &&
                        state->publishAllowOverwrite == true &&
                        state->cleanupCount == cleanupBefore + 1,
                    QStringLiteral("successful final authorization must publish staged audio once"));
            });

        suite.run(
            Automation::OperationIds::tasks::list,
            QStringLiteral("queued-terminal-and-wrong-document"), [&] {
                const auto listed = runtime.tasks().listTasks(runtime.documentVersion().documentId);
                const auto wrong = runtime.tasks().listTasks(Automation::DocumentId::create());
                bool containsSucceeded = false;
                if (listed) {
                    for (const auto &task : listed.get()) {
                        containsSucceeded =
                            containsSucceeded ||
                            (task.taskId == acceptedTask &&
                             task.state == Automation::AutomationTaskState::Succeeded);
                    }
                }
                suite.expect(listed && containsSucceeded &&
                                 isError(wrong, Automation::AutomationErrorCode::DocumentChanged,
                                         Automation::OperationIds::tasks::list),
                             QStringLiteral("task listing must be generation-scoped value data"));
            });

        suite.run(
            Automation::OperationIds::exports::audio::cleanup,
            QStringLiteral("cleanup-once-and-unknown-task"), [&] {
                const auto first = runtime.audioExports().cleanup(harness.context(), acceptedTask);
                const auto repeated =
                    runtime.audioExports().cleanup(harness.context(), acceptedTask);
                const auto unknown =
                    runtime.audioExports().cleanup(harness.context(), Automation::TaskId::create());
                suite.expect(
                    first && !first.get().changed && repeated && !repeated.get().changed &&
                        harness.audioExportState()->cleanupCount == 3 &&
                        isError(unknown, Automation::AutomationErrorCode::NotFound,
                                Automation::OperationIds::exports::audio::cleanup),
                    QStringLiteral(
                        "completed jobs must already be released; cleanup remains TaskId-scoped"));
            });
    }

    void testExtractionDomains(Suite &suite) {
        RuntimeHarness harness;
        suite.expect(harness.isReady(), QStringLiteral("extraction harness must initialize"));
        auto &runtime = harness.runtime();

        suite.run(
            Automation::OperationIds::extract::pitch::start,
            QStringLiteral("validate-success-typed-and-service-errors"), [&] {
                const auto tasksBefore = runtime.automationTasks().size();
                const auto validation = runtime.extractions().startPitch(
                    harness.context(true), harness.audioClipId(), harness.singingClipId());
                suite.expect(
                    validation && validation.get().validatedOnly &&
                        validation.get().taskId.isNull() &&
                        runtime.automationTasks().size() == tasksBefore &&
                        harness.extractionScheduler.pendingCount() == 0 &&
                        !harness.pitchStates.isEmpty() &&
                        harness.pitchStates.last()->startCount == 0,
                    QStringLiteral("pitch validation must prepare but not allocate or start"));

                const auto base = runtime.documentVersion();
                const auto accepted = runtime.extractions().startPitch(
                    harness.context(), harness.audioClipId(), harness.singingClipId());
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
                suite.expect(accepted && ran && state->startCount == 1 && terminal &&
                                 terminal.get().state ==
                                     Automation::AutomationTaskState::Succeeded &&
                                 terminal.get().progress.value == 30 &&
                                 runtime.documentVersion().revision == base.revision + 1,
                             QStringLiteral("pitch completion must commit one parameter revision"));

                const auto wrongType = runtime.extractions().startPitch(
                    harness.context(), harness.singingClipId(), harness.singingClipId());
                suite.expect(isError(wrongType, Automation::AutomationErrorCode::WrongObjectType,
                                     Automation::OperationIds::extract::pitch::start),
                             QStringLiteral("pitch source must be an audio clip"));

                Automation::AutomationError prepareError;
                prepareError.code = Automation::AutomationErrorCode::ModuleNotReady;
                prepareError.message = QStringLiteral("controlled pitch unavailable");
                harness.pitchPrepareError = prepareError;
                const auto rejected = runtime.extractions().startPitch(
                    harness.context(), harness.audioClipId(), harness.singingClipId());
                harness.pitchPrepareError.reset();
                suite.expect(isError(rejected, Automation::AutomationErrorCode::ModuleNotReady,
                                     Automation::OperationIds::extract::pitch::start),
                             QStringLiteral("pitch preparation errors must remain stable"));
            });

        suite.run(Automation::OperationIds::extract::midi::start,
                  QStringLiteral("success-backend-failure-and-typed-input"), [&] {
                      const auto tracksBefore = harness.model().tracks().size();
                      const auto base = runtime.documentVersion();
                      const auto accepted =
                          runtime.extractions().startMidi(harness.context(), harness.audioClipId());
                      const auto state = harness.midiStates.last();
                      const auto ran = harness.extractionScheduler.runNext();
                      state->complete({
                          .state = Automation::ExtractionBackendState::Succeeded,
                          .notes = {{.keyIndex = 62, .localStart = 10, .length = 240}},
                      });
                      const auto terminal =
                          accepted
                              ? runtime.tasks().getTask(base.documentId, accepted.get().taskId)
                              : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                    Automation::AutomationError{});
                      suite.expect(accepted && ran && state->startCount == 1 && terminal &&
                                       terminal.get().state ==
                                           Automation::AutomationTaskState::Succeeded &&
                                       runtime.documentVersion().revision == base.revision + 1 &&
                                       harness.model().tracks().size() == tracksBefore + 1,
                                   QStringLiteral("MIDI extraction must atomically add one track"));

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
                              ? runtime.tasks().getTask(mergeBase.documentId,
                                                        mergeAccepted.get().taskId)
                              : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                    Automation::AutomationError{});
                      suite.expect(
                          mergeAccepted && mergeRan && mergeTerminal &&
                              mergeTerminal.get().state ==
                                  Automation::AutomationTaskState::Failed &&
                              mergeTerminal.get().error &&
                              mergeTerminal.get().error->code ==
                                  Automation::AutomationErrorCode::InvalidArgument &&
                              mergeTerminal.get().error->fieldPath ==
                                  QStringLiteral("destination.start") &&
                              runtime.documentVersion() == mergeBase,
                          QStringLiteral("MIDI merge must reject translated note ranges that exceed "
                                         "the model tick type"));

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
                          failedAccepted
                              ? runtime.tasks().getTask(runtime.documentVersion().documentId,
                                                        failedAccepted.get().taskId)
                              : Automation::AutomationResult<Automation::AutomationTaskSnapshot>(
                                    Automation::AutomationError{});
                      const auto wrongType = runtime.extractions().startMidi(
                          harness.context(), harness.singingClipId());
                      suite.expect(
                          failedAccepted && failedTask &&
                              failedTask.get().state == Automation::AutomationTaskState::Failed &&
                              failedTask.get().error &&
                              failedTask.get().error->code ==
                                  Automation::AutomationErrorCode::InferenceError &&
                              isError(wrongType, Automation::AutomationErrorCode::WrongObjectType,
                                      Automation::OperationIds::extract::midi::start),
                          QStringLiteral("MIDI backend and input type errors must be stable"));
                  });

        suite.run(Automation::OperationIds::extract::pitch::start,
                  QStringLiteral("unavailable-extraction-service"), [&] {
                      RuntimeHarness unavailable({.extractionServices = false});
                      const auto result = unavailable.runtime().extractions().startPitch(
                          unavailable.context(), unavailable.audioClipId(),
                          unavailable.singingClipId());
                      suite.expect(isError(result, Automation::AutomationErrorCode::ModuleNotReady,
                                           Automation::OperationIds::extract::pitch::start),
                                   QStringLiteral("missing extraction service must be explicit"));
                  });
    }

}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    Suite suite;

    testInferenceMatrix(suite);
    testInferenceValidationBoundaries(suite);
    testAudioClipDomain(suite);
    testDocumentAndImportDomains(suite);
    testFormatsAndMidiExport(suite);
    testAudioExportAndTaskList(suite);
    testExtractionDomains(suite);

    return suite.finish();
}
