#ifndef AUDIO_AUDIOEXPORTER_P_H
#define AUDIO_AUDIOEXPORTER_P_H

#include "AudioExporter.h"

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
        AudioExporterConfig::FileType fileType;
        bool formatMono;
        int formatOption;
        int formatQuality;
        double formatSampleRate;
        AudioExporterConfig::MixingOption mixingOption;
        bool isMuteSoloEnabled;
        AudioExporterConfig::SourceOption sourceOption;
        QList<int> source;
        AudioExporterConfig::TimeRange timeRange;
    };

    class AudioExporterPrivate {
        Q_DECLARE_PUBLIC(AudioExporter)
    public:
        AudioExporter *q_ptr;
        AudioExporterConfig config;
        Core::IProjectWindow *windowHandle;

        static QString projectName();
        static QString projectDirectory();
        static QString trackName(int trackIndex);
        static talcs::DspxProjectContext *projectContext();
        static QPair<int, int> calculateRange();
        static QList<int> selectedSources();

        bool calculateTemplate(QString &templateString) const;
        bool calculateTemplate(QString &templateString, const QString &trackName, int trackIndex) const;

        int calculateFormat() const;

        AudioExporter::Warning warning;
        QStringList fileList;
        void updateFileListAndWarnings();

        QStringList temporaryFileList;

        talcs::DspxProjectAudioExporter *currentExporter = nullptr;
    };
}

#endif // AUDIO_AUDIOEXPORTER_P_H
