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
#include <QPointer>

#include <algorithm>

#include <lite/SynthrtEngine/SynthrtEngine.h>
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

        /// Checks that \a reference identifies an installed analyzer that implements
        /// \a interfaceName.
        ///
        /// The check queries the engine rather than the filesystem. An analyzer is a contribution
        /// of a package, so its availability depends on the installed packages and its suitability
        /// depends on the contract it declares. The existence of a file at a path indicates
        /// neither. A path-based check would accept a note model for pitch extraction and report
        /// the mismatch only at run time.
        AutomationResult<lite::synthrt::AnalyzerEntry> validateAnalyzer(
            const QString &reference, const QString &interfaceName, const QString &fieldPath,
            const QString &displayName) {
            if (reference.trimmed().isEmpty()) {
                AutomationError error;
                error.code = AutomationErrorCode::ModuleNotReady;
                error.fieldPath = fieldPath;
                error.message = displayName + QStringLiteral(" is not configured");
                return error;
            }
            const auto installed = SynthrtEngine::instance().analyzers(interfaceName);
            const auto wanted = reference.trimmed().toStdString();
            const auto found =
                std::find_if(installed.cbegin(), installed.cend(),
                             [&wanted](const lite::synthrt::AnalyzerEntry &entry) {
                                 return entry.reference() == wanted;
                             });
            if (found == installed.cend()) {
                AutomationError error;
                error.code = AutomationErrorCode::FileNotFound;
                error.fieldPath = fieldPath;
                error.message = displayName + QStringLiteral(" is not installed: ") + reference;
                return error;
            }
            return *found;
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
            result.audioPath = input.snapshotPath.isEmpty() ? input.audioPath : input.snapshotPath;
            result.displayAudioPath = input.audioPath;
            result.analyzer = input.analyzer;
            result.timeline = input.timeline;
            result.singingClipStartTick = input.singingClipStartTick;
            result.audioMaterialOriginMs = input.audioMaterialOriginMs;
            result.audioVisibleStartMs = input.audioVisibleStartMs;
            result.audioVisibleEndMs = input.audioVisibleEndMs;
            return result;
        }

        ExtractTask::Input taskInput(const MidiExtractionInput &input) {
            ExtractTask::Input result;
            result.audioPath = input.snapshotPath.isEmpty() ? input.audioPath : input.snapshotPath;
            result.displayAudioPath = input.audioPath;
            result.analyzer = input.analyzer;
            // The language resolved above, from the request or from the default singing language
            // of the editor, is passed to the task. Without a language, a multilingual note model
            // transcribes without language conditioning.
            result.language = input.defaultLanguage;
            result.timeline = input.timeline;
            result.audioClipStartTick = input.audioClipStartTick;
            result.audioMaterialOriginMs = input.audioMaterialOriginMs;
            result.audioVisibleStartMs = input.audioVisibleStartMs;
            result.audioVisibleEndMs = input.audioVisibleEndMs;
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

        /// Differences between the pitch and MIDI extraction jobs: the types, and the conversion
        /// of a finished task's result into the backend result.
        struct PitchJobTraits {
            using Job = IPitchExtractionJob;
            using Input = PitchExtractionInput;
            using Task = ExtractPitchTask;
            using Result = PitchExtractionBackendResult;

            static void takeResult(const Task &task, Result &result) {
                result.segments.reserve(task.result.size());
                for (const auto &segment : task.result)
                    result.segments.append({segment.globalStartTick, segment.values});
            }
        };

        struct MidiJobTraits {
            using Job = IMidiExtractionJob;
            using Input = MidiExtractionInput;
            using Task = ExtractMidiTask;
            using Result = MidiExtractionBackendResult;

            static void takeResult(const Task &task, Result &result) {
                result.notes.reserve(static_cast<qsizetype>(task.result.size()));
                for (const auto &note : task.result)
                    result.notes.append({note.note, note.start, note.duration});
            }
        };

        /// Runs one extraction: snapshots the source audio, checks it against the clip, runs the
        /// extraction task and reports the result once.
        template <class Traits>
        class ExtractionJobAdapter final
            : public Traits::Job,
              public std::enable_shared_from_this<ExtractionJobAdapter<Traits>> {
        public:
            using Input = typename Traits::Input;
            using Task = typename Traits::Task;
            using Result = typename Traits::Result;

            ExtractionJobAdapter(Input input, TaskManager *taskRuntime)
                : m_input(std::move(input)), m_taskManager(taskRuntime) {
            }

            void start(ExtractionJobCallbacks callbacks,
                       std::function<void(Result)> completed) override {
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
                const auto self = this->shared_from_this();
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
                if (!m_taskManager) {
                    fail(AutomationErrorCode::ModuleNotReady,
                         QStringLiteral("The task manager is unavailable"));
                    return;
                }
                auto *task = new Task(taskInput(m_input));
                m_task = task;
                const auto self = this->shared_from_this();
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

            void handleExtractionFinished(Task *task) {
                if (m_taskManager)
                    m_taskManager->removeTask(task);
                m_task = nullptr;
                if (task->success()) {
                    m_result.state = ExtractionBackendState::Succeeded;
                    Traits::takeResult(*task, m_result);
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
                Result result;
                result.state = ExtractionBackendState::Failed;
                result.errorCode = code;
                result.errorMessage = std::move(message);
                finish(std::move(result));
            }

            void finish(Result result) {
                if (m_finished)
                    return;
                m_finished = true;
                if (m_completed)
                    m_completed(std::move(result));
            }

            Input m_input;
            // Guarded because a finished task is delivered through the event queue and may arrive
            // after teardown has drained the task manager.
            QPointer<TaskManager> m_taskManager;
            std::unique_ptr<QTemporaryDir> m_snapshotDirectory;
            ComputeAudioHashTask *m_hashTask = nullptr;
            Task *m_task = nullptr;
            ExtractionJobCallbacks m_callbacks;
            std::function<void(Result)> m_completed;
            Result m_result;
            bool m_canceled = false;
            bool m_finished = false;
        };

        using PitchExtractionJobAdapter = ExtractionJobAdapter<PitchJobTraits>;
        using MidiExtractionJobAdapter = ExtractionJobAdapter<MidiJobTraits>;
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
            input.analyzer = options->general()->pitchAnalyzer;
            // options.model_id contains an analyzer reference as listed in the capability report.
            // Only the configured analyzer runs, so a different reference is rejected rather than
            // silently ignored.
            if (!input.modelId.isEmpty() && input.modelId != input.analyzer.trimmed()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("options.model_id"),
                    QStringLiteral("The requested pitch extraction model is not the configured "
                                   "pitch analyzer"));
            }
            const auto analyzer = validateAnalyzer(input.analyzer, lite::synthrt::f0Contract(),
                                                   QStringLiteral("pitch_analyzer"),
                                                   QStringLiteral("The pitch analyzer"));
            if (!analyzer)
                return analyzer.getError();
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
            input.analyzer = options->general()->noteAnalyzer;
            if (!input.modelId.isEmpty() && input.modelId != input.analyzer.trimmed()) {
                return AutomationError::invalidArgument(
                    QStringLiteral("options.model_id"),
                    QStringLiteral("The requested MIDI extraction model is not the configured "
                                   "note analyzer"));
            }
            const auto analyzer = validateAnalyzer(input.analyzer, lite::synthrt::noteContract(),
                                                   QStringLiteral("note_analyzer"),
                                                   QStringLiteral("The note analyzer"));
            if (!analyzer)
                return analyzer.getError();
            // The language that the transcription uses labels the new notes, clip and track. A
            // request without a language uses the default that the analyzer declares, and a
            // language that the analyzer does not declare is replaced by that default, matching
            // the replacement in the task. If the analyzer distinguishes no languages, the label
            // is the requested language or, if none is requested, the default singing language of
            // the editor.
            input.defaultLanguage = QString::fromStdString(
                analyzer.get().effectiveLanguage(input.defaultLanguage.toStdString()));
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
