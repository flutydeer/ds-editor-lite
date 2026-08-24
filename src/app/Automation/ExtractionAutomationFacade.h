#ifndef EXTRACTIONAUTOMATIONFACADE_H
#define EXTRACTIONAUTOMATIONFACADE_H

#include "AutomationDispatcher.h"
#include "AutomationTaskManager.h"
#include "DocumentObjectResolver.h"
#include "NoteAutomationFacade.h"
#include "ParameterAutomationFacade.h"
#include "ProjectAutomationFacade.h"

#include <lite/MusicBase/Timeline.h>

#include <QHash>

#include <functional>
#include <memory>
#include <optional>

namespace Automation {

    struct PitchExtractionSegmentDto {
        int globalStartTick = 0;
        QList<double> values;
    };

    struct MidiExtractionNoteDto {
        int keyIndex = 0;
        int localStart = 0;
        int length = 0;
    };

    struct PitchExtractionOptionsDto {
        QString modelId;
        std::optional<double> minimumFrequency;
        std::optional<double> maximumFrequency;
        std::function<AutomationResult<AutomationUnit>(const QString &)> authorizeSource;
    };

    struct MidiExtractionOptionsDto {
        QString modelId;
        QString defaultLanguage;
        QString defaultLyric;
        QString clientRef;
        std::optional<int> minimumNoteLength;
        QString destinationMode;
        std::optional<TrackId> targetTrackId;
        std::optional<ClipId> targetClipId;
        int targetStart = 0;
        std::function<AutomationResult<AutomationUnit>(const QString &)> authorizeSource;
    };

    struct PitchExtractionInput {
        ClipId audioClipId;
        ClipId singingClipId;
        QString audioPath;
        QString modelId;
        QString modelPath;
        Timeline timeline;
        int singingClipStartTick = 0;
        double audioMaterialOriginMs = 0.0;
        double audioVisibleStartMs = 0.0;
        double audioVisibleEndMs = 0.0;
        bool showProgressDialog = false;
        std::function<AutomationResult<AutomationUnit>(const QString &)> authorizeSource;
    };

    struct MidiExtractionInput {
        ClipId audioClipId;
        QString audioPath;
        QString modelId;
        QString modelPath;
        Timeline timeline;
        int audioClipStartTick = 0;
        int audioClipLengthTick = 0;
        QString defaultLanguage;
        QString defaultLyric;
        QString clientRef;
        std::optional<int> minimumNoteLength;
        QString destinationMode;
        std::optional<TrackId> targetTrackId;
        std::optional<ClipId> targetClipId;
        int targetStart = 0;
        bool showProgressDialog = false;
        std::function<AutomationResult<AutomationUnit>(const QString &)> authorizeSource;
    };

    enum class ExtractionBackendState {
        Succeeded,
        Failed,
        Canceled,
    };

    struct PitchExtractionBackendResult {
        ExtractionBackendState state = ExtractionBackendState::Failed;
        QList<PitchExtractionSegmentDto> segments;
        AutomationErrorCode errorCode = AutomationErrorCode::InferenceError;
        QString errorMessage;
    };

    struct MidiExtractionBackendResult {
        ExtractionBackendState state = ExtractionBackendState::Failed;
        QList<MidiExtractionNoteDto> notes;
        AutomationErrorCode errorCode = AutomationErrorCode::InferenceError;
        QString errorMessage;
    };

    struct ExtractionJobCallbacks {
        std::function<void(AutomationTaskProgress progress, const QString &message)> progress;
        std::function<void()> cancelRequested;
    };

    class IExtractionJob {
    public:
        virtual ~IExtractionJob() = default;
        virtual void cancel() = 0;
    };

    class IPitchExtractionJob : public IExtractionJob {
    public:
        virtual void start(ExtractionJobCallbacks callbacks,
                           std::function<void(PitchExtractionBackendResult)> completed) = 0;
    };

    class IMidiExtractionJob : public IExtractionJob {
    public:
        virtual void start(ExtractionJobCallbacks callbacks,
                           std::function<void(MidiExtractionBackendResult)> completed) = 0;
    };

    struct PreparedPitchExtraction {
        PitchExtractionInput input;
        std::shared_ptr<IPitchExtractionJob> job;
    };

    struct PreparedMidiExtraction {
        MidiExtractionInput input;
        std::shared_ptr<IMidiExtractionJob> job;
    };

    struct ExtractionRuntimeServices {
        std::function<AutomationResult<PreparedPitchExtraction>(PitchExtractionInput)> preparePitch;
        std::function<AutomationResult<PreparedMidiExtraction>(MidiExtractionInput)> prepareMidi;
        std::function<void(std::function<void()>)> schedule;
    };

    struct ExtractionObserver {
        std::function<void(const AutomationTaskSnapshot &)> finished;
    };

    class ExtractionAutomationFacade final {
    public:
        ExtractionAutomationFacade(OperationCatalog &catalog, AutomationDispatcher &dispatcher,
                                   AutomationTaskManager &tasks, DocumentObjectResolver &objects,
                                   ParameterAutomationFacade &parameters,
                                   ProjectAutomationFacade &project, NoteAutomationFacade &notes,
                                   ExtractionRuntimeServices services = {});

        AutomationResult<TaskAcceptedResult> startPitch(const CommandContext &context,
                                                        ClipId audioClipId, ClipId singingClipId,
                                                        ExtractionObserver observer = {});
        AutomationResult<TaskAcceptedResult> startPitch(const CommandContext &context,
                                                        ClipId audioClipId, ClipId singingClipId,
                                                        PitchExtractionOptionsDto options,
                                                        ExtractionObserver observer = {});
        AutomationResult<TaskAcceptedResult> startMidi(const CommandContext &context,
                                                       ClipId audioClipId,
                                                       ExtractionObserver observer = {});
        AutomationResult<TaskAcceptedResult> startMidi(const CommandContext &context,
                                                       ClipId audioClipId,
                                                       MidiExtractionOptionsDto options,
                                                       ExtractionObserver observer = {});

        void discardDocumentGeneration(const DocumentId &documentId);

    private:
        struct JobRecord {
            DocumentVersion baseDocument;
            std::shared_ptr<IExtractionJob> job;
        };

        void executePitchTask(const TaskId &taskId, DocumentVersion baseDocument,
                              PitchExtractionInput input, std::shared_ptr<IPitchExtractionJob> job,
                              ExtractionObserver observer);
        void executeMidiTask(const TaskId &taskId, DocumentVersion baseDocument,
                             MidiExtractionInput input, std::shared_ptr<IMidiExtractionJob> job,
                             ExtractionObserver observer);
        void completePitchTask(const TaskId &taskId, const DocumentVersion &baseDocument,
                               const PitchExtractionInput &input,
                               PitchExtractionBackendResult result,
                               const ExtractionObserver &observer);
        void completeMidiTask(const TaskId &taskId, const DocumentVersion &baseDocument,
                              const MidiExtractionInput &input, MidiExtractionBackendResult result,
                              const ExtractionObserver &observer);
        void notifyFinished(const TaskId &taskId, const DocumentId &documentId,
                            const ExtractionObserver &observer);
        void registerOperations();

        OperationCatalog &m_catalog;
        AutomationDispatcher &m_dispatcher;
        AutomationTaskManager &m_tasks;
        DocumentObjectResolver &m_objects;
        ParameterAutomationFacade &m_parameters;
        ProjectAutomationFacade &m_project;
        NoteAutomationFacade &m_notes;
        ExtractionRuntimeServices m_services;
        QHash<TaskId, JobRecord> m_jobs;
    };

} // namespace Automation

#endif // EXTRACTIONAUTOMATIONFACADE_H
