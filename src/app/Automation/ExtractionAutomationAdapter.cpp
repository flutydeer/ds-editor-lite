#include "ExtractionAutomationAdapter.h"

#include "Controller/Tasks/ComputeAudioHashTask.h"
#include "Model/AppOptions/AppOptions.h"
#include "Modules/Extractors/ExtractMidiTask.h"
#include "Modules/Extractors/ExtractPitchTask.h"
#include "UI/Dialogs/Base/TaskDialog.h"

#include <lite/Tasking/TaskManager.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QThreadPool>
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
            result.audioPath = input.snapshotPath.isEmpty() ? input.audioPath : input.snapshotPath;
            result.displayAudioPath = input.audioPath;
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
            result.audioPath = input.snapshotPath.isEmpty() ? input.audioPath : input.snapshotPath;
            result.displayAudioPath = input.audioPath;
            result.modelPath = input.modelPath;
            result.timeline = input.timeline;
            result.audioClipStartTick = input.audioClipStartTick;
            result.audioClipLengthTick = input.audioClipLengthTick;
            return result;
        }

        ComputeAudioHashTask *startAudioHashTask(
            const QString &path, const QString &snapshotPath,
            std::function<void(ComputeAudioHashTask *)> finished) {
            auto *task = new ComputeAudioHashTask;
            task->path = path;
            task->snapshotPath = snapshotPath;
            auto *application = QCoreApplication::instance();
            QObject *connectionContext = application ? static_cast<QObject *>(application)
                                                     : static_cast<QObject *>(task);
            QObject::connect(task, &Task::finished, connectionContext,
                             [task, finished = std::move(finished)] { finished(task); },
                             Qt::QueuedConnection);
            QThreadPool::globalInstance()->start(task);
            return task;
        }

        class PitchExtractionJobAdapter final
            : public IPitchExtractionJob,
              public std::enable_shared_from_this<PitchExtractionJobAdapter> {
        public:
            PitchExtractionJobAdapter(PitchExtractionInput input, TaskManager *taskRuntime)
                : m_input(std::move(input)), m_taskManager(taskRuntime) {
            }

            void start(ExtractionJobCallbacks callbacks,
                       std::function<void(PitchExtractionBackendResult)> completed) override {
                m_callbacks = std::move(callbacks);
                m_completed = std::move(completed);
                if (m_canceled) {
                    finish({.state = ExtractionBackendState::Canceled});
                    return;
                }
                m_snapshotDirectory = std::make_unique<QTemporaryDir>();
                if (!m_snapshotDirectory->isValid()) {
                    fail(AutomationErrorCode::IoError,
                         QStringLiteral("Failed to create an audio snapshot directory"));
                    return;
                }
                m_input.snapshotPath = QDir(m_snapshotDirectory->path())
                                           .filePath(QFileInfo(m_input.audioPath).fileName());
                const auto self = shared_from_this();
                m_hashTask = startAudioHashTask(
                    m_input.audioPath, m_input.snapshotPath,
                    [self](ComputeAudioHashTask *task) { self->handleSnapshotFinished(task); });
                if (m_callbacks.progress) {
                    m_callbacks.progress({.indeterminate = true},
                                         QStringLiteral("Creating an audio snapshot"));
                }
            }

            void cancel() override {
                m_canceled = true;
                if (m_hashTask)
                    m_hashTask->terminate();
                if (m_task)
                    m_task->terminate();
            }

        private:
            void handleSnapshotFinished(ComputeAudioHashTask *task) {
                m_hashTask = nullptr;
                if (m_canceled || task->terminated()) {
                    task->deleteLater();
                    finish({.state = ExtractionBackendState::Canceled});
                    return;
                }
                if (!task->success || task->resultSha512.isEmpty()) {
                    task->deleteLater();
                    fail(AutomationErrorCode::IoError,
                         QStringLiteral("Failed to snapshot the source audio"));
                    return;
                }
                if (!m_input.sourceAsset.pathInfo.sha512.isEmpty() &&
                    m_input.sourceAsset.pathInfo.sha512 != task->resultSha512) {
                    task->deleteLater();
                    fail(AutomationErrorCode::InvalidArgument,
                         QStringLiteral("The source audio does not match the selected clip"));
                    return;
                }
                task->deleteLater();
                startExtraction();
            }

            void startExtraction() {
                auto *task = new ExtractPitchTask(taskInput(m_input));
                m_task = task;
                const auto self = shared_from_this();
                auto *application = QCoreApplication::instance();
                QObject *connectionContext = application ? static_cast<QObject *>(application)
                                                         : static_cast<QObject *>(task);
                QObject::connect(task, &Task::statusUpdated, connectionContext,
                                 [callback = m_callbacks.progress](const TaskStatus &status) {
                                     if (callback)
                                         callback(progressFromStatus(status), status.message);
                                 });
                QObject::connect(
                    task, &Task::finished, connectionContext,
                    [self, task] { self->handleExtractionFinished(task); });
                if (m_input.showProgressDialog) {
                    auto *dialog = new TaskDialog(task, true, true);
                    dialog->setCancelCallback(m_callbacks.cancelRequested);
                    dialog->show();
                }
                if (m_callbacks.progress) {
                    m_callbacks.progress(progressFromStatus(task->status()),
                                         task->status().message);
                }
                m_taskManager->addAndStartTask(task);
            }

            void handleExtractionFinished(ExtractPitchTask *task) {
                m_taskManager->removeTask(task);
                m_task = nullptr;
                if (task->success()) {
                    m_result.state = ExtractionBackendState::Succeeded;
                    m_result.segments.reserve(task->result.size());
                    for (const auto &segment : task->result)
                        m_result.segments.append({segment.globalStartTick, segment.values});
                } else if (task->errorCode() == ExtractTask::ErrorCode::Terminated) {
                    m_result.state = ExtractionBackendState::Canceled;
                } else {
                    m_result.state = ExtractionBackendState::Failed;
                    m_result.errorCode = taskErrorCode(task->errorCode());
                    m_result.errorMessage = task->errorMessage();
                }
                delete task;
                finish(std::move(m_result));
            }

            void fail(const AutomationErrorCode code, QString message) {
                finish({.state = ExtractionBackendState::Failed,
                        .errorCode = code,
                        .errorMessage = std::move(message)});
            }

            void finish(PitchExtractionBackendResult result) {
                if (m_finished)
                    return;
                m_finished = true;
                if (m_completed)
                    m_completed(std::move(result));
            }

            PitchExtractionInput m_input;
            TaskManager *m_taskManager = nullptr;
            std::unique_ptr<QTemporaryDir> m_snapshotDirectory;
            ComputeAudioHashTask *m_hashTask = nullptr;
            ExtractPitchTask *m_task = nullptr;
            ExtractionJobCallbacks m_callbacks;
            std::function<void(PitchExtractionBackendResult)> m_completed;
            PitchExtractionBackendResult m_result;
            bool m_canceled = false;
            bool m_finished = false;
        };

        class MidiExtractionJobAdapter final
            : public IMidiExtractionJob,
              public std::enable_shared_from_this<MidiExtractionJobAdapter> {
        public:
            MidiExtractionJobAdapter(MidiExtractionInput input, TaskManager *taskRuntime)
                : m_input(std::move(input)), m_taskManager(taskRuntime) {
            }

            void start(ExtractionJobCallbacks callbacks,
                       std::function<void(MidiExtractionBackendResult)> completed) override {
                m_callbacks = std::move(callbacks);
                m_completed = std::move(completed);
                if (m_canceled) {
                    finish({.state = ExtractionBackendState::Canceled});
                    return;
                }
                m_snapshotDirectory = std::make_unique<QTemporaryDir>();
                if (!m_snapshotDirectory->isValid()) {
                    fail(AutomationErrorCode::IoError,
                         QStringLiteral("Failed to create an audio snapshot directory"));
                    return;
                }
                m_input.snapshotPath = QDir(m_snapshotDirectory->path())
                                           .filePath(QFileInfo(m_input.audioPath).fileName());
                const auto self = shared_from_this();
                m_hashTask = startAudioHashTask(
                    m_input.audioPath, m_input.snapshotPath,
                    [self](ComputeAudioHashTask *task) { self->handleSnapshotFinished(task); });
                if (m_callbacks.progress) {
                    m_callbacks.progress({.indeterminate = true},
                                         QStringLiteral("Creating an audio snapshot"));
                }
            }

            void cancel() override {
                m_canceled = true;
                if (m_hashTask)
                    m_hashTask->terminate();
                if (m_task)
                    m_task->terminate();
            }

        private:
            void handleSnapshotFinished(ComputeAudioHashTask *task) {
                m_hashTask = nullptr;
                if (m_canceled || task->terminated()) {
                    task->deleteLater();
                    finish({.state = ExtractionBackendState::Canceled});
                    return;
                }
                if (!task->success || task->resultSha512.isEmpty()) {
                    task->deleteLater();
                    fail(AutomationErrorCode::IoError,
                         QStringLiteral("Failed to snapshot the source audio"));
                    return;
                }
                if (!m_input.sourceAsset.pathInfo.sha512.isEmpty() &&
                    m_input.sourceAsset.pathInfo.sha512 != task->resultSha512) {
                    task->deleteLater();
                    fail(AutomationErrorCode::InvalidArgument,
                         QStringLiteral("The source audio does not match the selected clip"));
                    return;
                }
                task->deleteLater();
                startExtraction();
            }

            void startExtraction() {
                auto *task = new ExtractMidiTask(taskInput(m_input));
                m_task = task;
                const auto self = shared_from_this();
                auto *application = QCoreApplication::instance();
                QObject *connectionContext = application ? static_cast<QObject *>(application)
                                                         : static_cast<QObject *>(task);
                QObject::connect(task, &Task::statusUpdated, connectionContext,
                                 [callback = m_callbacks.progress](const TaskStatus &status) {
                                     if (callback)
                                         callback(progressFromStatus(status), status.message);
                                 });
                QObject::connect(
                    task, &Task::finished, connectionContext,
                    [self, task] { self->handleExtractionFinished(task); });
                if (m_input.showProgressDialog) {
                    auto *dialog = new TaskDialog(task, true, true);
                    dialog->setCancelCallback(m_callbacks.cancelRequested);
                    dialog->show();
                }
                if (m_callbacks.progress) {
                    m_callbacks.progress(progressFromStatus(task->status()),
                                         task->status().message);
                }
                m_taskManager->addAndStartTask(task);
            }

            void handleExtractionFinished(ExtractMidiTask *task) {
                m_taskManager->removeTask(task);
                m_task = nullptr;
                if (task->success()) {
                    m_result.state = ExtractionBackendState::Succeeded;
                    m_result.notes.reserve(static_cast<qsizetype>(task->result.size()));
                    for (const auto &note : task->result)
                        m_result.notes.append({note.note, note.start, note.duration});
                } else if (task->errorCode() == ExtractTask::ErrorCode::Terminated) {
                    m_result.state = ExtractionBackendState::Canceled;
                } else {
                    m_result.state = ExtractionBackendState::Failed;
                    m_result.errorCode = taskErrorCode(task->errorCode());
                    m_result.errorMessage = task->errorMessage();
                }
                delete task;
                finish(std::move(m_result));
            }

            void fail(const AutomationErrorCode code, QString message) {
                finish({.state = ExtractionBackendState::Failed,
                        .errorCode = code,
                        .errorMessage = std::move(message)});
            }

            void finish(MidiExtractionBackendResult result) {
                if (m_finished)
                    return;
                m_finished = true;
                if (m_completed)
                    m_completed(std::move(result));
            }

            MidiExtractionInput m_input;
            TaskManager *m_taskManager = nullptr;
            std::unique_ptr<QTemporaryDir> m_snapshotDirectory;
            ComputeAudioHashTask *m_hashTask = nullptr;
            ExtractMidiTask *m_task = nullptr;
            ExtractionJobCallbacks m_callbacks;
            std::function<void(MidiExtractionBackendResult)> m_completed;
            MidiExtractionBackendResult m_result;
            bool m_canceled = false;
            bool m_finished = false;
        };
    }

    ExtractionRuntimeServices createExtractionAutomationServices(AppOptions *options,
                                                                 TaskManager *taskRuntime) {
        ExtractionRuntimeServices services;
        services.preparePitch =
            [options,
             taskRuntime](PitchExtractionInput input) -> AutomationResult<PreparedPitchExtraction> {
            if (!options || !options->general() || !taskRuntime) {
                AutomationError error;
                error.code = AutomationErrorCode::ModuleNotReady;
                error.message = QStringLiteral("Application options are unavailable");
                return error;
            }
            auto valid = validateAudioPath(input.audioPath);
            if (!valid)
                return valid.getError();
            if (!input.modelId.isEmpty() && input.modelId != QStringLiteral("rmvpe")) {
                return AutomationError::invalidArgument(
                    QStringLiteral("options.model_id"),
                    QStringLiteral("The requested pitch extraction model is unavailable"));
            }
            input.modelPath = options->general()->rmvpePath;
            valid = validateModelPath(input.modelPath, false, QStringLiteral("rmvpe_model_path"),
                                      QStringLiteral("RMVPE model"));
            if (!valid)
                return valid.getError();
            auto job = std::make_shared<PitchExtractionJobAdapter>(input, taskRuntime);
            return PreparedPitchExtraction{std::move(input), std::move(job)};
        };
        services.prepareMidi =
            [options,
             taskRuntime](MidiExtractionInput input) -> AutomationResult<PreparedMidiExtraction> {
            if (!options || !options->general() || !taskRuntime) {
                AutomationError error;
                error.code = AutomationErrorCode::ModuleNotReady;
                error.message = QStringLiteral("Application options are unavailable");
                return error;
            }
            auto valid = validateAudioPath(input.audioPath);
            if (!valid)
                return valid.getError();
            if (!input.modelId.isEmpty() && input.modelId != QStringLiteral("game")) {
                return AutomationError::invalidArgument(
                    QStringLiteral("options.model_id"),
                    QStringLiteral("The requested MIDI extraction model is unavailable"));
            }
            input.modelPath = options->general()->gameDir;
            valid = validateModelPath(input.modelPath, true, QStringLiteral("game_model_path"),
                                      QStringLiteral("GAME model directory"));
            if (!valid)
                return valid.getError();
            if (input.defaultLanguage.isEmpty())
                input.defaultLanguage = options->general()->defaultSingingLanguage;
            if (input.defaultLyric.isEmpty())
                input.defaultLyric =
                    options->general()->defaultLyricForLanguage(input.defaultLanguage);
            auto job = std::make_shared<MidiExtractionJobAdapter>(input, taskRuntime);
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
