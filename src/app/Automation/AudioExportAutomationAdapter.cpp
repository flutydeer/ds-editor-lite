#include "AudioExportAutomationAdapter.h"

#include "Modules/Audio/AudioContext.h"
#include "Modules/Audio/AudioExporter.h"
#include "Modules/Audio/AudioExporter_p.h"

#include <lite/ProjectModel/AppModel/AppModel.h>

#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QPointer>
#include <QTimer>

#include <atomic>

namespace Automation {

    AudioExportConfigDto toAutomationDto(const Audio::AudioExporterConfig &config) {
        return {
            .fileName = config.fileName(),
            .fileDirectory = config.fileDirectory(),
            .fileType = static_cast<int>(config.fileType()),
            .mono = config.formatMono(),
            .formatOption = config.formatOption(),
            .formatQuality = config.formatQuality(),
            .sampleRate = config.formatSampleRate(),
            .mixingOption = static_cast<int>(config.mixingOption()),
            .muteSoloEnabled = config.isMuteSoloEnabled(),
            .sourceOption = static_cast<int>(config.sourceOption()),
            .sources = config.source(),
            .timeRange = static_cast<int>(config.timeRange()),
        };
    }

    Audio::AudioExporterConfig fromAutomationDto(const AudioExportConfigDto &config) {
        Audio::AudioExporterConfig result;
        result.setFileName(config.fileName);
        result.setFileDirectory(config.fileDirectory);
        result.setFileType(static_cast<Audio::AudioExporterConfig::FileType>(config.fileType));
        result.setFormatMono(config.mono);
        result.setFormatOption(config.formatOption);
        result.setFormatQuality(config.formatQuality);
        result.setFormatSampleRate(config.sampleRate);
        result.setMixingOption(
            static_cast<Audio::AudioExporterConfig::MixingOption>(config.mixingOption));
        result.setMuteSoloEnabled(config.muteSoloEnabled);
        result.setSourceOption(
            static_cast<Audio::AudioExporterConfig::SourceOption>(config.sourceOption));
        result.setSource(config.sources);
        result.setTimeRange(static_cast<Audio::AudioExporterConfig::TimeRange>(config.timeRange));
        return result;
    }

    class AudioExportJobAdapter final : public IAudioExportJob {
    public:
        AudioExportJobAdapter(AppModel *model, QString projectPath, AudioExportConfigDto config)
            : m_audioContext(AudioContext::instance()),
              m_exporter(std::make_unique<Audio::AudioExporter>(nullptr)) {
            m_config = std::move(config);
            if (m_config.sourceOption == Audio::AudioExporterConfig::SO_All) {
                m_config.sources.clear();
                m_config.sources.reserve(model->tracks().size());
                for (int index = 0; index < model->tracks().size(); ++index)
                    m_config.sources.append(index);
            }
            m_exporter->configureAutomationBackend(model, std::move(projectPath),
                                                   m_audioContext.data());
            m_exporter->setConfigInternal(fromAutomationDto(m_config));
        }

        AudioExportPreviewDto preview() const override {
            return {
                .baseDirectory = QDir(m_exporter->projectDirectoryInternal())
                                     .absoluteFilePath(m_config.fileDirectory),
                .filePaths = m_exporter->dryRunInternal(),
                .warningFlags = static_cast<quint32>(m_exporter->warningInternal()),
            };
        }

        AudioExportBackendResult execute(const AudioExportObserver &observer) override {
            if (!m_audioContext) {
                return {.state = AudioExportBackendState::Failed,
                        .errorMessage = QStringLiteral("Audio context is unavailable")};
            }
            auto inferenceStatus = m_audioContext->exportInferenceStatus();
            while (inferenceStatus == AudioContext::ExportInferenceStatus::Pending &&
                   !m_cancellationRequested.load(std::memory_order_acquire)) {
                QEventLoop loop;
                QTimer::singleShot(25, &loop, &QEventLoop::quit);
                loop.exec();
                if (m_audioContext)
                    inferenceStatus = m_audioContext->exportInferenceStatus();
            }
            if (m_cancellationRequested.load(std::memory_order_acquire))
                return {.state = AudioExportBackendState::Canceled};
            if (!m_audioContext) {
                return {.state = AudioExportBackendState::Failed,
                        .errorMessage = QStringLiteral("Audio context is unavailable")};
            }
            if (inferenceStatus == AudioContext::ExportInferenceStatus::Failed) {
                return {.state = AudioExportBackendState::Failed,
                        .errorMessage = QStringLiteral("Inference failed")};
            }
            const auto progressConnection = QObject::connect(
                m_exporter.get(), &Audio::AudioExporter::progressChanged,
                [callback = observer.progress](const double progress, const int sourceIndex) {
                    if (callback)
                        callback(progress, sourceIndex);
                });
            const auto clippingConnection =
                QObject::connect(m_exporter.get(), &Audio::AudioExporter::clippingDetected,
                                 [callback = observer.clipping](const int sourceIndex) {
                                     if (callback)
                                         callback(sourceIndex);
                                 });
            const auto warningConnection = QObject::connect(
                m_exporter.get(), &Audio::AudioExporter::warningAdded,
                [callback = observer.warning](const QString &message, const int sourceIndex) {
                    if (callback)
                        callback(message, sourceIndex);
                });
            const auto result = m_exporter->execInternal();
            QObject::disconnect(progressConnection);
            QObject::disconnect(clippingConnection);
            QObject::disconnect(warningConnection);
            switch (result) {
                case Audio::AudioExporter::R_Ok:
                    return {.state = AudioExportBackendState::Succeeded};
                case Audio::AudioExporter::R_Abort:
                    return {.state = AudioExportBackendState::Canceled};
                case Audio::AudioExporter::R_Fail:
                    return {.state = AudioExportBackendState::Failed,
                            .errorMessage = m_exporter->errorString()};
            }
            return {.state = AudioExportBackendState::Failed,
                    .errorMessage = QStringLiteral("Unknown audio export result")};
        }

        void cancel() override {
            m_cancellationRequested.store(true, std::memory_order_release);
            m_exporter->cancelInternal(false, {});
        }

        void cleanup() override {
            m_exporter->cleanUpInternal();
        }

    private:
        QPointer<AudioContext> m_audioContext;
        std::unique_ptr<Audio::AudioExporter> m_exporter;
        AudioExportConfigDto m_config;
        std::atomic_bool m_cancellationRequested = false;
    };

    AudioExportRuntimeServices createAudioExportAutomationServices() {
        AudioExportRuntimeServices services;
        services.createJob = [](AppModel *model, const QString &projectPath,
                                const AudioExportConfigDto &config)
            -> AutomationResult<std::shared_ptr<IAudioExportJob>> {
            if (!model) {
                AutomationError error;
                error.code = AutomationErrorCode::ModuleNotReady;
                error.message = QStringLiteral("AppModel is unavailable");
                return error;
            }
            for (const auto source : config.sources) {
                if (source >= model->tracks().size()) {
                    return AutomationError::invalidArgument(
                        QStringLiteral("config.sources"),
                        QStringLiteral("Audio export source index is out of range"));
                }
            }
            return std::shared_ptr<IAudioExportJob>(
                std::make_shared<AudioExportJobAdapter>(model, projectPath, config));
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
