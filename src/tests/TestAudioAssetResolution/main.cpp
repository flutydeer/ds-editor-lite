#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "Automation/ProjectAutomationDtos.h"
#include "Controller/Actions/AppModel/Clip/EditAudioClipPathAction.h"
#include "Controller/Tasks/ResolveAudioPathTask.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThreadPool>

#include <memory>
#include <optional>

namespace {
    class Checks final {
    public:
        void scenario(const char *name) {
            ++m_scenarios;
            m_currentScenario = QString::fromUtf8(name);
        }

        void expect(const bool condition, const char *message, const int line) {
            ++m_assertions;
            if (condition)
                return;
            ++m_failures;
            QTextStream(stderr) << "FAILED [" << m_currentScenario << ":" << line
                                << "]: " << message << Qt::endl;
        }

        [[nodiscard]] int finish() const {
            QTextStream(stdout) << "TestAudioAssetResolution: " << m_scenarios << " scenarios, "
                                << m_assertions << " assertions, " << m_failures << " failures"
                                << Qt::endl;
            return m_failures == 0 ? 0 : 1;
        }

    private:
        QString m_currentScenario;
        int m_scenarios = 0;
        int m_assertions = 0;
        int m_failures = 0;
    };

#define EXPECT(checks, condition, message) (checks).expect((condition), (message), __LINE__)

    [[nodiscard]] QString sha512(const QByteArray &data) {
        return QString::fromLatin1(
            QCryptographicHash::hash(data, QCryptographicHash::Sha512).toHex());
    }

    [[nodiscard]] bool writeFixture(const QString &directory, const QString &fileName,
                                    const QByteArray &data) {
        if (!QDir().mkpath(directory))
            return false;
        QFile file(QDir(directory).filePath(fileName));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        return file.write(data) == data.size();
    }

    [[nodiscard]] bool runResolveTask(ResolveAudioPathTask &task) {
        QThreadPool pool;
        pool.setMaxThreadCount(1);
        pool.start(&task);
        return pool.waitForDone(5000);
    }

    class CurrentDirectoryGuard final {
    public:
        explicit CurrentDirectoryGuard(const QString &path)
            : m_originalPath(QDir::currentPath()), m_changed(QDir::setCurrent(path)) {
        }

        ~CurrentDirectoryGuard() {
            if (m_changed)
                QDir::setCurrent(m_originalPath);
        }

        [[nodiscard]] bool changed() const {
            return m_changed;
        }

    private:
        QString m_originalPath;
        bool m_changed = false;
    };

    void testHitRelative(Checks &checks, const QString &root) {
        checks.scenario("relative candidate with matching hash");
        const QByteArray content("relative-audio-payload");
        const auto projectDir = QDir(root).filePath(QStringLiteral("relative-project"));
        const auto assetDir = QDir(projectDir).filePath(QStringLiteral("assets"));
        const auto fileName = QStringLiteral("relative.wav");
        EXPECT(checks, writeFixture(assetDir, fileName, content),
               "the relative audio fixture must be created");
        EXPECT(checks, writeFixture(projectDir, fileName, content),
               "the sibling candidate fixture must be created");

        ResolveAudioPathTask task;
        task.projectDir = projectDir;
        task.relativeDir = QStringLiteral("assets");
        task.fileName = fileName;
        task.expectedSha512 = sha512(content);
        EXPECT(checks, runResolveTask(task), "the relative-path task must finish");
        EXPECT(checks, task.started() && task.stopped(),
               "the task lifecycle must reach the stopped state");
        EXPECT(checks,
               task.result == ResolveAudioPathTask::Result::HitRelative &&
                   QDir::cleanPath(task.resolvedPath) ==
                       QDir::cleanPath(QDir(assetDir).filePath(fileName)),
               "the matching relative candidate must win before the sibling candidate");
    }

    void testHitSibling(Checks &checks, const QString &root) {
        checks.scenario("sibling candidate with matching hash");
        const QByteArray content("sibling-audio-payload");
        const auto projectDir = QDir(root).filePath(QStringLiteral("sibling-project"));
        const auto fileName = QStringLiteral("sibling.wav");
        EXPECT(checks, writeFixture(projectDir, fileName, content),
               "the sibling audio fixture must be created");

        ResolveAudioPathTask task;
        task.projectDir = projectDir;
        task.relativeDir = QStringLiteral("missing-media");
        task.fileName = fileName;
        task.expectedSha512 = sha512(content);
        EXPECT(checks, runResolveTask(task), "the sibling-path task must finish");
        EXPECT(checks,
               task.result == ResolveAudioPathTask::Result::HitSibling &&
                   QDir::cleanPath(task.resolvedPath) ==
                       QDir::cleanPath(QDir(projectDir).filePath(fileName)),
               "the matching sibling candidate must be selected after the relative miss");
    }

    void testHitUnconfirmed(Checks &checks, const QString &root) {
        checks.scenario("relative candidate without stored hash");
        const auto projectDir = QDir(root).filePath(QStringLiteral("unconfirmed-project"));
        const auto assetDir = QDir(projectDir).filePath(QStringLiteral("media"));
        const auto fileName = QStringLiteral("legacy.wav");
        EXPECT(checks, writeFixture(assetDir, fileName, QByteArray("legacy-audio-payload")),
               "the legacy audio fixture must be created");

        ResolveAudioPathTask task;
        task.projectDir = projectDir;
        task.relativeDir = QStringLiteral("media");
        task.fileName = fileName;
        EXPECT(checks, runResolveTask(task), "the no-hash task must finish");
        EXPECT(checks,
               task.result == ResolveAudioPathTask::Result::HitUnconfirmed &&
                   QDir::cleanPath(task.resolvedPath) ==
                       QDir::cleanPath(QDir(assetDir).filePath(fileName)),
               "a name-only hit must remain explicitly unconfirmed");
    }

    void testHashMismatch(Checks &checks, const QString &root) {
        checks.scenario("all project candidates have mismatching hashes");
        const auto projectDir = QDir(root).filePath(QStringLiteral("mismatch-project"));
        const auto assetDir = QDir(projectDir).filePath(QStringLiteral("assets"));
        const auto fileName = QStringLiteral("mismatch.wav");
        EXPECT(checks, writeFixture(assetDir, fileName, QByteArray("wrong-relative-payload")),
               "the mismatching relative fixture must be created");
        EXPECT(checks, writeFixture(projectDir, fileName, QByteArray("wrong-sibling-payload")),
               "the mismatching sibling fixture must be created");

        ResolveAudioPathTask task;
        task.projectDir = projectDir;
        task.relativeDir = QStringLiteral("assets");
        task.fileName = fileName;
        task.expectedSha512 = sha512(QByteArray("expected-audio-payload"));
        EXPECT(checks, runResolveTask(task), "the hash-mismatch task must finish");
        EXPECT(checks,
               task.result == ResolveAudioPathTask::Result::Miss && task.resolvedPath.isEmpty(),
               "hash mismatches must not produce a resolved path");
    }

    void testCurrentDirectoryDecoy(Checks &checks, const QString &root) {
        checks.scenario("current-directory name collision is ignored");
        const QByteArray decoyContent("cwd-decoy-payload");
        const auto projectDir = QDir(root).filePath(QStringLiteral("isolated-project"));
        const auto decoyDir = QDir(root).filePath(QStringLiteral("current-directory-decoy"));
        const auto fileName = QStringLiteral("same-name.wav");
        EXPECT(checks, QDir().mkpath(projectDir), "the isolated project directory must be created");
        EXPECT(checks, writeFixture(decoyDir, fileName, decoyContent),
               "the current-directory decoy must be created");

        CurrentDirectoryGuard currentDirectory(decoyDir);
        EXPECT(checks, currentDirectory.changed(), "the test current directory must be selected");
        ResolveAudioPathTask task;
        task.projectDir = projectDir;
        task.fileName = fileName;
        task.expectedSha512 = sha512(decoyContent);
        EXPECT(checks, runResolveTask(task), "the current-directory decoy task must finish");
        EXPECT(checks,
               task.result == ResolveAudioPathTask::Result::Miss && task.resolvedPath.isEmpty(),
               "a matching file outside the project directory must not be selected");
    }

    void testDirectoryCandidateRejected(Checks &checks, const QString &root) {
        checks.scenario("directory with an audio-like name is rejected");
        const auto projectDir = QDir(root).filePath(QStringLiteral("directory-candidate-project"));
        const auto fileName = QStringLiteral("not-a-file.wav");
        EXPECT(checks, QDir().mkpath(QDir(projectDir).filePath(fileName)),
               "the audio-like directory candidate must be created");

        ResolveAudioPathTask task;
        task.projectDir = projectDir;
        task.fileName = fileName;
        EXPECT(checks, runResolveTask(task), "the directory candidate task must finish");
        EXPECT(checks,
               task.result == ResolveAudioPathTask::Result::Miss && task.resolvedPath.isEmpty(),
               "a directory must never become an unconfirmed audio match");
    }

    class RuntimeFixture final {
    public:
        RuntimeFixture() : m_history(resetHistory()) {
            m_model.newProject();
            m_runtime = std::make_unique<Automation::CoreRuntime>(&m_model, m_history);
            m_ready = initializeDocument();
        }

        ~RuntimeFixture() {
            m_runtime.reset();
            m_history->reset();
        }

        [[nodiscard]] bool isReady() const {
            return m_ready;
        }

        [[nodiscard]] Automation::CoreRuntime &runtime() const {
            return *m_runtime;
        }

        [[nodiscard]] Automation::TrackId trackId() const {
            return m_trackId;
        }

        [[nodiscard]] Automation::ClipId audioClipId() const {
            return m_audioClipId;
        }

        [[nodiscard]] AudioClip *audioClip() {
            return dynamic_cast<AudioClip *>(m_model.findClipById(m_audioClipId.value()));
        }

        [[nodiscard]] Automation::CommandContext context() const {
            return {
                .expected = m_runtime->documentVersion(),
                .source = Automation::InvocationSource::Test,
            };
        }

        [[nodiscard]] static Automation::DocumentDraftDto emptyDocument() {
            AppModel model;
            model.newProject();
            return Automation::documentDraftDto(model.takeProjectData());
        }

    private:
        [[nodiscard]] static HistoryManager *resetHistory() {
            auto *history = HistoryManager::instance();
            history->reset(HistoryManager::ResetState::Saved);
            return history;
        }

        [[nodiscard]] bool initializeDocument() {
            Automation::TrackDraftDto track;
            track.clientRef = QStringLiteral("audio-asset-track");
            track.name = QStringLiteral("Audio Asset Track");
            track.gain = 1.0;
            track.defaultLanguage = QStringLiteral("en");
            const auto insertedTrack = m_runtime->project().insertTrack(context(), 0, track);
            if (!insertedTrack || insertedTrack.get().affectedObjects.size() != 1)
                return false;
            m_trackId = Automation::TrackId(insertedTrack.get().affectedObjects.first().value);

            Automation::ClipDraftDto audio;
            audio.clientRef = QStringLiteral("audio-asset-clip");
            audio.type = Automation::ClipDraftDto::Type::Audio;
            audio.properties.name = QStringLiteral("source.wav");
            audio.properties.length = 1920;
            audio.properties.clipLen = 1920;
            audio.properties.gain = 1.0;
            audio.audioPath = QStringLiteral("missing/source.wav");
            audio.audioPathStatus = AudioClip::PathStatus::Missing;
            const auto insertedClip = m_runtime->project().insertClips(
                context(), {
                               {.trackId = m_trackId, .clip = std::move(audio)}
            });
            if (!insertedClip || insertedClip.get().affectedObjects.size() != 1)
                return false;
            m_audioClipId = Automation::ClipId(insertedClip.get().affectedObjects.first().value);
            return true;
        }

        AppModel m_model;
        HistoryManager *m_history;
        std::unique_ptr<Automation::CoreRuntime> m_runtime;
        Automation::TrackId m_trackId;
        Automation::ClipId m_audioClipId;
        bool m_ready = false;
    };

    [[nodiscard]] std::optional<Automation::ClipDraftDto>
        audioClipSnapshot(Automation::CoreRuntime &runtime, const Automation::ClipId clipId) {
        const auto project = runtime.project().getProject(runtime.documentVersion().documentId);
        if (!project)
            return std::nullopt;
        for (const auto &track : project.get().tracks) {
            for (const auto &clip : track.clips) {
                if (clip.id == clipId && clip.data.type == Automation::ClipDraftDto::Type::Audio)
                    return clip.data;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool sameAudioInfo(const AudioInfoModel &left, const AudioInfoModel &right) {
        return left.chunkSize == right.chunkSize && left.mipmapScale == right.mipmapScale &&
               left.sampleRate == right.sampleRate && left.channels == right.channels &&
               left.frames == right.frames && left.peakCache == right.peakCache &&
               left.peakCacheMipmap == right.peakCacheMipmap;
    }

    [[nodiscard]] bool sameHistoryState(const Automation::HistoryStateDto &left,
                                        const Automation::HistoryStateDto &right) {
        return left.document == right.document && left.canUndo == right.canUndo &&
               left.canRedo == right.canRedo && left.onSavePoint == right.onSavePoint &&
               left.undoName == right.undoName && left.redoName == right.redoName;
    }

    template <typename T>
    [[nodiscard]] bool isStaleAssetError(const Automation::AutomationResult<T> &result,
                                         const Automation::OperationId &operationId) {
        return !result &&
               result.getError().code == Automation::AutomationErrorCode::InvalidArgument &&
               result.getError().fieldPath == QStringLiteral("expected_asset") &&
               result.getError().operationId == operationId;
    }

    template <typename T>
    [[nodiscard]] bool isResolvedPathError(const Automation::AutomationResult<T> &result,
                                           const Automation::AutomationErrorCode code,
                                           const Automation::ClipId clipId) {
        return !result && result.getError().code == code &&
               result.getError().fieldPath == QStringLiteral("resolved_path") &&
               result.getError().operationId ==
                   Automation::OperationIds::audio_clips::apply_resolved_path &&
               result.getError().object == std::optional(Automation::ObjectRef{
                                               Automation::ObjectKind::Clip, clipId.value()});
    }

    void testDerivedAudioWritebacks(Checks &checks) {
        checks.scenario("derived audio writebacks tolerate revision drift within one generation");
        RuntimeFixture fixture;
        EXPECT(checks, fixture.isReady(), "the runtime fixture must initialize through the facade");
        if (!fixture.isReady())
            return;

        auto &runtime = fixture.runtime();
        QTemporaryDir resolvedFiles;
        EXPECT(checks, resolvedFiles.isValid(), "the resolved audio directory must be available");
        if (!resolvedFiles.isValid())
            return;
        const auto resolvedPath =
            QDir(resolvedFiles.path()).filePath(QStringLiteral("resolved-source.wav"));
        EXPECT(checks,
               writeFixture(resolvedFiles.path(), QStringLiteral("resolved-source.wav"),
                            QByteArray("resolved-audio-payload")),
               "the resolved audio file must be created");
        const auto taskVersion = runtime.documentVersion();
        const auto initialClip = audioClipSnapshot(runtime, fixture.audioClipId());
        EXPECT(checks,
               initialClip && initialClip->audioPath == QStringLiteral("missing/source.wav") &&
                   initialClip->audioPathStatus == AudioClip::PathStatus::Missing,
               "the audio clip must start with the captured task path");
        if (!initialClip)
            return;

        const auto ordinaryEdit =
            runtime.project().setTrackColor(fixture.context(), fixture.trackId(), 3);
        const auto editedVersion = runtime.documentVersion();
        EXPECT(checks,
               ordinaryEdit && ordinaryEdit.get().changed &&
                   editedVersion.documentId == taskVersion.documentId &&
                   editedVersion.revision == taskVersion.revision + 1,
               "an ordinary edit must advance revision within the captured generation");
        const auto historyAfterEdit = runtime.history().getState(editedVersion.documentId);
        EXPECT(checks, static_cast<bool>(historyAfterEdit),
               "history state must be available after the ordinary edit");

        const auto validationContext = runtime.derivedWritebackContext(taskVersion, true);
        EXPECT(checks,
               validationContext && validationContext.get().expected == editedVersion &&
                   validationContext.get().validateOnly &&
                   validationContext.get().source ==
                       Automation::InvocationSource::InternalAutomation,
               "a stale task revision must rebase to a validate-only internal writeback context");
        const QString oldPath = QStringLiteral("missing/source.wav");
        const auto initialAsset = Automation::audioAssetSnapshotDto(*fixture.audioClip());
        if (validationContext) {
            const auto validation = runtime.project().applyResolvedAudioPath(
                validationContext.get(), fixture.audioClipId(), initialAsset, resolvedPath,
                AudioClip::PathStatus::Normal);
            const auto afterValidation = audioClipSnapshot(runtime, fixture.audioClipId());
            EXPECT(
                checks,
                validation && validation.get().changed && validation.get().validatedOnly &&
                    validation.get().previous == editedVersion &&
                    validation.get().current == editedVersion,
                "resolved-path validation must predict the derived change without revision growth");
            EXPECT(checks, afterValidation && afterValidation->audioPath == oldPath,
                   "validate-only resolution must leave the clip path unchanged");
        }

        const auto resolvedContext = runtime.derivedWritebackContext(taskVersion, false);
        EXPECT(checks,
               resolvedContext && resolvedContext.get().expected == editedVersion &&
                   !resolvedContext.get().validateOnly,
               "the captured task version must rebase for the resolved-path commit");
        if (!resolvedContext)
            return;
        const auto resolved = runtime.project().applyResolvedAudioPath(
            resolvedContext.get(), fixture.audioClipId(), initialAsset, resolvedPath,
            AudioClip::PathStatus::Normal);
        EXPECT(checks,
               resolved && resolved.get().changed && resolved.get().previous == editedVersion &&
                   resolved.get().current == editedVersion &&
                   runtime.documentVersion() == editedVersion,
               "apply_resolved_path must succeed without advancing revision");

        AudioInfoModel audioInfo;
        audioInfo.chunkSize = 256;
        audioInfo.mipmapScale = 4;
        audioInfo.sampleRate = 48000;
        audioInfo.channels = 2;
        audioInfo.frames = 4096;
        audioInfo.peakCache = {
            {-12, 12},
            {-7,  9 }
        };
        audioInfo.peakCacheMipmap = {
            {-12, 12}
        };
        const auto hash = sha512(QByteArray("resolved-audio-payload"));

        const auto staleContext = runtime.derivedWritebackContext(taskVersion, false);
        EXPECT(checks, static_cast<bool>(staleContext),
               "a same-generation stale-path check must obtain a context");
        if (staleContext) {
            const auto staleResolved = runtime.project().applyResolvedAudioPath(
                staleContext.get(), fixture.audioClipId(), initialAsset,
                QStringLiteral("resolved/other.wav"), AudioClip::PathStatus::Normal);
            const auto staleCache = runtime.project().applyAudioDecodeCache(
                staleContext.get(), fixture.audioClipId(), initialAsset, audioInfo);
            const auto staleHash = runtime.project().setAudioClipHash(
                staleContext.get(), fixture.audioClipId(), initialAsset, hash);
            const auto staleStatus = runtime.project().setAudioClipPathStatus(
                staleContext.get(), fixture.audioClipId(), initialAsset,
                AudioClip::PathStatus::Missing);
            EXPECT(
                checks,
                isStaleAssetError(staleResolved,
                                  Automation::OperationIds::audio_clips::apply_resolved_path) &&
                    isStaleAssetError(staleCache,
                                      Automation::OperationIds::audio_clips::apply_decode_cache) &&
                    isStaleAssetError(staleHash, Automation::OperationIds::audio_clips::set_hash) &&
                    isStaleAssetError(staleStatus,
                                      Automation::OperationIds::audio_clips::set_path_status),
                "every derived audio mutation must reject a changed expected asset");
            const auto afterStaleAttempts = audioClipSnapshot(runtime, fixture.audioClipId());
            EXPECT(checks,
                   afterStaleAttempts && afterStaleAttempts->audioPath == resolvedPath &&
                       afterStaleAttempts->audioPathInfo.sha512.isEmpty() &&
                       afterStaleAttempts->audioPathStatus == AudioClip::PathStatus::Normal &&
                       runtime.documentVersion() == editedVersion,
                   "stale-path rejection must leave derived state and revision untouched");
        }

        const auto resolvedClip = audioClipSnapshot(runtime, fixture.audioClipId());
        EXPECT(checks, static_cast<bool>(resolvedClip),
               "the resolved audio snapshot must remain queryable");
        if (!resolvedClip)
            return;
        const auto resolvedAsset = Automation::audioAssetSnapshotDto(*fixture.audioClip());
        const auto cacheContext = runtime.derivedWritebackContext(taskVersion, false);
        const auto decoded =
            cacheContext
                ? runtime.project().applyAudioDecodeCache(cacheContext.get(), fixture.audioClipId(),
                                                          resolvedAsset, audioInfo)
                : Automation::AutomationResult<Automation::MutationResult>(cacheContext.getError());
        EXPECT(checks,
               decoded && decoded.get().changed && decoded.get().previous == editedVersion &&
                   decoded.get().current == editedVersion &&
                   runtime.documentVersion() == editedVersion,
               "apply_decode_cache must succeed without advancing revision");

        const auto hashContext = runtime.derivedWritebackContext(taskVersion, false);
        const auto hashResult =
            hashContext
                ? runtime.project().setAudioClipHash(hashContext.get(), fixture.audioClipId(),
                                                     resolvedAsset, hash)
                : Automation::AutomationResult<Automation::MutationResult>(hashContext.getError());
        EXPECT(checks,
               hashResult && hashResult.get().changed &&
                   hashResult.get().previous == editedVersion &&
                   hashResult.get().current == editedVersion &&
                   runtime.documentVersion() == editedVersion,
               "set_hash must succeed without advancing revision");

        const auto hashedClip = audioClipSnapshot(runtime, fixture.audioClipId());
        EXPECT(checks, static_cast<bool>(hashedClip),
               "the hashed audio snapshot must remain queryable");
        if (!hashedClip)
            return;
        const auto statusContext = runtime.derivedWritebackContext(taskVersion, false);
        const auto status = statusContext
                                ? runtime.project().setAudioClipPathStatus(
                                      statusContext.get(), fixture.audioClipId(),
                                      Automation::audioAssetSnapshotDto(*fixture.audioClip()),
                                      AudioClip::PathStatus::Unconfirmed)
                                : Automation::AutomationResult<Automation::MutationResult>(
                                      statusContext.getError());
        EXPECT(checks,
               status && status.get().changed && status.get().previous == editedVersion &&
                   status.get().current == editedVersion &&
                   runtime.documentVersion() == editedVersion,
               "set_path_status must succeed without advancing revision");

        const auto finalClip = audioClipSnapshot(runtime, fixture.audioClipId());
        EXPECT(checks,
               finalClip && finalClip->audioPath == resolvedPath &&
                   sameAudioInfo(finalClip->audioInfo, audioInfo) &&
                   finalClip->audioPathInfo.sha512 == hash &&
                   finalClip->audioPathStatus == AudioClip::PathStatus::Unconfirmed,
               "all accepted derived audio state must be visible through the project facade");
        const auto historyAfterWritebacks = runtime.history().getState(editedVersion.documentId);
        EXPECT(checks,
               historyAfterEdit && historyAfterWritebacks &&
                   sameHistoryState(historyAfterEdit.get(), historyAfterWritebacks.get()),
               "derived audio writebacks must not create history entries");

        const auto replacement = runtime.documents().commitNewDocument(
            fixture.context(), RuntimeFixture::emptyDocument());
        EXPECT(checks,
               replacement && replacement.get().changed &&
                   replacement.get().current.documentId != taskVersion.documentId &&
                   replacement.get().current.revision == 0,
               "document replacement must rotate the generation and reset revision");
        const auto rejectedGeneration = runtime.derivedWritebackContext(taskVersion, false);
        EXPECT(checks,
               !rejectedGeneration &&
                   rejectedGeneration.getError().code ==
                       Automation::AutomationErrorCode::DocumentChanged &&
                   rejectedGeneration.getError().documentId == taskVersion.documentId,
               "a captured task version must be rejected after generation replacement");
    }

    void testSamePathSourceReplacement(Checks &checks) {
        checks.scenario("same-path source replacement invalidates a captured task snapshot");
        RuntimeFixture fixture;
        EXPECT(checks, fixture.isReady(), "the runtime fixture must initialize through the facade");
        if (!fixture.isReady())
            return;

        auto &runtime = fixture.runtime();
        const auto taskVersion = runtime.documentVersion();
        const auto before = audioClipSnapshot(runtime, fixture.audioClipId());
        EXPECT(checks, static_cast<bool>(before), "the initial audio asset must be queryable");
        if (!before)
            return;
        const auto taskAsset = Automation::audioAssetSnapshotDto(*fixture.audioClip());

        AudioPathInfo replacementInfo;
        replacementInfo.relativeDir = QStringLiteral("replacement-media");
        replacementInfo.sha512 = QStringLiteral("replacement-hash");
        const QJsonObject replacementFormat{
            {QStringLiteral("entryClassName"), QStringLiteral("ReplacementFormat")}
        };
        const auto replaced =
            runtime.project().relocateAudioClip(fixture.context(), fixture.audioClipId(),
                                                taskAsset.path, replacementInfo, replacementFormat);
        EXPECT(checks,
               replaced && replaced.get().changed &&
                   runtime.documentVersion().revision == taskVersion.revision + 1,
               "same-path source metadata replacement must be a normal document edit");
        auto *replacementClip = fixture.audioClip();
        EXPECT(checks,
               replacementClip && replacementClip->sourceGeneration() > taskAsset.sourceGeneration,
               "same-path source replacement must advance its runtime generation");
        if (!replacementClip)
            return;
        const auto replacementAsset = Automation::audioAssetSnapshotDto(*replacementClip);
        const auto replacementHash = sha512(QByteArray("replacement-audio-payload"));
        const auto replacementHashResult = runtime.project().setAudioClipHash(
            fixture.context(), fixture.audioClipId(), replacementAsset, replacementHash);
        EXPECT(checks, replacementHashResult && replacementHashResult.get().changed,
               "the replacement source hash must commit before stale tasks finish");

        AudioInfoModel staleInfo;
        staleInfo.sampleRate = 44100;
        staleInfo.channels = 1;
        staleInfo.frames = 128;
        Automation::CommandContext staleRevisionContext{
            .expected = taskVersion,
            .source = Automation::InvocationSource::Test,
        };
        const auto strict = runtime.project().applyAudioDecodeCache(
            staleRevisionContext, fixture.audioClipId(), taskAsset, staleInfo);
        EXPECT(checks,
               isStaleAssetError(strict,
                                 Automation::OperationIds::audio_clips::apply_decode_cache),
               "a derived writeback must honor its revision-free contract while rejecting the "
               "stale asset snapshot");

        const auto rebasedContext = runtime.derivedWritebackContext(taskVersion, false);
        const auto staleCache =
            rebasedContext ? runtime.project().applyAudioDecodeCache(
                                 rebasedContext.get(), fixture.audioClipId(), taskAsset, staleInfo)
                           : Automation::AutomationResult<Automation::MutationResult>(
                                 rebasedContext.getError());
        const auto staleHash = rebasedContext
                                   ? runtime.project().setAudioClipHash(
                                         rebasedContext.get(), fixture.audioClipId(), taskAsset,
                                         sha512(QByteArray("stale-audio-payload")))
                                   : Automation::AutomationResult<Automation::MutationResult>(
                                         rebasedContext.getError());
        const auto staleStatus = rebasedContext
                                     ? runtime.project().setAudioClipPathStatus(
                                           rebasedContext.get(), fixture.audioClipId(), taskAsset,
                                           AudioClip::PathStatus::Missing)
                                     : Automation::AutomationResult<Automation::MutationResult>(
                                           rebasedContext.getError());
        EXPECT(checks,
               isStaleAssetError(staleCache,
                                 Automation::OperationIds::audio_clips::apply_decode_cache) &&
                   isStaleAssetError(staleHash, Automation::OperationIds::audio_clips::set_hash) &&
                   isStaleAssetError(staleStatus,
                                     Automation::OperationIds::audio_clips::set_path_status),
               "rebasing must reject every old writeback after a same-path source replacement");
        const auto finalClip = audioClipSnapshot(runtime, fixture.audioClipId());
        EXPECT(checks,
               finalClip && finalClip->audioPathInfo.sha512 == replacementHash &&
                   finalClip->audioPathStatus == AudioClip::PathStatus::Normal,
               "old tasks must not overwrite the replacement hash or path status");
    }

    void testResolvedPathValidation(Checks &checks) {
        checks.scenario("resolved audio target must be an existing absolute regular file");
        RuntimeFixture fixture;
        QTemporaryDir targets;
        EXPECT(checks, fixture.isReady() && targets.isValid(),
               "the runtime and target fixtures must initialize");
        if (!fixture.isReady() || !targets.isValid())
            return;

        auto &runtime = fixture.runtime();
        const auto source = audioClipSnapshot(runtime, fixture.audioClipId());
        const auto beforeVersion = runtime.documentVersion();
        const auto beforeHistory = runtime.history().getState(beforeVersion.documentId);
        EXPECT(checks, source && beforeHistory,
               "the source and history snapshots must be available");
        if (!source || !beforeHistory)
            return;

        const auto asset = Automation::audioAssetSnapshotDto(*fixture.audioClip());
        const auto relative = runtime.project().applyResolvedAudioPath(
            fixture.context(), fixture.audioClipId(), asset, QStringLiteral("relative.wav"),
            AudioClip::PathStatus::Normal);
        const auto missing = runtime.project().applyResolvedAudioPath(
            fixture.context(), fixture.audioClipId(), asset,
            QDir(targets.path()).filePath(QStringLiteral("missing.wav")),
            AudioClip::PathStatus::Normal);
        const auto directory = runtime.project().applyResolvedAudioPath(
            fixture.context(), fixture.audioClipId(), asset, targets.path(),
            AudioClip::PathStatus::Normal);
        const auto after = audioClipSnapshot(runtime, fixture.audioClipId());
        const auto afterHistory = runtime.history().getState(beforeVersion.documentId);

        EXPECT(checks,
               isResolvedPathError(relative, Automation::AutomationErrorCode::InvalidArgument,
                                   fixture.audioClipId()) &&
                   isResolvedPathError(missing, Automation::AutomationErrorCode::FileNotFound,
                                       fixture.audioClipId()) &&
                   isResolvedPathError(directory, Automation::AutomationErrorCode::InvalidArgument,
                                       fixture.audioClipId()),
               "relative, missing, and directory targets must return typed operation errors");
        EXPECT(checks,
               after && after->audioPath == source->audioPath &&
                   after->audioPathStatus == source->audioPathStatus &&
                   runtime.documentVersion() == beforeVersion && afterHistory &&
                   sameHistoryState(beforeHistory.get(), afterHistory.get()),
               "invalid targets must not change source state, revision, or history");
    }

    void testSamePathResolutionNotification(Checks &checks) {
        checks.scenario("same-path resolution notifies source consumers exactly once");
        RuntimeFixture fixture;
        QTemporaryDir sourceFiles;
        EXPECT(checks, fixture.isReady() && sourceFiles.isValid(),
               "the runtime and source fixtures must initialize");
        if (!fixture.isReady() || !sourceFiles.isValid())
            return;

        const auto sourcePath =
            QDir(sourceFiles.path()).filePath(QStringLiteral("same-source.wav"));
        EXPECT(checks,
               writeFixture(sourceFiles.path(), QStringLiteral("same-source.wav"),
                            QByteArray("same-source-audio")),
               "the same-path audio fixture must be created");
        auto &runtime = fixture.runtime();
        const auto relocated = runtime.project().relocateAudioClip(
            fixture.context(), fixture.audioClipId(), sourcePath, {}, {});
        auto *clip = fixture.audioClip();
        EXPECT(checks, relocated && relocated.get().changed && clip,
               "the clip must adopt the absolute source before resolution");
        if (!relocated || !clip)
            return;

        const auto missing = runtime.project().setAudioClipPathStatus(
            fixture.context(), fixture.audioClipId(), Automation::audioAssetSnapshotDto(*clip),
            AudioClip::PathStatus::Missing);
        int sourceChangeCount = 0;
        QObject::connect(clip, &AudioClip::sourceChanged, clip,
                         [&sourceChangeCount] { ++sourceChangeCount; });
        const auto base = runtime.documentVersion();
        const auto resolved = runtime.project().applyResolvedAudioPath(
            fixture.context(), fixture.audioClipId(), Automation::audioAssetSnapshotDto(*clip),
            sourcePath, AudioClip::PathStatus::Normal);
        EXPECT(checks,
               missing && resolved && resolved.get().changed && sourceChangeCount == 1 &&
                   clip->path() == sourcePath &&
                   clip->pathStatus() == AudioClip::PathStatus::Normal &&
                   runtime.documentVersion() == base,
               "status-only resolution must wake decoders without advancing revision");

        const auto generationAfterNormal = clip->sourceGeneration();
        const auto missingAgain = runtime.project().setAudioClipPathStatus(
            fixture.context(), fixture.audioClipId(), Automation::audioAssetSnapshotDto(*clip),
            AudioClip::PathStatus::Missing);
        sourceChangeCount = 0;
        auto statusAtSourceChange = AudioClip::PathStatus::Normal;
        QObject::connect(clip, &AudioClip::sourceChanged, clip, [clip, &statusAtSourceChange] {
            statusAtSourceChange = clip->pathStatus();
        });
        const auto unconfirmed = runtime.project().applyResolvedAudioPath(
            fixture.context(), fixture.audioClipId(), Automation::audioAssetSnapshotDto(*clip),
            sourcePath, AudioClip::PathStatus::Unconfirmed);
        EXPECT(checks,
               missingAgain && unconfirmed && unconfirmed.get().changed && sourceChangeCount == 1 &&
                   statusAtSourceChange == AudioClip::PathStatus::Unconfirmed &&
                   clip->pathStatus() == AudioClip::PathStatus::Unconfirmed &&
                   clip->sourceGeneration() == generationAfterNormal + 1 &&
                   runtime.documentVersion() == base,
               "same-path name-only resolution must publish and preserve Unconfirmed status");

        AudioInfoModel decodedInfo;
        decodedInfo.sampleRate = 48000;
        decodedInfo.channels = 1;
        decodedInfo.frames = 960;
        decodedInfo.peakCache = {
            {-5, 7}
        };
        const auto decoded = runtime.project().applyAudioDecodeCache(
            fixture.context(), fixture.audioClipId(), Automation::audioAssetSnapshotDto(*clip),
            decodedInfo);
        EXPECT(checks,
               decoded && decoded.get().changed &&
                   clip->pathStatus() == AudioClip::PathStatus::Unconfirmed &&
                   sameAudioInfo(clip->audioInfo(), decodedInfo) &&
                   runtime.documentVersion() == base,
               "successful decoding must not implicitly confirm a name-only audio match");
    }

    void testSamePathRelinkNotification(Checks &checks) {
        checks.scenario("same-path manual relink notifies source consumers on execute and undo");
        AudioClip clip;
        clip.setPath(QStringLiteral("same-source.wav"));
        AudioPathInfo oldInfo;
        oldInfo.relativeDir = QStringLiteral("old-media");
        oldInfo.sha512 = QStringLiteral("old-hash");
        clip.setPathInfo(oldInfo);
        const QJsonObject oldFormat{
            {QStringLiteral("entryClassName"), QStringLiteral("OldFormat")}
        };
        clip.workspace().insert(QStringLiteral("diffscope.audio.formatData"), oldFormat);
        AudioInfoModel oldAudioInfo;
        oldAudioInfo.sampleRate = 44100;
        oldAudioInfo.channels = 1;
        oldAudioInfo.frames = 256;
        clip.setAudioInfo(oldAudioInfo);

        AudioPathInfo newInfo;
        newInfo.relativeDir = QStringLiteral("new-media");
        newInfo.sha512 = QStringLiteral("new-hash");
        const QJsonObject newFormat{
            {QStringLiteral("entryClassName"), QStringLiteral("NewFormat")}
        };
        int sourceChangeCount = 0;
        int pathChangeCount = 0;
        QObject::connect(&clip, &AudioClip::sourceChanged, &clip,
                         [&sourceChangeCount] { ++sourceChangeCount; });
        QObject::connect(&clip, &AudioClip::pathChanged, &clip,
                         [&pathChangeCount] { ++pathChangeCount; });
        const auto initialGeneration = clip.sourceGeneration();

        std::unique_ptr<EditAudioClipPathAction> action(
            EditAudioClipPathAction::build(&clip, clip.path(), newInfo, newFormat));
        action->execute();
        const bool executed =
            clip.pathInfo().relativeDir == newInfo.relativeDir &&
            clip.pathInfo().sha512 == newInfo.sha512 &&
            clip.workspace().value(QStringLiteral("diffscope.audio.formatData")) == newFormat &&
            clip.audioInfo().peakCache.isEmpty() && clip.audioInfo().sampleRate == 0 &&
            clip.sourceGeneration() == initialGeneration + 1;
        action->undo();
        const bool undone =
            clip.pathInfo().relativeDir == oldInfo.relativeDir &&
            clip.pathInfo().sha512 == oldInfo.sha512 &&
            clip.workspace().value(QStringLiteral("diffscope.audio.formatData")) == oldFormat &&
            clip.sourceGeneration() == initialGeneration + 2;
        EXPECT(checks, executed && undone && sourceChangeCount == 2 && pathChangeCount == 0,
               "metadata-only relink and undo must each publish one source change");
    }

    void testResolvedPathSourceFailureWins(Checks &checks) {
        checks.scenario("source load failure wins during different-path resolution notification");
        RuntimeFixture fixture;
        QTemporaryDir resolvedFiles;
        EXPECT(checks, fixture.isReady() && resolvedFiles.isValid(),
               "the runtime and resolved-file fixtures must initialize");
        if (!fixture.isReady() || !resolvedFiles.isValid())
            return;

        const auto resolvedPath =
            QDir(resolvedFiles.path()).filePath(QStringLiteral("resolved-source.wav"));
        EXPECT(checks,
               writeFixture(resolvedFiles.path(), QStringLiteral("resolved-source.wav"),
                            QByteArray("source-load-failure-fixture")),
               "the different-path resolution target must be created");

        auto &runtime = fixture.runtime();
        auto *clip = fixture.audioClip();
        const auto base = runtime.documentVersion();
        const auto historyBefore = runtime.history().getState(base.documentId);
        EXPECT(checks, clip && historyBefore, "the source and history snapshots must be available");
        if (!clip || !historyBefore)
            return;
        const auto source = Automation::audioAssetSnapshotDto(*clip);

        int sourceChangeCount = 0;
        auto statusAtSourceChange = AudioClip::PathStatus::Missing;
        bool failureStatusApplied = false;
        QObject::connect(clip, &AudioClip::sourceChanged, clip, [&] {
            ++sourceChangeCount;
            statusAtSourceChange = clip->pathStatus();
            const auto failure = runtime.project().setAudioClipPathStatus(
                fixture.context(), fixture.audioClipId(), Automation::audioAssetSnapshotDto(*clip),
                AudioClip::PathStatus::Missing);
            failureStatusApplied = failure && failure.get().changed;
        });

        const auto resolved = runtime.project().applyResolvedAudioPath(
            fixture.context(), fixture.audioClipId(), source, resolvedPath,
            AudioClip::PathStatus::Normal);
        const auto historyAfter = runtime.history().getState(base.documentId);
        EXPECT(checks,
               resolved && resolved.get().changed && sourceChangeCount == 1 &&
                   statusAtSourceChange == AudioClip::PathStatus::Normal && failureStatusApplied &&
                   clip->path() == resolvedPath &&
                   clip->pathStatus() == AudioClip::PathStatus::Missing &&
                   clip->sourceGeneration() == source.sourceGeneration + 1,
               "source observers must see the requested status and retain a synchronous failure");
        EXPECT(checks,
               runtime.documentVersion() == base && historyAfter &&
                   sameHistoryState(historyBefore.get(), historyAfter.get()),
               "derived resolution and source failure must not change revision or history");
    }

    void testRelinkHistoryNotifications(Checks &checks) {
        checks.scenario("different-path relink publishes one source generation per history step");
        RuntimeFixture fixture;
        EXPECT(checks, fixture.isReady(), "the runtime fixture must initialize through the facade");
        if (!fixture.isReady())
            return;

        auto &runtime = fixture.runtime();
        auto *clip = fixture.audioClip();
        EXPECT(checks, clip != nullptr, "the audio clip must be addressable");
        if (!clip)
            return;

        const auto originalAsset = Automation::audioAssetSnapshotDto(*clip);
        const auto base = runtime.documentVersion();
        AudioPathInfo replacementInfo;
        replacementInfo.relativeDir = QStringLiteral("replacement-media");
        replacementInfo.sha512 = QStringLiteral("replacement-hash");
        const QJsonObject replacementFormat{
            {QStringLiteral("entryClassName"), QStringLiteral("ReplacementFormat")}
        };
        QList<Automation::AudioAssetSnapshotDto> observedSources;
        int pathChangeCount = 0;
        QObject::connect(clip, &AudioClip::sourceChanged, clip, [clip, &observedSources] {
            observedSources.append(Automation::audioAssetSnapshotDto(*clip));
        });
        QObject::connect(clip, &AudioClip::pathChanged, clip,
                         [&pathChangeCount] { ++pathChangeCount; });

        const auto replacementPath = QStringLiteral("replacement/source.wav");
        const auto relocate = runtime.project().relocateAudioClip(
            fixture.context(), fixture.audioClipId(), replacementPath, replacementInfo,
            replacementFormat);
        const auto undo = runtime.history().undo(fixture.context());
        const auto redo = runtime.history().redo(fixture.context());
        const auto finalAsset = Automation::audioAssetSnapshotDto(*clip);

        EXPECT(checks,
               relocate && relocate.get().changed && undo && undo.get().changed && redo &&
                   redo.get().changed && runtime.documentVersion().revision == base.revision + 3,
               "relink, undo, and redo must each be one document revision");
        EXPECT(checks,
               observedSources.size() == 3 && pathChangeCount == 3 &&
                   observedSources.at(0).path == replacementPath &&
                   observedSources.at(0).pathInfo.relativeDir == replacementInfo.relativeDir &&
                   observedSources.at(0).formatData == replacementFormat &&
                   observedSources.at(1).path == originalAsset.path &&
                   observedSources.at(2).path == replacementPath &&
                   observedSources.at(0).sourceGeneration == originalAsset.sourceGeneration + 1 &&
                   observedSources.at(1).sourceGeneration == originalAsset.sourceGeneration + 2 &&
                   observedSources.at(2).sourceGeneration == originalAsset.sourceGeneration + 3 &&
                   finalAsset == observedSources.last(),
               "each history direction must publish the complete source exactly once");
    }

    void testResolveDecodeTaskProtocol(Checks &checks) {
        checks.scenario("resolved source generation flows into a successful decode task");
        RuntimeFixture fixture;
        QTemporaryDir projectFiles;
        EXPECT(checks, fixture.isReady() && projectFiles.isValid(),
               "the runtime and project fixtures must initialize");
        if (!fixture.isReady() || !projectFiles.isValid())
            return;

        const QByteArray content("resolved-task-protocol-audio");
        const auto mediaDir = QDir(projectFiles.path()).filePath(QStringLiteral("media"));
        const auto fileName = QStringLiteral("source.wav");
        EXPECT(checks, writeFixture(mediaDir, fileName, content),
               "the protocol audio fixture must be created");

        auto &runtime = fixture.runtime();
        AudioPathInfo pathInfo;
        pathInfo.relativeDir = QStringLiteral("media");
        pathInfo.sha512 = sha512(content);
        const auto configured =
            runtime.project().relocateAudioClip(fixture.context(), fixture.audioClipId(),
                                                QStringLiteral("missing/source.wav"), pathInfo, {});
        auto *clip = fixture.audioClip();
        const auto markedMissing =
            configured && clip
                ? runtime.project().setAudioClipPathStatus(fixture.context(), fixture.audioClipId(),
                                                           Automation::audioAssetSnapshotDto(*clip),
                                                           AudioClip::PathStatus::Missing)
                : Automation::AutomationResult<Automation::MutationResult>(
                      Automation::AutomationError{.message = QStringLiteral("setup failed")});
        const auto base = runtime.documentVersion();
        const auto historyBefore = runtime.history().getState(base.documentId);
        EXPECT(checks,
               configured && configured.get().changed && markedMissing && clip && historyBefore,
               "the clip must start as a hash-addressed missing source");
        if (!configured || !markedMissing || !clip || !historyBefore)
            return;

        const auto resolveAsset = Automation::audioAssetSnapshotDto(*clip);
        ResolveAudioPathTask resolveTask;
        resolveTask.clipId = fixture.audioClipId().value();
        resolveTask.documentVersion = base;
        resolveTask.assetSnapshot = resolveAsset;
        resolveTask.originalPath = resolveAsset.path;
        resolveTask.projectDir = projectFiles.path();
        resolveTask.relativeDir = resolveAsset.pathInfo.relativeDir;
        resolveTask.fileName = QFileInfo(resolveAsset.path).fileName();
        resolveTask.expectedSha512 = resolveAsset.pathInfo.sha512;

        const auto resolveRecord = runtime.automationTasks().createTask(
            Automation::OperationIds::audio_clips::apply_resolved_path, base,
            Automation::ObjectRef{Automation::ObjectKind::Clip, fixture.audioClipId().value()});
        resolveTask.automationTaskId = resolveRecord.taskId;
        const bool resolveRunning = runtime.automationTasks().markRunning(resolveRecord.taskId);
        const bool resolveFinished = runResolveTask(resolveTask);
        EXPECT(checks,
               resolveRunning && resolveFinished &&
                   resolveTask.result == ResolveAudioPathTask::Result::HitRelative &&
                   QDir::cleanPath(resolveTask.resolvedPath) ==
                       QDir::cleanPath(QDir(mediaDir).filePath(fileName)),
               "the resolve task must reach a verified relative hit while Running");

        std::optional<Automation::AudioAssetSnapshotDto> decodeAsset;
        std::optional<Automation::AutomationTaskSnapshot> decodeRecord;
        int sourceChangeCount = 0;
        bool decodeRunning = false;
        QObject::connect(clip, &AudioClip::sourceChanged, clip, [&] {
            ++sourceChangeCount;
            decodeAsset = Automation::audioAssetSnapshotDto(*clip);
            decodeRecord = runtime.automationTasks().createTask(
                Automation::OperationIds::audio_clips::apply_decode_cache,
                runtime.documentVersion(),
                Automation::ObjectRef{Automation::ObjectKind::Clip, fixture.audioClipId().value()});
            decodeRunning = runtime.automationTasks().markRunning(decodeRecord->taskId);
        });

        const auto resolveValidationContext = runtime.derivedWritebackContext(base, true);
        EXPECT(checks, static_cast<bool>(resolveValidationContext),
               "the resolve task must obtain a validation context");
        if (!resolveValidationContext)
            return;
        const auto resolveValidation = runtime.project().applyResolvedAudioPath(
            resolveValidationContext.get(), fixture.audioClipId(), resolveAsset,
            resolveTask.resolvedPath, AudioClip::PathStatus::Normal);
        const auto resolveCommitting =
            runtime.automationTasks().beginCommitting(resolveRecord.taskId);
        EXPECT(checks,
               resolveValidation && resolveValidation.get().validatedOnly && resolveCommitting &&
                   resolveCommitting.get(),
               "the verified resolve result must validate before entering Committing");
        if (!resolveValidation || !resolveCommitting || !resolveCommitting.get())
            return;

        auto resolveCommitContext = resolveValidationContext.get();
        resolveCommitContext.validateOnly = false;
        const auto resolved = runtime.project().applyResolvedAudioPath(
            resolveCommitContext, fixture.audioClipId(), resolveAsset, resolveTask.resolvedPath,
            AudioClip::PathStatus::Normal);
        const bool resolveSucceeded =
            resolved && runtime.automationTasks().succeed(resolveRecord.taskId, resolved.get());
        EXPECT(checks,
               resolveSucceeded && sourceChangeCount == 1 && decodeAsset && decodeRecord &&
                   decodeRunning &&
                   decodeAsset->sourceGeneration == resolveAsset.sourceGeneration + 1 &&
                   decodeAsset->path == resolveTask.resolvedPath,
               "sourceChanged must hand the resolved generation to one Running decode task");
        if (!resolveSucceeded || !decodeAsset || !decodeRecord || !decodeRunning)
            return;

        AudioInfoModel decodedInfo;
        decodedInfo.sampleRate = 48000;
        decodedInfo.channels = 2;
        decodedInfo.frames = 1920;
        decodedInfo.peakCache = {
            {-8, 10}
        };
        const auto decodeValidationContext =
            runtime.derivedWritebackContext(decodeRecord->baseDocument, true);
        EXPECT(checks, static_cast<bool>(decodeValidationContext),
               "the decode task must obtain a validation context");
        if (!decodeValidationContext)
            return;
        const auto decodeValidation = runtime.project().applyAudioDecodeCache(
            decodeValidationContext.get(), fixture.audioClipId(), *decodeAsset, decodedInfo);
        const auto decodeCommitting =
            runtime.automationTasks().beginCommitting(decodeRecord->taskId);
        EXPECT(checks,
               decodeValidation && decodeValidation.get().validatedOnly && decodeCommitting &&
                   decodeCommitting.get(),
               "the decoded cache must validate before entering Committing");
        if (!decodeValidation || !decodeCommitting || !decodeCommitting.get())
            return;

        auto decodeCommitContext = decodeValidationContext.get();
        decodeCommitContext.validateOnly = false;
        const auto decoded = runtime.project().applyAudioDecodeCache(
            decodeCommitContext, fixture.audioClipId(), *decodeAsset, decodedInfo);
        const bool decodeSucceeded =
            decoded && runtime.automationTasks().succeed(decodeRecord->taskId, decoded.get());
        const auto resolveTerminal =
            runtime.automationTasks().get(base.documentId, resolveRecord.taskId);
        const auto decodeTerminal =
            runtime.automationTasks().get(base.documentId, decodeRecord->taskId);
        const auto historyAfter = runtime.history().getState(base.documentId);
        EXPECT(checks,
               decodeSucceeded && resolveTerminal && decodeTerminal &&
                   resolveTerminal.get().state == Automation::AutomationTaskState::Succeeded &&
                   decodeTerminal.get().state == Automation::AutomationTaskState::Succeeded &&
                   resolveTerminal.get().mutation && decodeTerminal.get().mutation &&
                   sameAudioInfo(clip->audioInfo(), decodedInfo) &&
                   runtime.documentVersion() == base && historyAfter &&
                   sameHistoryState(historyBefore.get(), historyAfter.get()),
               "resolve and decode tasks must succeed without revision or history growth");
    }

    void testDeletedAudioTargetTerminalState(Checks &checks) {
        checks.scenario("deleted audio target rejects derived writeback with a stable failure");
        RuntimeFixture fixture;
        EXPECT(checks, fixture.isReady(), "the runtime fixture must initialize through the facade");
        if (!fixture.isReady())
            return;

        auto &runtime = fixture.runtime();
        auto *clip = fixture.audioClip();
        EXPECT(checks, clip != nullptr, "the audio task target must be addressable");
        if (!clip)
            return;
        const auto base = runtime.documentVersion();
        const auto taskAsset = Automation::audioAssetSnapshotDto(*clip);
        const auto task = runtime.automationTasks().createTask(
            Automation::OperationIds::audio_clips::apply_decode_cache, base,
            Automation::ObjectRef{Automation::ObjectKind::Clip, fixture.audioClipId().value()});
        const bool running = runtime.automationTasks().markRunning(task.taskId);
        EXPECT(checks, running, "the audio task must reach Running before target deletion");

        const auto removed =
            runtime.project().removeClips(fixture.context(), {fixture.audioClipId()});
        const auto afterRemoval = runtime.documentVersion();
        const auto historyAfterRemoval = runtime.history().getState(afterRemoval.documentId);
        EXPECT(checks,
               removed && removed.get().changed && afterRemoval.documentId == base.documentId &&
                   afterRemoval.revision == base.revision + 1 && historyAfterRemoval,
               "removing the target clip must be one ordinary document edit");
        if (!removed || !historyAfterRemoval)
            return;

        const auto writebackContext = runtime.derivedWritebackContext(base, false);
        EXPECT(checks,
               writebackContext && writebackContext.get().expected == afterRemoval &&
                   !writebackContext.get().validateOnly,
               "the deleted target writeback must rebase within the current generation");
        if (!writebackContext)
            return;

        AudioInfoModel lateInfo;
        lateInfo.sampleRate = 48000;
        lateInfo.channels = 1;
        lateInfo.frames = 480;
        const auto writeback = runtime.project().applyAudioDecodeCache(
            writebackContext.get(), fixture.audioClipId(), taskAsset, lateInfo);
        std::optional<Automation::AutomationError> writebackError;
        bool failureRecorded = false;
        if (!writeback) {
            writebackError = writeback.getError();
            failureRecorded = runtime.automationTasks().fail(task.taskId, *writebackError);
        }
        EXPECT(checks,
               writebackError &&
                   writebackError->code == Automation::AutomationErrorCode::NotFound &&
                   writebackError->operationId ==
                       Automation::OperationIds::audio_clips::apply_decode_cache &&
                   writebackError->object ==
                       std::optional(Automation::ObjectRef{Automation::ObjectKind::Clip,
                                                           fixture.audioClipId().value()}) &&
                   failureRecorded,
               "a rebased writeback must fail against the deleted Clip object");

        const auto terminal = runtime.automationTasks().get(base.documentId, task.taskId);
        EXPECT(checks,
               writebackError && terminal &&
                   terminal.get().state == Automation::AutomationTaskState::Failed &&
                   terminal.get().error &&
                   terminal.get().error->code == Automation::AutomationErrorCode::NotFound &&
                   terminal.get().error->object == writebackError->object,
               "the NotFound error must become the task's stable Failed terminal state");
        if (!terminal)
            return;

        Automation::MutationResult lateMutation;
        lateMutation.previous = afterRemoval;
        lateMutation.current = afterRemoval;
        lateMutation.changed = true;
        const bool lateSucceeded = runtime.automationTasks().succeed(task.taskId, lateMutation);
        const bool lateCanceled = runtime.automationTasks().cancel(task.taskId);
        EXPECT(checks, !lateSucceeded && !lateCanceled,
               "late success and cancellation must be rejected after terminal failure");

        const auto finalTask = runtime.automationTasks().get(base.documentId, task.taskId);
        const auto historyAfterLateCallbacks = runtime.history().getState(afterRemoval.documentId);
        EXPECT(checks,
               finalTask && finalTask.get() == terminal.get() &&
                   runtime.documentVersion() == afterRemoval && historyAfterLateCallbacks &&
                   sameHistoryState(historyAfterRemoval.get(), historyAfterLateCallbacks.get()),
               "late callbacks must not change the task, document revision, or history");
    }

    void testHashDecodeOrdering(Checks &checks, const bool hashFirst) {
        checks.scenario(hashFirst ? "hash completion before decode completion"
                                  : "decode completion before hash completion");
        RuntimeFixture fixture;
        EXPECT(checks, fixture.isReady(), "the runtime fixture must initialize through the facade");
        if (!fixture.isReady())
            return;

        auto &runtime = fixture.runtime();
        const auto taskVersion = runtime.documentVersion();
        const auto source = audioClipSnapshot(runtime, fixture.audioClipId());
        EXPECT(checks, static_cast<bool>(source), "the audio source must be queryable");
        if (!source)
            return;
        const auto taskAsset = Automation::audioAssetSnapshotDto(*fixture.audioClip());
        const auto ordinaryEdit =
            runtime.project().setTrackColor(fixture.context(), fixture.trackId(), 5);
        const auto editedVersion = runtime.documentVersion();
        const auto historyBefore = runtime.history().getState(editedVersion.documentId);
        EXPECT(checks, ordinaryEdit && ordinaryEdit.get().changed && historyBefore,
               "an unrelated edit must establish the revision-drift baseline");

        AudioInfoModel audioInfo;
        audioInfo.sampleRate = 48000;
        audioInfo.channels = 2;
        audioInfo.frames = 2048;
        audioInfo.peakCache = {
            {-9, 11}
        };
        const auto hash = sha512(QByteArray("hash-decode-ordering"));
        Automation::AutomationResult<Automation::MutationResult> first(
            Automation::AutomationError{.message = QStringLiteral("first writeback not run")});
        Automation::AutomationResult<Automation::MutationResult> second(
            Automation::AutomationError{.message = QStringLiteral("second writeback not run")});

        const auto firstContext = runtime.derivedWritebackContext(taskVersion, false);
        if (firstContext) {
            first = hashFirst
                        ? runtime.project().setAudioClipHash(firstContext.get(),
                                                             fixture.audioClipId(), taskAsset, hash)
                        : runtime.project().applyAudioDecodeCache(
                              firstContext.get(), fixture.audioClipId(), taskAsset, audioInfo);
        }
        const auto secondContext = runtime.derivedWritebackContext(taskVersion, false);
        if (secondContext) {
            second = hashFirst ? runtime.project().applyAudioDecodeCache(secondContext.get(),
                                                                         fixture.audioClipId(),
                                                                         taskAsset, audioInfo)
                               : runtime.project().setAudioClipHash(
                                     secondContext.get(), fixture.audioClipId(), taskAsset, hash);
        }

        const auto finalClip = audioClipSnapshot(runtime, fixture.audioClipId());
        const auto historyAfter = runtime.history().getState(editedVersion.documentId);
        EXPECT(checks,
               first && first.get().changed && second && second.get().changed && finalClip &&
                   finalClip->audioPathInfo.sha512 == hash &&
                   sameAudioInfo(finalClip->audioInfo, audioInfo) &&
                   finalClip->audioPathStatus == AudioClip::PathStatus::Normal &&
                   runtime.documentVersion() == editedVersion && historyBefore && historyAfter &&
                   sameHistoryState(historyBefore.get(), historyAfter.get()),
               "independent hash and decode writebacks must both survive either completion order");
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    Checks checks;
    QTemporaryDir fixtures;
    EXPECT(checks, fixtures.isValid(), "the runtime fixture root must be available");
    if (fixtures.isValid()) {
        testHitRelative(checks, fixtures.path());
        testHitSibling(checks, fixtures.path());
        testHitUnconfirmed(checks, fixtures.path());
        testHashMismatch(checks, fixtures.path());
        testCurrentDirectoryDecoy(checks, fixtures.path());
        testDirectoryCandidateRejected(checks, fixtures.path());
    }
    testDerivedAudioWritebacks(checks);
    testSamePathSourceReplacement(checks);
    testResolvedPathValidation(checks);
    testSamePathResolutionNotification(checks);
    testSamePathRelinkNotification(checks);
    testResolvedPathSourceFailureWins(checks);
    testRelinkHistoryNotifications(checks);
    testResolveDecodeTaskProtocol(checks);
    testDeletedAudioTargetTerminalState(checks);
    testHashDecodeOrdering(checks, true);
    testHashDecodeOrdering(checks, false);
    return checks.finish();
}
