#ifndef CONTROLLEDRACESUPPORT_H
#define CONTROLLEDRACESUPPORT_H

#include "Automation/CoreRuntime.h"
#include "Automation/OperationIds.h"
#include "Automation/ProjectAutomationDtos.h"

#include <lite/History/HistoryManager.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QList>

#include <deque>
#include <functional>
#include <memory>
#include <utility>

namespace AutomationTaskRaceTests {

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

    struct ControlledPitchState {
        int startCount = 0;
        int cancelCount = 0;
        bool cancellationSeen = false;
        Automation::ExtractionJobCallbacks callbacks;
        std::function<void(Automation::PitchExtractionBackendResult)> completed;

        void complete(Automation::PitchExtractionBackendResult result) const {
            if (completed)
                completed(std::move(result));
        }
    };

    class ControlledPitchJob final : public Automation::IPitchExtractionJob {
    public:
        explicit ControlledPitchJob(std::shared_ptr<ControlledPitchState> state)
            : m_state(std::move(state)) {
        }

        void start(
            Automation::ExtractionJobCallbacks callbacks,
            std::function<void(Automation::PitchExtractionBackendResult)> completed) override {
            ++m_state->startCount;
            m_state->callbacks = std::move(callbacks);
            m_state->completed = std::move(completed);
        }

        void cancel() override {
            if (m_state->cancellationSeen)
                return;
            m_state->cancellationSeen = true;
            ++m_state->cancelCount;
        }

    private:
        std::shared_ptr<ControlledPitchState> m_state;
    };

    struct ObserverLog {
        QList<Automation::AutomationTaskSnapshot> snapshots;

        [[nodiscard]] Automation::ExtractionObserver observer() {
            return {
                .finished =
                    [this](const Automation::AutomationTaskSnapshot &snapshot) {
                        snapshots.append(snapshot);
                    },
            };
        }
    };

    class RaceFixture final {
    public:
        RaceFixture() : m_history(resetHistory()) {
            Automation::ExtractionRuntimeServices extractionServices;
            extractionServices.preparePitch = [this](Automation::PitchExtractionInput input) {
                auto state = std::make_shared<ControlledPitchState>();
                m_pitchStates.append(state);
                auto job = std::make_shared<ControlledPitchJob>(state);
                return Automation::AutomationResult<Automation::PreparedPitchExtraction>(
                    Automation::PreparedPitchExtraction{std::move(input), std::move(job)});
            };
            extractionServices.schedule = [this](std::function<void()> work) {
                m_scheduler.schedule(std::move(work));
            };

            m_runtime = std::make_unique<Automation::CoreRuntime>(
                &m_model, m_history, Automation::DocumentRuntimeServices{},
                Automation::PlaybackRuntimeServices{}, Automation::EditorRuntimeServices{},
                Automation::SettingsRuntimeServices{}, Automation::PresetRuntimeServices{},
                Automation::PackageRuntimeServices{}, Automation::InferenceRuntimeServices{},
                Automation::FileRuntimeServices{}, Automation::AudioExportRuntimeServices{},
                std::move(extractionServices), Automation::ApplicationRuntimeServices{});
            m_ready = initializeDocument();
        }

        ~RaceFixture() {
            m_runtime.reset();
            m_history->reset();
        }

        RaceFixture(const RaceFixture &) = delete;
        RaceFixture &operator=(const RaceFixture &) = delete;

        [[nodiscard]] bool isReady() const {
            return m_ready;
        }

        [[nodiscard]] Automation::CommandContext context(const bool validateOnly = false) const {
            return {
                .expected = m_runtime->documentVersion(),
                .validateOnly = validateOnly,
                .source = Automation::InvocationSource::Test,
            };
        }

        [[nodiscard]] Automation::CoreRuntime &runtime() const {
            return *m_runtime;
        }

        [[nodiscard]] ManualScheduler &scheduler() {
            return m_scheduler;
        }

        [[nodiscard]] const QList<std::shared_ptr<ControlledPitchState>> &pitchStates() const {
            return m_pitchStates;
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

        Automation::AutomationResult<Automation::TaskAcceptedResult>
            startPitch(Automation::ExtractionObserver observer = {}) {
            return m_runtime->extractions().startPitch(context(), m_audioClipId, m_singingClipId,
                                                       std::move(observer));
        }

        [[nodiscard]] static Automation::DocumentDraftDto emptyDocumentDraft() {
            AppModel replacement;
            replacement.newProject();
            auto data = replacement.takeProjectData();
            return Automation::documentDraftDto(data);
        }

        [[nodiscard]] static Automation::PitchExtractionBackendResult successfulPitch() {
            return {
                .state = Automation::ExtractionBackendState::Succeeded,
                .segments = {{.globalStartTick = 40, .values = {61.0, 61.5}}},
                .sourceSha512 = QStringLiteral("verified-source"),
                .sourceIdentityVerified = true,
            };
        }

    private:
        static HistoryManager *resetHistory() {
            auto *history = HistoryManager::instance();
            history->reset();
            return history;
        }

        bool initializeDocument() {
            Automation::TrackDraftDto track;
            track.name = QStringLiteral("Race Track");
            track.gain = 1.0;
            track.defaultLanguage = QStringLiteral("unknown");
            const auto insertedTrack = m_runtime->project().insertTrack(context(), 0, track);
            if (!insertedTrack || insertedTrack.get().affectedObjects.size() != 1)
                return false;
            m_trackId = Automation::TrackId(insertedTrack.get().affectedObjects.first().value);

            Automation::ClipDraftDto singing;
            singing.type = Automation::ClipDraftDto::Type::Singing;
            singing.properties.name = QStringLiteral("Race Singing");
            singing.properties.length = 960;
            singing.properties.clipLen = 960;
            singing.properties.gain = 1.0;
            singing.defaultLanguage = QStringLiteral("unknown");

            Automation::ClipDraftDto audio;
            audio.type = Automation::ClipDraftDto::Type::Audio;
            audio.properties.name = QStringLiteral("race-audio.wav");
            audio.properties.length = 960;
            audio.properties.clipLen = 960;
            audio.properties.gain = 1.0;
            audio.audioPath = QStringLiteral("race-audio.wav");

            const auto insertedClips = m_runtime->project().insertClips(
                context(), {
                               {.trackId = m_trackId, .clip = singing},
                               {.trackId = m_trackId, .clip = audio  }
            });
            if (!insertedClips || insertedClips.get().affectedObjects.size() != 2)
                return false;
            m_singingClipId = Automation::ClipId(insertedClips.get().affectedObjects.at(0).value);
            m_audioClipId = Automation::ClipId(insertedClips.get().affectedObjects.at(1).value);
            return true;
        }

        ManualScheduler m_scheduler;
        QList<std::shared_ptr<ControlledPitchState>> m_pitchStates;
        AppModel m_model;
        HistoryManager *m_history;
        std::unique_ptr<Automation::CoreRuntime> m_runtime;
        Automation::TrackId m_trackId;
        Automation::ClipId m_singingClipId;
        Automation::ClipId m_audioClipId;
        bool m_ready = false;
    };

} // namespace AutomationTaskRaceTests

#endif // CONTROLLEDRACESUPPORT_H
