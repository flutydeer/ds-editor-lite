#ifndef ASYNCFILEDOMAINSUPPORT_H
#define ASYNCFILEDOMAINSUPPORT_H

#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "Automation/ProjectAutomationDtos.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QTemporaryDir>
#include <QTextStream>

#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <utility>

namespace AutomationAsyncFileTests {

    class Suite final {
    public:
        template <typename Function>
        void run(const Automation::OperationId &operationId, const QString &scenario,
                 Function function) {
            m_current = operationId + QStringLiteral("/") + scenario;
            ++m_scenarios;
            ++m_operationScenarios[operationId];
            const auto before = m_failures;
            function();
            if (before == m_failures)
                ++m_passedScenarios;
        }

        void expect(const bool condition, const QString &message) {
            ++m_assertions;
            if (condition)
                return;
            ++m_failures;
            QTextStream(stderr) << "FAILED [" << m_current << "]: " << message << Qt::endl;
        }

        void requireOperations(const QList<Automation::OperationId> &operationIds) {
            run(QStringLiteral("coverage"), QStringLiteral("async-file-operation-manifest"), [&] {
                for (const auto &operationId : operationIds) {
                    expect(m_operationScenarios.value(operationId) > 0,
                           QStringLiteral("missing direct scenario for %1").arg(operationId));
                }
            });
        }

        [[nodiscard]] int finish() const {
            QTextStream(stdout) << "Automation async/file domains: " << m_scenarios
                                << " scenarios, " << m_passedScenarios << " passed, "
                                << m_assertions << " assertions, " << m_failures << " failures"
                                << Qt::endl;
            return m_failures == 0 ? 0 : 1;
        }

    private:
        QString m_current;
        QHash<Automation::OperationId, int> m_operationScenarios;
        int m_scenarios = 0;
        int m_passedScenarios = 0;
        int m_assertions = 0;
        int m_failures = 0;
    };

    class ManualScheduler final {
    public:
        void schedule(std::function<void()> work) {
            m_work.push_back(std::move(work));
        }

        [[nodiscard]] qsizetype pendingCount() const {
            return static_cast<qsizetype>(m_work.size());
        }

        bool runNext() {
            if (m_work.empty())
                return false;
            auto work = std::move(m_work.front());
            m_work.pop_front();
            work();
            return true;
        }

    private:
        std::deque<std::function<void()>> m_work;
    };

    struct FakeAudioExportState {
        int createCount = 0;
        int executeCount = 0;
        int cancelCount = 0;
        int cleanupCount = 0;
        quint32 warningFlags = 0;
        Automation::AudioExportBackendState backendState =
            Automation::AudioExportBackendState::Succeeded;
        QString backendError = QStringLiteral("controlled audio export failure");
        std::function<void()> executeHook;
    };

    class FakeAudioExportJob final : public Automation::IAudioExportJob {
    public:
        FakeAudioExportJob(Automation::AudioExportConfigDto config,
                           std::shared_ptr<FakeAudioExportState> state)
            : m_config(std::move(config)), m_state(std::move(state)) {
        }

        [[nodiscard]] Automation::AudioExportPreviewDto preview() const override {
            Automation::AudioExportPreviewDto result;
            result.baseDirectory = m_config.fileDirectory;
            result.warningFlags = m_state->warningFlags;
            if (!(m_state->warningFlags & Automation::AudioExportNoFile)) {
                result.filePaths.append(
                    QDir(m_config.fileDirectory).absoluteFilePath(m_config.fileName));
            }
            return result;
        }

        Automation::AudioExportBackendResult
            execute(const Automation::AudioExportObserver &observer) override {
            ++m_state->executeCount;
            if (observer.progress)
                observer.progress(0.75, 0);
            if (observer.warning)
                observer.warning(QStringLiteral("controlled warning"), 0);
            if (m_state->executeHook)
                m_state->executeHook();
            return {
                .state = m_state->backendState,
                .errorMessage = m_state->backendState == Automation::AudioExportBackendState::Failed
                                    ? m_state->backendError
                                    : QString(),
            };
        }

        void cancel() override {
            ++m_state->cancelCount;
        }

        void cleanup() override {
            ++m_state->cleanupCount;
        }

    private:
        Automation::AudioExportConfigDto m_config;
        std::shared_ptr<FakeAudioExportState> m_state;
    };

    struct FakePitchState {
        int startCount = 0;
        int cancelCount = 0;
        int destroyCount = 0;
        Automation::PitchExtractionInput input;
        Automation::ExtractionJobCallbacks callbacks;
        std::function<void(Automation::PitchExtractionBackendResult)> completed;

        void complete(Automation::PitchExtractionBackendResult result) const {
            if (completed)
                completed(std::move(result));
        }
    };

    class FakePitchJob final : public Automation::IPitchExtractionJob {
    public:
        explicit FakePitchJob(std::shared_ptr<FakePitchState> state) : m_state(std::move(state)) {
        }

        ~FakePitchJob() override {
            ++m_state->destroyCount;
        }

        void start(
            Automation::ExtractionJobCallbacks callbacks,
            std::function<void(Automation::PitchExtractionBackendResult)> completed) override {
            ++m_state->startCount;
            m_state->callbacks = std::move(callbacks);
            m_state->completed = std::move(completed);
            if (m_state->callbacks.progress) {
                m_state->callbacks.progress(
                    {.minimum = 0, .maximum = 100, .value = 30, .indeterminate = false},
                    QStringLiteral("controlled pitch"));
            }
        }

        void cancel() override {
            ++m_state->cancelCount;
        }

    private:
        std::shared_ptr<FakePitchState> m_state;
    };

    struct FakeMidiState {
        int startCount = 0;
        int cancelCount = 0;
        int destroyCount = 0;
        Automation::MidiExtractionInput input;
        Automation::ExtractionJobCallbacks callbacks;
        std::function<void(Automation::MidiExtractionBackendResult)> completed;

        void complete(Automation::MidiExtractionBackendResult result) const {
            if (completed)
                completed(std::move(result));
        }
    };

    class FakeMidiJob final : public Automation::IMidiExtractionJob {
    public:
        explicit FakeMidiJob(std::shared_ptr<FakeMidiState> state) : m_state(std::move(state)) {
        }

        ~FakeMidiJob() override {
            ++m_state->destroyCount;
        }

        void
            start(Automation::ExtractionJobCallbacks callbacks,
                  std::function<void(Automation::MidiExtractionBackendResult)> completed) override {
            ++m_state->startCount;
            m_state->callbacks = std::move(callbacks);
            m_state->completed = std::move(completed);
            if (m_state->callbacks.progress) {
                m_state->callbacks.progress(
                    {.minimum = 0, .maximum = 100, .value = 40, .indeterminate = false},
                    QStringLiteral("controlled midi"));
            }
        }

        void cancel() override {
            ++m_state->cancelCount;
        }

    private:
        std::shared_ptr<FakeMidiState> m_state;
    };

    struct HarnessOptions {
        bool documentServices = true;
        bool inferenceServices = true;
        bool fileServices = true;
        bool audioExportServices = true;
        bool extractionServices = true;
    };

    class RuntimeHarness final {
    public:
        explicit RuntimeHarness(const HarnessOptions &options = {})
            : m_history(HistoryManager::instance()),
              m_audioExportState(std::make_shared<FakeAudioExportState>()) {
            m_history->reset();
            m_model.newProject();
            m_runtime = std::make_unique<Automation::CoreRuntime>(
                &m_model, m_history, documentServices(options.documentServices),
                Automation::PlaybackRuntimeServices{}, Automation::EditorRuntimeServices{},
                Automation::SettingsRuntimeServices{}, Automation::PresetRuntimeServices{},
                Automation::PackageRuntimeServices{}, inferenceServices(options.inferenceServices),
                fileServices(options.fileServices),
                audioExportServices(options.audioExportServices),
                extractionServices(options.extractionServices),
                Automation::ApplicationRuntimeServices{});
            m_ready = initializeDocument();
        }

        ~RuntimeHarness() {
            m_runtime.reset();
            m_history->reset();
        }

        RuntimeHarness(const RuntimeHarness &) = delete;
        RuntimeHarness &operator=(const RuntimeHarness &) = delete;

        [[nodiscard]] bool isReady() const {
            return m_ready && m_temporaryDirectory.isValid();
        }

        [[nodiscard]] Automation::CoreRuntime &runtime() const {
            return *m_runtime;
        }

        [[nodiscard]] AppModel &model() {
            return m_model;
        }

        [[nodiscard]] Automation::TrackId trackId() const {
            return m_trackId;
        }

        [[nodiscard]] Automation::ClipId singingClipId() const {
            return m_singingClipId;
        }

        [[nodiscard]] Automation::ClipId audioClipId() const {
            return m_audioClipId;
        }

        [[nodiscard]] Automation::NoteId noteId() const {
            return m_noteId;
        }

        [[nodiscard]] QString temporaryPath(const QString &fileName) const {
            return m_temporaryDirectory.filePath(fileName);
        }

        [[nodiscard]] QString temporaryDirectoryPath() const {
            return m_temporaryDirectory.path();
        }

        [[nodiscard]] Automation::CommandContext context(const bool validateOnly = false) const {
            return {
                .expected = m_runtime->documentVersion(),
                .validateOnly = validateOnly,
                .source = Automation::InvocationSource::Test,
            };
        }

        [[nodiscard]] static Automation::CommandContext
            contextFor(const Automation::DocumentVersion &version,
                       const bool validateOnly = false) {
            return {
                .expected = version,
                .validateOnly = validateOnly,
                .source = Automation::InvocationSource::Test,
            };
        }

        [[nodiscard]] static Automation::DocumentDraftDto emptyDocument() {
            AppModel model;
            model.newProject();
            return Automation::documentDraftDto(model.takeProjectData());
        }

        bool inferenceChanged = true;
        bool inferenceAdvancesRevision = true;
        int inferencePrepareCount = 0;
        int inferenceApplyCount = 0;
        Automation::InferenceMutationKind lastPreparedInferenceKind =
            Automation::InferenceMutationKind::ApplyPronunciations;
        Automation::InferenceMutationKind lastAppliedInferenceKind =
            Automation::InferenceMutationKind::ApplyPronunciations;
        std::optional<Automation::AutomationError> inferenceError;

        bool saveSucceeds = true;
        int saveCount = 0;
        QString lastSavePath;
        int beforeReplaceCount = 0;

        bool midiExportSucceeds = true;
        int midiExportCount = 0;
        QString lastMidiExportPath;
        Automation::MidiExportOptionsDto lastMidiExportOptions;
        QList<Automation::ProjectFormatDto> formats{
            {
             .id = QStringLiteral("dspx"),
             .displayName = QStringLiteral("DSPX"),
             .extensions = {QStringLiteral("dspx")},
             .canOpen = true,
             .canImport = true,
             },
            {
             .id = QStringLiteral("midi"),
             .displayName = QStringLiteral("MIDI"),
             .extensions = {QStringLiteral("mid"), QStringLiteral("midi")},
             .canImport = true,
             .canExport = true,
             },
        };

        ManualScheduler audioScheduler;

        std::shared_ptr<FakeAudioExportState> audioExportState() const {
            return m_audioExportState;
        }

        std::optional<Automation::AutomationError> audioCreateError;

        ManualScheduler extractionScheduler;
        QList<std::shared_ptr<FakePitchState>> pitchStates;
        QList<std::shared_ptr<FakeMidiState>> midiStates;
        int pitchPrepareCount = 0;
        int midiPrepareCount = 0;
        std::optional<Automation::AutomationError> pitchPrepareError;
        std::optional<Automation::AutomationError> midiPrepareError;

    private:
        Automation::DocumentRuntimeServices documentServices(const bool enabled) {
            if (!enabled)
                return {};
            Automation::DocumentRuntimeServices services;
            services.saveProject = [this](const QString &path, AppModel *, QString &errorMessage) {
                ++saveCount;
                lastSavePath = path;
                if (!saveSucceeds)
                    errorMessage = QStringLiteral("controlled save failure");
                return saveSucceeds;
            };
            services.beforeReplaceGeneration = [this](const Automation::DocumentId &) {
                ++beforeReplaceCount;
            };
            return services;
        }

        Automation::InferenceRuntimeServices inferenceServices(const bool enabled) {
            if (!enabled)
                return {};
            Automation::InferenceRuntimeServices services;
            services.prepareMutation = [this](AppModel *model,
                                              const Automation::InferenceMutationRequest &request) {
                ++inferencePrepareCount;
                lastPreparedInferenceKind = request.kind;
                if (inferenceError)
                    return Automation::AutomationResult<Automation::PreparedInferenceMutation>(
                        *inferenceError);
                if (!request.clipId.isValid()) {
                    return Automation::AutomationResult<Automation::PreparedInferenceMutation>(
                        Automation::AutomationError::invalidArgument(
                            QStringLiteral("clip_id"),
                            QStringLiteral("Controlled clip ID is invalid")));
                }
                auto *clip = model ? model->findClipById(request.clipId.value()) : nullptr;
                if (!clip) {
                    return Automation::AutomationResult<Automation::PreparedInferenceMutation>(
                        Automation::AutomationError::notFound(
                            {Automation::ObjectKind::Clip, request.clipId.value()},
                            QStringLiteral("Controlled clip was not found")));
                }
                if (!dynamic_cast<SingingClip *>(clip)) {
                    return Automation::AutomationResult<Automation::PreparedInferenceMutation>(
                        Automation::AutomationError::wrongObjectType(
                            {Automation::ObjectKind::Clip, request.clipId.value()},
                            QStringLiteral("Controlled clip is not singing")));
                }
                if (request.pieceId.value() == 900002) {
                    return Automation::AutomationResult<Automation::PreparedInferenceMutation>(
                        Automation::AutomationError::notFound(
                            {Automation::ObjectKind::InferPiece, request.pieceId.value()},
                            QStringLiteral("Controlled piece was not found")));
                }
                for (const auto noteId : request.noteIds) {
                    if (noteId.value() == 900003) {
                        return Automation::AutomationResult<Automation::PreparedInferenceMutation>(
                            Automation::AutomationError::notFound(
                                {Automation::ObjectKind::Note, noteId.value()},
                                QStringLiteral("Controlled note was not found")));
                    }
                }

                QList<Automation::ObjectRef> affected{
                    {Automation::ObjectKind::Clip, request.clipId.value()}
                };
                if (request.pieceId.isValid()) {
                    affected.append({Automation::ObjectKind::InferPiece, request.pieceId.value()});
                }
                for (const auto noteId : request.noteIds)
                    affected.append({Automation::ObjectKind::Note, noteId.value()});

                Automation::PreparedInferenceMutation prepared;
                prepared.changed = inferenceChanged;
                prepared.advancesRevision = inferenceAdvancesRevision;
                prepared.affectedObjects = std::move(affected);
                prepared.apply = [this,
                                  request](Automation::InferenceMutationSideEffects &effects) {
                    ++inferenceApplyCount;
                    lastAppliedInferenceKind = request.kind;
                    if (request.pieceId.isValid())
                        effects.changedPieces.append(request.pieceId);
                    if (request.kind == Automation::InferenceMutationKind::InvalidateClip)
                        effects.removedPieces = request.pieceIds;
                    if (request.kind == Automation::InferenceMutationKind::ResegmentClip)
                        effects.addedPieces = request.pieceIds;
                };
                return Automation::AutomationResult<Automation::PreparedInferenceMutation>(
                    std::move(prepared));
            };
            return services;
        }

        Automation::FileRuntimeServices fileServices(const bool enabled) {
            if (!enabled)
                return {};
            Automation::FileRuntimeServices services;
            services.listProjectFormats = [this] { return formats; };
            services.exportMidi = [this](AppModel *, const QString &path,
                                         const Automation::MidiExportOptionsDto &options,
                                         QString &errorMessage) {
                ++midiExportCount;
                lastMidiExportPath = path;
                lastMidiExportOptions = options;
                if (!midiExportSucceeds)
                    errorMessage = QStringLiteral("controlled MIDI export failure");
                return midiExportSucceeds;
            };
            return services;
        }

        Automation::AudioExportRuntimeServices audioExportServices(const bool enabled) {
            if (!enabled)
                return {};
            Automation::AudioExportRuntimeServices services;
            services.createJob = [this](AppModel *, const QString &,
                                        const Automation::AudioExportConfigDto &config)
                -> Automation::AutomationResult<std::shared_ptr<Automation::IAudioExportJob>> {
                ++m_audioExportState->createCount;
                if (audioCreateError)
                    return *audioCreateError;
                std::shared_ptr<Automation::IAudioExportJob> job =
                    std::make_shared<FakeAudioExportJob>(config, m_audioExportState);
                return job;
            };
            services.schedule = [this](std::function<void()> work) {
                audioScheduler.schedule(std::move(work));
            };
            return services;
        }

        Automation::ExtractionRuntimeServices extractionServices(const bool enabled) {
            if (!enabled)
                return {};
            Automation::ExtractionRuntimeServices services;
            services.preparePitch = [this](Automation::PitchExtractionInput input) {
                ++pitchPrepareCount;
                if (pitchPrepareError) {
                    return Automation::AutomationResult<Automation::PreparedPitchExtraction>(
                        *pitchPrepareError);
                }
                input.modelPath = QStringLiteral("controlled-pitch-model");
                auto state = std::make_shared<FakePitchState>();
                state->input = input;
                pitchStates.append(state);
                std::shared_ptr<Automation::IPitchExtractionJob> job =
                    std::make_shared<FakePitchJob>(state);
                return Automation::AutomationResult<Automation::PreparedPitchExtraction>(
                    Automation::PreparedPitchExtraction{std::move(input), std::move(job)});
            };
            services.prepareMidi = [this](Automation::MidiExtractionInput input) {
                ++midiPrepareCount;
                if (midiPrepareError) {
                    return Automation::AutomationResult<Automation::PreparedMidiExtraction>(
                        *midiPrepareError);
                }
                input.modelPath = QStringLiteral("controlled-midi-model");
                input.defaultLanguage = QStringLiteral("en");
                input.defaultLyric = QStringLiteral("la");
                auto state = std::make_shared<FakeMidiState>();
                state->input = input;
                midiStates.append(state);
                std::shared_ptr<Automation::IMidiExtractionJob> job =
                    std::make_shared<FakeMidiJob>(state);
                return Automation::AutomationResult<Automation::PreparedMidiExtraction>(
                    Automation::PreparedMidiExtraction{std::move(input), std::move(job)});
            };
            services.schedule = [this](std::function<void()> work) {
                extractionScheduler.schedule(std::move(work));
            };
            return services;
        }

        bool initializeDocument() {
            Automation::TrackDraftDto track;
            track.clientRef = QStringLiteral("async-file-track");
            track.name = QStringLiteral("Async File Track");
            track.gain = 1.0;
            track.defaultLanguage = QStringLiteral("en");
            const auto insertedTrack = m_runtime->project().insertTrack(context(), 0, track);
            if (!insertedTrack || insertedTrack.get().affectedObjects.size() != 1)
                return false;
            m_trackId = Automation::TrackId(insertedTrack.get().affectedObjects.first().value);

            Automation::ClipDraftDto singing;
            singing.clientRef = QStringLiteral("async-file-singing");
            singing.type = Automation::ClipDraftDto::Type::Singing;
            singing.properties.name = QStringLiteral("Async Singing");
            singing.properties.length = 1920;
            singing.properties.clipLen = 1920;
            singing.properties.gain = 1.0;
            singing.defaultLanguage = QStringLiteral("en");

            Automation::ClipDraftDto audio;
            audio.clientRef = QStringLiteral("async-file-audio");
            audio.type = Automation::ClipDraftDto::Type::Audio;
            audio.properties.name = QStringLiteral("source.wav");
            audio.properties.length = 1920;
            audio.properties.clipLen = 1920;
            audio.properties.gain = 1.0;
            audio.audioPath = QStringLiteral("source.wav");
            audio.audioPathStatus = AudioClip::PathStatus::Missing;

            const auto insertedClips = m_runtime->project().insertClips(
                context(), {
                               {.trackId = m_trackId, .clip = singing},
                               {.trackId = m_trackId, .clip = audio  }
            });
            if (!insertedClips || insertedClips.get().affectedObjects.size() != 2)
                return false;
            m_singingClipId = Automation::ClipId(insertedClips.get().affectedObjects.at(0).value);
            m_audioClipId = Automation::ClipId(insertedClips.get().affectedObjects.at(1).value);

            Automation::NoteDraftDto note;
            note.clientRef = QStringLiteral("async-file-note");
            note.localStart = 0;
            note.length = 480;
            note.keyIndex = 60;
            note.lyric = QStringLiteral("la");
            note.language = QStringLiteral("en");
            const auto insertedNote =
                m_runtime->notes().insertNotes(context(), m_singingClipId, {note});
            if (!insertedNote || insertedNote.get().affectedObjects.size() != 1)
                return false;
            m_noteId = Automation::NoteId(insertedNote.get().affectedObjects.first().value);
            return true;
        }

        AppModel m_model;
        HistoryManager *m_history;
        QTemporaryDir m_temporaryDirectory;
        std::shared_ptr<FakeAudioExportState> m_audioExportState;
        std::unique_ptr<Automation::CoreRuntime> m_runtime;
        Automation::TrackId m_trackId;
        Automation::ClipId m_singingClipId;
        Automation::ClipId m_audioClipId;
        Automation::NoteId m_noteId;
        bool m_ready = false;
    };

    template <typename T>
    [[nodiscard]] bool isError(const Automation::AutomationResult<T> &result,
                               const Automation::AutomationErrorCode code,
                               const Automation::OperationId &operationId = {}) {
        return !result && result.getError().code == code &&
               (operationId.isEmpty() || result.getError().operationId == operationId);
    }

} // namespace AutomationAsyncFileTests

#endif // ASYNCFILEDOMAINSUPPORT_H
