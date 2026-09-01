#ifndef AUDIO_AUDIOEXPORTER_P_H
#define AUDIO_AUDIOEXPORTER_P_H

#include "AudioExporter.h"
#include "Automation/AutomationTypes.h"

#include <QHash>
#include <QSharedData>

namespace talcs {
    class DspxProjectAudioExporter;
    class DspxProjectContext;
}

namespace Audio {

    class AudioExporterConfigData : public QSharedData {
    public:
        QString fileName;
        QString fileDirectory;
        AudioExporterConfig::FileType fileType = AudioExporterConfig::FT_Wav;
        bool formatMono = false;
        int formatOption = 0;
        int formatQuality = 100;
        double formatSampleRate = 44100.0;
        AudioExporterConfig::MixingOption mixingOption = AudioExporterConfig::MO_Mixed;
        bool isMuteSoloEnabled = true;
        AudioExporterConfig::SourceOption sourceOption = AudioExporterConfig::SO_All;
        QList<int> source;
        AudioExporterConfig::TimeRange timeRange = AudioExporterConfig::TR_All;
    };

    class AudioExporterPrivate {
        Q_DECLARE_PUBLIC(AudioExporter)
    public:
        AudioExporter *q_ptr;
        AudioExporterConfig config;
        Core::IProjectWindow *windowHandle;
        AppModel *model = nullptr;
        QString projectPath;
        talcs::DspxProjectContext *context = nullptr;
        bool automationBackend = false;
        Automation::TaskId lastTaskId;

        QString projectName() const;
        QString projectDirectory() const;
        QString trackName(int trackIndex) const;
        talcs::DspxProjectContext *projectContext() const;
        QPair<int, int> calculateRange() const;
        QList<int> selectedSources() const;

        bool calculateTemplate(QString &templateString) const;
        bool calculateTemplate(QString &templateString, const QString &trackName,
                               int trackIndex) const;

        int calculateFormat() const;

        AudioExporter::Warning warning;
        QStringList fileList;
        void updateFileListAndWarnings();

        QStringList temporaryFileList;
        QHash<QString, QString> pendingTemporaryFiles;

        talcs::DspxProjectAudioExporter *currentExporter = nullptr;
    };
}

#endif // AUDIO_AUDIOEXPORTER_P_H
