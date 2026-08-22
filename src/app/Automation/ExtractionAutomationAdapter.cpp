#include "ExtractionAutomationAdapter.h"

#include "Model/AppOptions/AppOptions.h"
#include "Modules/Extractors/ExtractMidiTask.h"
#include "Modules/Extractors/ExtractPitchTask.h"
#include "UI/Dialogs/Base/TaskDialog.h"

#include <lite/Tasking/TaskManager.h>

#include <QCoreApplication>
#include <QFileInfo>
#include <QTimer>

#include <utility>

namespace Automation {
    namespace {
        AutomationResult<AutomationUnit> validateAudioPath(const QString &path) {
            if (path.trimmed().isEmpty()) {
                AutomationError error;
                error.code = AutomationErrorCode::PathRequired;
                error.fieldPath = QStringLiteral("audio_clip_id");
                error.message = QStringLiteral("Audio clip path is required for extraction");
                return error;
            }
            const QFileInfo file(path);
            if (!file.exists() || !file.isFile()) {
                AutomationError error;
                error.code = AutomationErrorCode::FileNotFound;
                error.fieldPath = QStringLiteral("audio_clip_id");
                error.message = QStringLiteral("Audio clip file was not found");
                return error;
            }
            return AutomationUnit{};
        }

        AutomationResult<AutomationUnit> validateModelPath(const QString &path,
                                                           const bool directory,
                                                           const QString &fieldPath,
                                                           const QString &displayName) {
            if (path.trimmed().isEmpty()) {
                AutomationError error;
                error.code = AutomationErrorCode::ModuleNotReady;
                error.fieldPath = fieldPath;
                error.message = displayName + QStringLiteral(" is not configured");
                return error;
            }
            const QFileInfo file(path);
            if (!file.exists() || (directory ? !file.isDir() : !file.isFile())) {
                AutomationError error;
                error.code = AutomationErrorCode::FileNotFound;
                error.fieldPath = fieldPath;
                error.message = displayName + QStringLiteral(" was not found");
                return error;
            }
            return AutomationUnit{};
        }

        AutomationTaskProgress progressFromStatus(const TaskStatus &status) {
            return {
                .minimum = status.minimum,
                .maximum = status.maximum,
                .value = status.progress,
                .indeterminate = status.isIndetermine,
            };
        }

        AutomationErrorCode taskErrorCode(const ExtractTask::ErrorCode code) {
            switch (code) {
                case ExtractTask::ErrorCode::InferEngineNotLoaded:
                case ExtractTask::ErrorCode::ModelNotLoaded:
                    return AutomationErrorCode::ModuleNotReady;
                case ExtractTask::ErrorCode::ModelRunFailed:
                case ExtractTask::ErrorCode::UnknownError:
                    return AutomationErrorCode::InferenceError;
                case ExtractTask::ErrorCode::Terminated:
                case ExtractTask::ErrorCode::Success:
                    break;
            }
            return AutomationErrorCode::InferenceError;
        }

        ExtractTask::Input taskInput(const PitchExtractionInput &input) {
            ExtractTask::Input result;
            result.singingClipId = input.singingClipId.value();
            result.audioClipId = input.audioClipId.value();
            result.audioPath = input.audioPath;
            result.modelPath = input.modelPath;
            result.timeline = input.timeline;
            result.singingClipStartTick = input.singingClipStartTick;
            result.audioMaterialOriginMs = input.audioMaterialOriginMs;
            result.audioVisibleStartMs = input.audioVisibleStartMs;
            result.audioVisibleEndMs = input.audioVisibleEndMs;
            return result;
        }

        ExtractTask::Input taskInput(const MidiExtractionInput &input) {
            ExtractTask::Input result;
            result.audioClipId = input.audioClipId.value();
            result.audioPath = input.audioPath;
            result.modelPath = input.modelPath;
            result.timeline = input.timeline;
            result.audioClipStartTick = input.audioClipStartTick;
            result.audioClipLengthTick = input.audioClipLengthTick;
            return result;
        }

        class PitchExtractionJobAdapter final
            : public IPitchExtractionJob,
              public std::enable_shared_from_this<PitchExtractionJobAdapter> {
        public:
            explicit PitchExtractionJobAdapter(PitchExtractionInput input)
                : m_input(std::move(input)) {
            }

            void start(ExtractionJobCallbacks callbacks,
                       std::function<void(PitchExtractionBackendResult)> completed) override {
                if (m_canceled) {
                    completed({.state = ExtractionBackendState::Canceled});
                    return;
                }
                auto *task = new ExtractPitchTask(taskInput(m_input));
                m_task = task;
                const auto self = shared_from_this();
                auto *application = QCoreApplication::instance();
                QObject *connectionContext = application ? static_cast<QObject *>(application)
                                                         : static_cast<QObject *>(task);
                QObject::connect(task, &Task::statusUpdated, connectionContext,
                                 [callback = callbacks.progress](const TaskStatus &status) {
                                     if (callback)
                                         callback(progressFromStatus(status), status.message);
                                 });
                QObject::connect(
                    task, &Task::finished, connectionContext,
                    [self, task, completed = std::move(completed)]() mutable {
                        taskManager->removeTask(task);
                        PitchExtractionBackendResult result;
                        if (task->success()) {
                            result.state = ExtractionBackendState::Succeeded;
                            result.segments.reserve(task->result.size());
                            for (const auto &segment : task->result) {
                                result.segments.append({segment.globalStartTick, segment.values});
                            }
                        } else if (task->errorCode() == ExtractTask::ErrorCode::Terminated) {
                            result.state = ExtractionBackendState::Canceled;
                        } else {
                            result.state = ExtractionBackendState::Failed;
                            result.errorCode = taskErrorCode(task->errorCode());
                            result.errorMessage = task->errorMessage();
                        }
                        self->m_task = nullptr;
                        completed(std::move(result));
                        delete task;
                    });
                if (m_input.showProgressDialog) {
                    auto *dialog = new TaskDialog(task, true, true);
                    dialog->setCancelCallback(std::move(callbacks.cancelRequested));
                    dialog->show();
                }
                if (callbacks.progress)
                    callbacks.progress(progressFromStatus(task->status()), task->status().message);
                taskManager->addAndStartTask(task);
            }

            void cancel() override {
                m_canceled = true;
                if (m_task)
                    m_task->terminate();
            }

        private:
            PitchExtractionInput m_input;
            ExtractPitchTask *m_task = nullptr;
            bool m_canceled = false;
        };

        class MidiExtractionJobAdapter final
            : public IMidiExtractionJob,
              public std::enable_shared_from_this<MidiExtractionJobAdapter> {
        public:
            explicit MidiExtractionJobAdapter(MidiExtractionInput input)
                : m_input(std::move(input)) {
            }

            void start(ExtractionJobCallbacks callbacks,
                       std::function<void(MidiExtractionBackendResult)> completed) override {
                if (m_canceled) {
                    completed({.state = ExtractionBackendState::Canceled});
                    return;
                }
                auto *task = new ExtractMidiTask(taskInput(m_input));
                m_task = task;
                const auto self = shared_from_this();
                auto *application = QCoreApplication::instance();
                QObject *connectionContext = application ? static_cast<QObject *>(application)
                                                         : static_cast<QObject *>(task);
                QObject::connect(task, &Task::statusUpdated, connectionContext,
                                 [callback = callbacks.progress](const TaskStatus &status) {
                                     if (callback)
                                         callback(progressFromStatus(status), status.message);
                                 });
                QObject::connect(
                    task, &Task::finished, connectionContext,
                    [self, task, completed = std::move(completed)]() mutable {
                        taskManager->removeTask(task);
                        MidiExtractionBackendResult result;
                        if (task->success()) {
                            result.state = ExtractionBackendState::Succeeded;
                            result.notes.reserve(static_cast<qsizetype>(task->result.size()));
                            for (const auto &note : task->result)
                                result.notes.append({note.note, note.start, note.duration});
                        } else if (task->errorCode() == ExtractTask::ErrorCode::Terminated) {
                            result.state = ExtractionBackendState::Canceled;
                        } else {
                            result.state = ExtractionBackendState::Failed;
                            result.errorCode = taskErrorCode(task->errorCode());
                            result.errorMessage = task->errorMessage();
                        }
                        self->m_task = nullptr;
                        completed(std::move(result));
                        delete task;
                    });
                if (m_input.showProgressDialog) {
                    auto *dialog = new TaskDialog(task, true, true);
                    dialog->setCancelCallback(std::move(callbacks.cancelRequested));
                    dialog->show();
                }
                if (callbacks.progress)
                    callbacks.progress(progressFromStatus(task->status()), task->status().message);
                taskManager->addAndStartTask(task);
            }

            void cancel() override {
                m_canceled = true;
                if (m_task)
                    m_task->terminate();
            }

        private:
            MidiExtractionInput m_input;
            ExtractMidiTask *m_task = nullptr;
            bool m_canceled = false;
        };
    }

    ExtractionRuntimeServices createExtractionAutomationServices(AppOptions *options) {
        ExtractionRuntimeServices services;
        services.preparePitch =
            [options](PitchExtractionInput input) -> AutomationResult<PreparedPitchExtraction> {
            if (!options || !options->general()) {
                AutomationError error;
                error.code = AutomationErrorCode::ModuleNotReady;
                error.message = QStringLiteral("Application options are unavailable");
                return error;
            }
            auto valid = validateAudioPath(input.audioPath);
            if (!valid)
                return valid.getError();
            input.modelPath = options->general()->rmvpePath;
            valid = validateModelPath(input.modelPath, false, QStringLiteral("rmvpe_model_path"),
                                      QStringLiteral("RMVPE model"));
            if (!valid)
                return valid.getError();
            auto job = std::make_shared<PitchExtractionJobAdapter>(input);
            return PreparedPitchExtraction{std::move(input), std::move(job)};
        };
        services.prepareMidi =
            [options](MidiExtractionInput input) -> AutomationResult<PreparedMidiExtraction> {
            if (!options || !options->general()) {
                AutomationError error;
                error.code = AutomationErrorCode::ModuleNotReady;
                error.message = QStringLiteral("Application options are unavailable");
                return error;
            }
            auto valid = validateAudioPath(input.audioPath);
            if (!valid)
                return valid.getError();
            input.modelPath = options->general()->gameDir;
            valid = validateModelPath(input.modelPath, true, QStringLiteral("game_model_path"),
                                      QStringLiteral("GAME model directory"));
            if (!valid)
                return valid.getError();
            input.defaultLanguage = options->general()->defaultSingingLanguage;
            input.defaultLyric = options->general()->defaultLyricForLanguage(input.defaultLanguage);
            auto job = std::make_shared<MidiExtractionJobAdapter>(input);
            return PreparedMidiExtraction{std::move(input), std::move(job)};
        };
        services.schedule = [](std::function<void()> execute) {
            if (auto *application = QCoreApplication::instance()) {
                QTimer::singleShot(0, application,
                                   [execute = std::move(execute)]() mutable { execute(); });
                return;
            }
            execute();
        };
        return services;
    }

} // namespace Automation
