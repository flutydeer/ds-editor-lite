#include "AudioExporter.h"

#include "AppContext.h"
#include "Automation/AudioExportAutomationAdapter.h"
#include "Automation/CoreRuntime.h"
#include "AudioContext.h"
#include "AudioFilePublisher.h"
#include "AudioExporter_p.h"
#include "Controller/AppController.h"
#include "Controller/DocumentWorkflow/DocumentWorkflowController.h"
#include <lite/ProjectModel/AppModel/Track.h>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QVariant>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QHash>

#include <TalcsCore/TransportAudioSource.h>
#include <TalcsCore/MixerAudioSource.h>
#include <TalcsFormat/AudioFormatIO.h>
#include <TalcsDspx/DspxProjectAudioExporter.h>
#include <TalcsDspx/DspxProjectContext.h>
#include <TalcsDspx/DspxTrackContext.h>

#include <Modules/Audio/AudioSettings.h>

namespace Audio {
    using namespace Internal;

    AudioExporterConfig::AudioExporterConfig() : d(new AudioExporterConfigData) {
    }

    AudioExporterConfig::~AudioExporterConfig() = default;

    QString AudioExporterConfig::fileName() const {
        return d->fileName;
    }

    void AudioExporterConfig::setFileName(const QString &a_) {
        d->fileName = a_;
    }

    QString AudioExporterConfig::fileDirectory() const {
        return d->fileDirectory;
    }

    void AudioExporterConfig::setFileDirectory(const QString &a_) {
        d->fileDirectory = a_;
    }

    AudioExporterConfig::FileType AudioExporterConfig::fileType() const {
        return d->fileType;
    }

    void AudioExporterConfig::setFileType(const AudioExporterConfig::FileType a_) {
        d->fileType = a_;
    }

    bool AudioExporterConfig::formatMono() const {
        return d->formatMono;
    }

    void AudioExporterConfig::setFormatMono(const bool a_) {
        d->formatMono = a_;
    }

    QStringList AudioExporterConfig::formatOptionsOfType(const FileType type) {
        switch (type) {
            case FT_Wav:
                return {
                    QApplication::translate("Audio::AudioExporterConfig",
                                            "32-bit float (IEEE 754)"),
                    QApplication::translate("Audio::AudioExporterConfig", "24-bit PCM"),
                    QApplication::translate("Audio::AudioExporterConfig", "16-bit PCM"),
                    QApplication::translate("Audio::AudioExporterConfig", "Unsigned 8-bit PCM"),
                };
            case FT_Flac:
                return {
                    QApplication::translate("Audio::AudioExporterConfig", "24-bit PCM"),
                    QApplication::translate("Audio::AudioExporterConfig", "16-bit PCM"),
                    QApplication::translate("Audio::AudioExporterConfig", "8-bit PCM"),
                };
            default:
                return {};
        }
    }

    QString AudioExporterConfig::extensionOfType(const FileType type) {
        switch (type) {
            case FT_Wav:
                return QStringLiteral("wav");
            case FT_Flac:
                return QStringLiteral("flac");
            case FT_OggVorbis:
                return QStringLiteral("ogg");
            case FT_Mp3:
                return QStringLiteral("mp3");
        }
        return {};
    }

    int AudioExporterConfig::formatOption() const {
        return d->formatOption;
    }

    void AudioExporterConfig::setFormatOption(const int a_) {
        d->formatOption = a_;
    }

    int AudioExporterConfig::formatQuality() const {
        return d->formatQuality;
    }

    void AudioExporterConfig::setFormatQuality(const int a_) {
        d->formatQuality = a_;
    }

    double AudioExporterConfig::formatSampleRate() const {
        return d->formatSampleRate;
    }

    void AudioExporterConfig::setFormatSampleRate(const double a_) {
        d->formatSampleRate = a_;
    }

    AudioExporterConfig::MixingOption AudioExporterConfig::mixingOption() const {
        return d->mixingOption;
    }

    void AudioExporterConfig::setMixingOption(const AudioExporterConfig::MixingOption a_) {
        d->mixingOption = a_;
    }

    bool AudioExporterConfig::isMuteSoloEnabled() const {
        return d->isMuteSoloEnabled;
    }

    void AudioExporterConfig::setMuteSoloEnabled(const bool a_) {
        d->isMuteSoloEnabled = a_;
    }

    AudioExporterConfig::SourceOption AudioExporterConfig::sourceOption() const {
        return d->sourceOption;
    }

    void AudioExporterConfig::setSourceOption(const AudioExporterConfig::SourceOption a_) {
        d->sourceOption = a_;
    }

    QList<int> AudioExporterConfig::source() const {
        return d->source;
    }

    void AudioExporterConfig::setSource(const QList<int> &a_) {
        d->source = a_;
    }

    AudioExporterConfig::TimeRange AudioExporterConfig::timeRange() const {
        return d->timeRange;
    }

    void AudioExporterConfig::setTimeRange(const AudioExporterConfig::TimeRange a_) {
        d->timeRange = a_;
    }

    QVariantMap AudioExporterConfig::toVariantMap() const {
        return QVariantMap({
            {"fileName",          d->fileName                   },
            {"fileDirectory",     d->fileDirectory              },
            {"fileType",          d->fileType                   },
            {"formatMono",        d->formatMono                 },
            {"formatOption",      d->formatOption               },
            {"formatQuality",     d->formatQuality              },
            {"formatSampleRate",  d->formatSampleRate           },
            {"mixingOption",      d->mixingOption               },
            {"isMuteSoloEnabled", d->isMuteSoloEnabled          },
            {"sourceOption",      d->sourceOption               },
            {"source",            QVariant::fromValue(d->source)},
            {"timeRange",         d->timeRange                  },
        });
    }

    AudioExporterConfig AudioExporterConfig::fromVariantMap(const QVariantMap &map) {
        AudioExporterConfig config;
        config.d->fileName = map.value("fileName").toString();
        config.d->fileDirectory = map.value("fileDirectory").toString();
        config.d->fileType = static_cast<FileType>(map.value("fileType").toInt());
        config.d->formatMono = map.value("formatMono").toBool();
        config.d->formatOption = map.value("formatOption").toInt();
        config.d->formatQuality = map.value("formatQuality").toInt();
        config.d->formatSampleRate = map.value("formatSampleRate").toDouble();
        config.d->mixingOption = static_cast<MixingOption>(map.value("mixingOption").toInt());
        config.d->isMuteSoloEnabled = map.value("isMuteSoloEnabled").toBool();
        config.d->sourceOption = static_cast<SourceOption>(map.value("sourceOption").toInt());
        config.d->source = map.value("source").value<QList<int>>();
        config.d->timeRange = static_cast<TimeRange>(map.value("timeRange").toInt());
        return config;
    }

    bool AudioExporterConfig::operator==(const AudioExporterConfig &other) const {
        return d->fileName == other.d->fileName && d->fileDirectory == other.d->fileDirectory &&
               d->fileType == other.d->fileType && d->formatMono == other.d->formatMono &&
               d->formatOption == other.d->formatOption &&
               d->formatQuality == other.d->formatQuality &&
               d->formatSampleRate == other.d->formatSampleRate &&
               d->mixingOption == other.d->mixingOption &&
               d->isMuteSoloEnabled == other.d->isMuteSoloEnabled &&
               d->sourceOption == other.d->sourceOption && d->source == other.d->source &&
               d->timeRange == other.d->timeRange;
    }

    QString AudioExporterPrivate::projectName() const {
        const auto path =
            automationBackend ? projectPath : documentWorkflowController->projectPath();
        const auto name = QFileInfo(path).baseName();
        return name.isEmpty()
                   ? QCoreApplication::translate("Audio::AudioExporterPrivate", "Untitled")
                   : name;
    }

    auto AudioExporterPrivate::projectDirectory() const -> QString {
        const auto path =
            automationBackend ? projectPath : documentWorkflowController->projectPath();
        if (const auto dir = QFileInfo(path).dir(); dir.isRelative()) {
            const auto documents =
                QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
            return documents.isEmpty() ? QDir::homePath() : documents.first();
        } else {
            return dir.path();
        }
    }

    QString AudioExporterPrivate::trackName(const int trackIndex) const {
        const auto currentModel = model ? model : appModel;
        return currentModel->tracks().at(trackIndex)->name();
    }

    talcs::DspxProjectContext *AudioExporterPrivate::projectContext() const {
        return context ? context : AudioContext::instance();
    }

    QPair<int, int> AudioExporterPrivate::calculateRange() const {
        const auto currentModel = model ? model : appModel;
        return {0, currentModel->projectLengthInTicks()}; // TODO
    }

    QList<int> AudioExporterPrivate::selectedSources() const {
        return {}; // TODO
    }

    bool AudioExporterPrivate::calculateTemplate(QString &templateString) const {
        return calculateTemplate(templateString, {}, -1);
    }

    bool AudioExporterPrivate::calculateTemplate(QString &templateString, const QString &trackName,
                                                 const int trackIndex) const {
        static const QRegularExpression re(R"re(\$\{(.*?)\})re");
        bool allTemplatesMatch = true;
        const QStringView templateStringView(templateString);
        qsizetype pos = 0;
        QString result;
        auto matchIt =
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            re.globalMatchView(templateStringView);
#else
            re.globalMatch(templateStringView);
#endif
        for (; matchIt.hasNext(); matchIt.next()) {
            const auto match = matchIt.peekNext();
            const auto templateName = match.capturedView(1);
            auto replacedText = match.captured(0);
            if (templateName == "projectName") {
                replacedText = projectName();
            } else if (templateName == "sampleRate") {
                // File-name template substitutions are stable machine-readable text.
                replacedText = QString::number(config.formatSampleRate()).replace('.', '_');
            } else if (templateName == "today") {
                replacedText = QDate::currentDate().toString("yyyyMMdd");
            } else if (templateName == "$") {
                replacedText = QStringLiteral("$");
            } else if (trackIndex != -1) {
                if (templateName == "trackName") {
                    replacedText = trackName;
                } else if (templateName == "trackIndex") {
                    // File-name template substitutions are stable machine-readable text.
                    replacedText = QString::number(trackIndex + 1);
                } else {
                    allTemplatesMatch = false;
                }
            } else {
                allTemplatesMatch = false;
            }
            result += templateStringView.sliced(pos, match.capturedStart(0) - pos);
            result += replacedText;
            pos = match.capturedEnd(0);
        }
        result += templateStringView.last(templateStringView.length() - pos);
        templateString = result;
        return allTemplatesMatch;
    }

    int AudioExporterPrivate::calculateFormat() const {
        int format = 0;
        switch (config.fileType()) {
            case AudioExporterConfig::FT_Wav:
                format |= talcs::AudioFormatIO::WAV;
                switch (config.formatOption()) {
                    case 0:
                        format |= talcs::AudioFormatIO::FLOAT;
                        return format;
                    case 1:
                        format |= talcs::AudioFormatIO::PCM_24;
                        return format;
                    case 2:
                        format |= talcs::AudioFormatIO::PCM_16;
                        return format;
                    case 3:
                        format |= talcs::AudioFormatIO::PCM_U8;
                        return format;
                }
                break;
            case AudioExporterConfig::FT_Flac:
                format |= talcs::AudioFormatIO::FLAC;
                switch (config.formatOption()) {
                    case 0:
                        format |= talcs::AudioFormatIO::PCM_24;
                        return format;
                    case 1:
                        format |= talcs::AudioFormatIO::PCM_16;
                        return format;
                    case 2:
                        format |= talcs::AudioFormatIO::PCM_S8;
                        return format;
                }
                break;
            case AudioExporterConfig::FT_OggVorbis:
                format |= static_cast<int>(talcs::AudioFormatIO::OGG) |
                          static_cast<int>(talcs::AudioFormatIO::VORBIS);
                return format;
            case AudioExporterConfig::FT_Mp3:
                format |= static_cast<int>(talcs::AudioFormatIO::MPEG) |
                          static_cast<int>(talcs::AudioFormatIO::MPEG_LAYER_III);
                return format;
        }
        return 0;
    }

    void AudioExporterPrivate::updateFileListAndWarnings() {
        warning = {};
        if (config.fileType() == AudioExporterConfig::FT_Mp3 ||
            config.fileType() == AudioExporterConfig::FT_OggVorbis)
            warning |= AudioExporter::W_LossyFormat;
        fileList.clear();
        if (config.mixingOption() == AudioExporterConfig::MO_Mixed) {
            auto calculatedFileName = config.fileName();
            if (!calculateTemplate(calculatedFileName))
                warning |= AudioExporter::W_UnrecognizedTemplate;
            const auto fileInfo =
                QFileInfo(QDir(QDir(projectDirectory()).absoluteFilePath(config.fileDirectory()))
                              .absoluteFilePath(calculatedFileName));
            if (fileInfo.exists())
                warning |= AudioExporter::W_WillOverwrite;
            fileList.append(fileInfo.absoluteFilePath());
        } else {
            if (config.source().isEmpty())
                warning |= AudioExporter::W_NoFile;
            QSet<QString> fileSet;
            for (const auto index : config.source()) {
                auto calculatedFileName = config.fileName();
                if (!calculateTemplate(calculatedFileName, trackName(index), index))
                    warning |= AudioExporter::W_UnrecognizedTemplate;
                auto fileInfo = QFileInfo(
                    QDir(QDir(projectDirectory()).absoluteFilePath(config.fileDirectory()))
                        .absoluteFilePath(calculatedFileName));
                if (fileInfo.exists())
                    warning |= AudioExporter::W_WillOverwrite;
                if (fileSet.contains(fileInfo.absoluteFilePath()))
                    warning |= AudioExporter::W_DuplicatedFile;
                fileSet.insert(fileInfo.absoluteFilePath());
                fileList.append(fileInfo.absoluteFilePath());
            }
        }
    }

    AudioExporter::AudioExporter(Core::IProjectWindow *window, QObject *parent)
        : QObject(parent), d_ptr(new AudioExporterPrivate) {
        Q_D(AudioExporter);
        d->q_ptr = this;
        d->windowHandle = window;
    }

    AudioExporter::~AudioExporter() = default;

    Core::IProjectWindow *AudioExporter::windowHandle() const {
        Q_D(const AudioExporter);
        return d->windowHandle;
    }

    QStringList AudioExporter::presets() {
        auto *runtime = AppContext::instance<Automation::CoreRuntime>();
        if (!runtime)
            return {};
        const auto snapshot = runtime->settings().getSettings();
        if (!snapshot)
            return {};
        QStringList result;
        for (const auto &preset : snapshot.get().audio.audioExporterPresets)
            result.append(preset.name);
        return result;
    }

    QList<QPair<QString, AudioExporterConfig>> AudioExporter::predefinedPresets() {
        static auto wavMix = AudioExporterConfig::fromVariantMap({
            {"fileName",          "${projectName}.wav"         },
            {"fileDirectory",     {}                           },
            {"fileType",          AudioExporterConfig::FT_Wav  },
            {"formatMono",        false                        },
            {"formatOption",      0                            },
            {"formatQuality",     100                          },
            {"formatSampleRate",  44100                        },
            {"mixingOption",      AudioExporterConfig::MO_Mixed},
            {"isMuteSoloEnabled", true                         },
            {"sourceOption",      AudioExporterConfig::SO_All  },
            {"source",            {}                           },
            {"timeRange",         AudioExporterConfig::TR_All  },
        });
        static auto wavSep = AudioExporterConfig::fromVariantMap({
            {"fileName",          "${projectName}_${trackIndex}_${trackName}.wav"},
            {"fileDirectory",     {}                                             },
            {"fileType",          AudioExporterConfig::FT_Wav                    },
            {"formatMono",        false                                          },
            {"formatOption",      0                                              },
            {"formatQuality",     100                                            },
            {"formatSampleRate",  44100                                          },
            {"mixingOption",      AudioExporterConfig::MO_SeparatedThruMaster    },
            {"isMuteSoloEnabled", true                                           },
            {"sourceOption",      AudioExporterConfig::SO_All                    },
            {"source",            {}                                             },
            {"timeRange",         AudioExporterConfig::TR_All                    },
        });
        static auto flacMix = AudioExporterConfig::fromVariantMap({
            {"fileName",          "${projectName}.flac"        },
            {"fileDirectory",     {}                           },
            {"fileType",          AudioExporterConfig::FT_Flac },
            {"formatMono",        false                        },
            {"formatOption",      0                            },
            {"formatQuality",     100                          },
            {"formatSampleRate",  44100                        },
            {"mixingOption",      AudioExporterConfig::MO_Mixed},
            {"isMuteSoloEnabled", true                         },
            {"sourceOption",      AudioExporterConfig::SO_All  },
            {"source",            {}                           },
            {"timeRange",         AudioExporterConfig::TR_All  },
        });
        static auto flacSep = AudioExporterConfig::fromVariantMap({
            {"fileName",          "${projectName}_${trackIndex}_${trackName}.flac"},
            {"fileDirectory",     {}                                              },
            {"fileType",          AudioExporterConfig::FT_Flac                    },
            {"formatMono",        false                                           },
            {"formatOption",      0                                               },
            {"formatQuality",     100                                             },
            {"formatSampleRate",  44100                                           },
            {"mixingOption",      AudioExporterConfig::MO_SeparatedThruMaster     },
            {"isMuteSoloEnabled", true                                            },
            {"sourceOption",      AudioExporterConfig::SO_All                     },
            {"source",            {}                                              },
            {"timeRange",         AudioExporterConfig::TR_All                     },
        });
        static auto oggMix = AudioExporterConfig::fromVariantMap({
            {"fileName",          "${projectName}.ogg"             },
            {"fileDirectory",     {}                               },
            {"fileType",          AudioExporterConfig::FT_OggVorbis},
            {"formatMono",        false                            },
            {"formatOption",      0                                },
            {"formatQuality",     100                              },
            {"formatSampleRate",  44100                            },
            {"mixingOption",      AudioExporterConfig::MO_Mixed    },
            {"isMuteSoloEnabled", true                             },
            {"sourceOption",      AudioExporterConfig::SO_All      },
            {"source",            {}                               },
            {"timeRange",         AudioExporterConfig::TR_All      },
        });
        static auto oggSep = AudioExporterConfig::fromVariantMap({
            {"fileName",          "${projectName}_${trackIndex}_${trackName}.ogg"},
            {"fileDirectory",     {}                                             },
            {"fileType",          AudioExporterConfig::FT_OggVorbis              },
            {"formatMono",        false                                          },
            {"formatOption",      0                                              },
            {"formatQuality",     100                                            },
            {"formatSampleRate",  44100                                          },
            {"mixingOption",      AudioExporterConfig::MO_SeparatedThruMaster    },
            {"isMuteSoloEnabled", true                                           },
            {"sourceOption",      AudioExporterConfig::SO_All                    },
            {"source",            {}                                             },
            {"timeRange",         AudioExporterConfig::TR_All                    },
        });
        return {
            {tr("WAV - Mixed"),            wavMix },
            {tr("WAV - Separated"),        wavSep },
            {tr("FLAC - Mixed"),           flacMix},
            {tr("FLAC - Separated"),       flacSep},
            {tr("Ogg/Vorbis - Mixed"),     oggMix },
            {tr("Ogg/Vorbis - Separated"), oggSep },
        };
    }

    AudioExporterConfig AudioExporter::preset(const QString &name) {
        auto *runtime = AppContext::instance<Automation::CoreRuntime>();
        if (!runtime)
            return {};
        const auto snapshot = runtime->settings().getSettings();
        if (!snapshot)
            return {};
        for (const auto &preset : snapshot.get().audio.audioExporterPresets) {
            if (preset.name == name)
                return Automation::fromAutomationDto(preset.config);
        }
        return {};
    }

    void AudioExporter::addPreset(const QString &name, const AudioExporterConfig &config) {
        auto *runtime = AppContext::instance<Automation::CoreRuntime>();
        if (!runtime)
            return;
        const auto snapshot = runtime->settings().getSettings();
        if (!snapshot)
            return;
        auto settings = snapshot.get().audio;
        bool replaced = false;
        for (auto &preset : settings.audioExporterPresets) {
            if (preset.name == name) {
                preset.config = Automation::toAutomationDto(config);
                replaced = true;
                break;
            }
        }
        if (!replaced) {
            settings.audioExporterPresets.append({
                .name = name,
                .config = Automation::toAutomationDto(config),
            });
        }
        runtime->settings().updateAudio({}, settings);
    }

    bool AudioExporter::removePreset(const QString &name) {
        auto *runtime = AppContext::instance<Automation::CoreRuntime>();
        if (!runtime)
            return false;
        const auto snapshot = runtime->settings().getSettings();
        if (!snapshot)
            return false;
        auto settings = snapshot.get().audio;
        const auto previousSize = settings.audioExporterPresets.size();
        settings.audioExporterPresets.removeIf(
            [&name](const Automation::AudioExportPresetDto &preset) {
                return preset.name == name;
            });
        if (settings.audioExporterPresets.size() == previousSize)
            return false;
        const auto result = runtime->settings().updateAudio({}, settings);
        return static_cast<bool>(result);
    }

    static QList<AudioExporterListener *> m_listeners;

    void AudioExporter::registerListener(AudioExporterListener *listener) {
        m_listeners.append(listener);
    }

    void AudioExporter::setConfig(const AudioExporterConfig &config) {
        Q_D(AudioExporter);
        d->config = config;
        d->warning = {};
        d->fileList.clear();
        auto *runtime = AppContext::instance<Automation::CoreRuntime>();
        if (!runtime)
            return;
        const auto preview = runtime->audioExports().preview(runtime->documentVersion().documentId,
                                                             Automation::toAutomationDto(config));
        if (!preview)
            return;
        d->warning = static_cast<Warning>(preview.get().warningFlags);
        d->fileList = preview.get().filePaths;
    }

    void AudioExporter::configureAutomationBackend(AppModel *model, QString projectPath,
                                                   talcs::DspxProjectContext *projectContext) {
        Q_D(AudioExporter);
        d->model = model;
        d->projectPath = std::move(projectPath);
        d->context = projectContext;
        d->automationBackend = true;
    }

    void AudioExporter::setConfigInternal(const AudioExporterConfig &config) {
        Q_D(AudioExporter);
        d->config = config;
        d->updateFileListAndWarnings();
    }

    AudioExporterConfig AudioExporter::config() const {
        Q_D(const AudioExporter);
        return d->config;
    }

    AudioExporter::Warning AudioExporter::warning() const {
        Q_D(const AudioExporter);
        return d->warning;
    }

    AudioExporter::Warning AudioExporter::warningInternal() const {
        return warning();
    }

    QStringList AudioExporter::warningText(const AudioExporter::Warning warning) {
        QStringList list;
        if (warning & W_NoFile) {
            list.append(tr("No file will be exported. Please check if any source is selected."));
        }
        if (warning & W_DuplicatedFile) {
            list.append(tr("The files to be exported contain files with duplicate names. Please "
                           "check if the file name template is unique for each source."));
        }
        if (warning & W_WillOverwrite) {
            list.append(tr("The files to be exported contain files with the same name as existing "
                           "files. If continue, the existing files will be overwritten."));
        }
        if (warning & W_UnrecognizedTemplate) {
            list.append(tr("Unrecognized file name template. Please check the syntax of the file "
                           "name template."));
        }
        if (warning & W_LossyFormat) {
            list.append(tr("The currently selected file type is a lossy format. To avoid loss of "
                           "sound quality, please use WAV or FLAC format."));
        }
        return list;
    }

    QStringList AudioExporter::dryRun() const {
        Q_D(const AudioExporter);
        return d->fileList;
    }

    QStringList AudioExporter::dryRunInternal() const {
        return dryRun();
    }

    QString AudioExporter::projectDirectoryInternal() const {
        Q_D(const AudioExporter);
        return d->projectDirectory();
    }

    AudioExporter::Result AudioExporter::exec() {
        Q_D(AudioExporter);
        auto *runtime = AppContext::instance<Automation::CoreRuntime>();
        if (!runtime) {
            setErrorString(tr("Audio export runtime is unavailable"));
            return R_Fail;
        }

        Automation::AudioExportObserver observer;
        observer.progress = [this](const double progress, const int sourceIndex) {
            emit progressChanged(progress, sourceIndex);
        };
        observer.inferenceProgress = [this](const double progress) {
            emit inferenceProgressChanged(progress);
        };
        observer.clipping = [this](const int sourceIndex) { emit clippingDetected(sourceIndex); };
        observer.warning = [this](const QString &message, const int sourceIndex) {
            emit warningAdded(message, sourceIndex);
        };
        const Automation::AudioExportPolicyDto policy{
            .allowNoFiles = true,
            .allowDuplicatePaths = true,
            .allowOverwrite = true,
            .allowUnrecognizedTemplate = true,
            .allowLossyFormat = true,
        };
        const auto accepted = runtime->audioExports().start(
            {.expected = runtime->documentVersion(),
             .source = Automation::InvocationSource::TrustedGui},
            Automation::toAutomationDto(config()), policy, std::move(observer), {}, {});
        if (!accepted) {
            setErrorString(accepted.getError().message);
            return R_Fail;
        }
        d->lastTaskId = accepted.get().taskId;

        while (true) {
            const auto task =
                runtime->tasks().getTask(accepted.get().document.documentId, accepted.get().taskId);
            if (!task) {
                setErrorString(task.getError().message);
                return R_Fail;
            }
            switch (task.get().state) {
                case Automation::AutomationTaskState::Succeeded:
                    return R_Ok;
                case Automation::AutomationTaskState::Failed:
                    setErrorString(task.get().error ? task.get().error->message
                                                    : tr("Audio export failed"));
                    return R_Fail;
                case Automation::AutomationTaskState::Canceled:
                    return R_Abort;
                case Automation::AutomationTaskState::Queued:
                case Automation::AutomationTaskState::Running:
                case Automation::AutomationTaskState::CancelRequested:
                case Automation::AutomationTaskState::Committing:
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 25);
                    break;
            }
        }
    }

    AudioExporter::Result AudioExporter::execInternal(const bool deferPublish) {
        Q_D(AudioExporter);

        const auto config = this->config();
        const auto projectContext = d->projectContext();
        clearErrorString();
        QHash<QString, QString> temporaryFiles;

        d->temporaryFileList.clear();
        d->pendingTemporaryFiles.clear();

        {
            // prepare AudioFormatIO for exporting
            QObject o;
            std::unique_ptr<talcs::AudioFormatIO[]> ioList(
                new talcs::AudioFormatIO[d->fileList.size()]);
            const auto uuid = QByteArray::fromHex(QUuid::createUuid().toByteArray(QUuid::Id128))
                                  .toBase64(QByteArray::Base64UrlEncoding)
                                  .mid(0, 8);
            for (int i = 0; i < d->fileList.size(); i++) {
                QString filename = d->fileList.at(i);
                auto temporaryFileName = QFileInfo(filename).dir().filePath(
                    ".ds$" + uuid + QFileInfo(filename).fileName() + ".exporting");
                temporaryFiles.insert(filename, temporaryFileName);
                d->temporaryFileList.append(temporaryFileName);
                filename = temporaryFileName;
                const auto file = new QFile(filename, &o);
                if (!file->open(QIODevice::WriteOnly)) {
                    setErrorString(tr("Cannot open file for writing: %1").arg(filename));
                    return R_Fail;
                }
                auto &io = ioList[i];
                io.setStream(file);
                io.setSampleRate(config.formatSampleRate());
                io.setChannelCount(config.formatMono() ? 1 : 2);
                io.setFormat(d->calculateFormat());
                if (!io.open(talcs::AbstractAudioFormatIO::Write)) {
                    setErrorString(tr("Format not supported: %1").arg(io.errorString()));
                    return R_Fail;
                }
                io.setCompressionLevel(0.01 * (100 - config.formatQuality()));
            }

            // create and configure talcs::DspxProjectAudioExporter
            talcs::DspxProjectAudioExporter exporter(projectContext);
            auto cleanup = [=](void *) { d->currentExporter = nullptr; };
            std::unique_ptr<void, decltype(cleanup)> _1(this, cleanup);
            d->currentExporter = &exporter;
            exporter.setMonoChannel(config.formatMono());
            exporter.setThruMaster(config.mixingOption() ==
                                   AudioExporterConfig::MO_SeparatedThruMaster);
            exporter.setClippingCheckEnabled(AudioSettings::audioExporterClippingCheckEnabled());
            exporter.setMuteSoloEnabled(config.isMuteSoloEnabled());
            const auto [fst, snd] = d->calculateRange();
            exporter.setRange(fst, snd);
            QList<talcs::DspxTrackContext *> tracks;
            switch (config.sourceOption()) {
                case AudioExporterConfig::SO_All:
                    tracks = projectContext->tracks();
                    break;
                case AudioExporterConfig::SO_Selected:
                case AudioExporterConfig::SO_Custom:
                    for (const auto index : config.source()) {
                        tracks.append(projectContext->tracks().at(index));
                    }
                    break;
            }
            if (config.mixingOption() == AudioExporterConfig::MO_Mixed) {
                exporter.setMixedTask(tracks, &ioList[0]);
            } else {
                for (int i = 0; i < tracks.size(); i++) {
                    exporter.addSeparatedTask(tracks[i], &ioList[i]);
                }
            }
            QHash<talcs::DspxTrackContext *, int> sourceIndexMap;
            sourceIndexMap.insert(nullptr, -1);
            for (int i = 0; i < tracks.size(); i++) {
                sourceIndexMap.insert(tracks[i], i);
            }

            // deal with audio components
            projectContext->transport()->pause();
            const auto currentBufferSize = projectContext->preMixer()->bufferSize();
            const auto currentSampleRate = projectContext->preMixer()->sampleRate();
            auto reopenMixer = [projectContext, currentBufferSize, currentSampleRate,
                                this](void *) {
                if (!projectContext->preMixer()->open(currentBufferSize, currentSampleRate))
                    addWarning(tr("Cannot reopen audio after exported"));
            };
            std::unique_ptr<void, decltype(reopenMixer)> _2(this, reopenMixer);
            if (!projectContext->preMixer()->open(
                    1024,
                    config.formatSampleRate())) { // TODO let user configure buffer size in settings
                setErrorString(tr("Cannot start audio exporting"));
                return R_Fail;
            }

            // call listeners
            QList<AudioExporterListener *> listenerToCallFinishList;
            auto callFinish = [&](void *) {
                for (const auto listener : listenerToCallFinishList) {
                    listener->willFinishCallback(this);
                }
            };
            std::unique_ptr<void, decltype(callFinish)> _3(this, callFinish);
            // Note: order of destruction: call AudioExporterListener::willFinish after mixer
            // reopened
            std::unique_ptr<void, decltype(reopenMixer)> _4 = std::move(_2);
            for (const auto listener : m_listeners) {
                if (!listener->willStartCallback(this))
                    return R_Fail;
                listenerToCallFinishList.prepend(listener);
            }

            // start exporting
            connect(
                &exporter, &talcs::DspxProjectAudioExporter::progressChanged, this,
                [sourceIndexMap, this](const double progressRatio, talcs::DspxTrackContext *track) {
                    emit progressChanged(progressRatio, sourceIndexMap.value(track));
                });
            connect(&exporter, &talcs::DspxProjectAudioExporter::clippingDetected, this,
                    [sourceIndexMap, this](talcs::DspxTrackContext *track) {
                        emit clippingDetected(sourceIndexMap.value(track));
                    });
            const auto ret = exporter.exec();
            if (ret == talcs::DspxProjectAudioExporter::Fail) {
                if (errorString().isEmpty())
                    setErrorString(tr("Internal Error"));
                return R_Fail;
            }
            if (ret == talcs::DspxProjectAudioExporter::Interrupted)
                return R_Abort;
        }

        d->pendingTemporaryFiles = std::move(temporaryFiles);
        if (deferPublish)
            return R_Ok;
        return publishInternal(true);
    }

    AudioExporter::Result AudioExporter::publishInternal(const bool allowOverwrite) {
        Q_D(AudioExporter);
        const auto pendingTemporaryFiles = d->pendingTemporaryFiles;
        d->pendingTemporaryFiles.clear();
        d->temporaryFileList.clear();

        const auto publication = publishAudioFiles(pendingTemporaryFiles, allowOverwrite);
        d->temporaryFileList = publication.remainingTemporaryFiles;
        if (!publication.succeeded()) {
            setErrorString(
                tr("Cannot publish temporary audio file: %1").arg(publication.failedTarget));
            return R_Fail;
        }
        for (const auto &path : publication.remainingTemporaryFiles)
            addWarning(tr("Cannot remove temporary audio export file: %1").arg(path));
        return R_Ok;
    }

    void AudioExporter::cleanUp() {
        Q_D(AudioExporter);
        auto *runtime = AppContext::instance<Automation::CoreRuntime>();
        if (!runtime || d->lastTaskId.isNull())
            return;
        const auto result =
            runtime->audioExports().cleanup({.expected = runtime->documentVersion(),
                                             .source = Automation::InvocationSource::TrustedGui},
                                            d->lastTaskId);
        if (result)
            d->lastTaskId = {};
    }

    void AudioExporter::cleanUpInternal() {
        Q_D(AudioExporter);
        for (const auto &filename : d->temporaryFileList) {
            QFile::remove(filename);
        }
        d->temporaryFileList.clear();
        d->pendingTemporaryFiles.clear();
    }

    void AudioExporter::cancel(const bool isFail, const QString &message) {
        Q_D(AudioExporter);
        auto *runtime = AppContext::instance<Automation::CoreRuntime>();
        if (!runtime || d->lastTaskId.isNull())
            return;
        if (isFail && !message.isEmpty())
            setErrorString(message);
        runtime->tasks().cancelTask({.expected = runtime->documentVersion(),
                                     .source = Automation::InvocationSource::TrustedGui},
                                    d->lastTaskId);
    }

    void AudioExporter::cancelInternal(const bool isFail, const QString &message) {
        Q_D(AudioExporter);
        if (!d->currentExporter)
            return;
        if (isFail)
            setErrorString(message.isEmpty() ? tr("Unknown error") : message);
        d->currentExporter->interrupt(isFail);
    }

    void AudioExporter::addWarning(const QString &message, const int sourceIndex) {
        emit warningAdded(message, sourceIndex);
    }
} // Audio
