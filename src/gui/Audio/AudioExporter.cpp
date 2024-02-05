//
// Created by Crs_1 on 2024/2/6.
//

#include "AudioExporter.h"

#include <QSettings>
#include <QDir>

#include <TalcsFormat/AudioFormatIO.h>

#include "Model/AppModel.h"

QList<QPair<QString, AudioExporter::Option>> AudioExporter::builtInPresets() {
    return {
        {tr("WAV - Mix all tracks"),
         {
             ".",
             "${projectName}",
             talcs::AudioFormatIO::WAV | talcs::AudioFormatIO::FLOAT,
             100,
             48000,
             "wav",
             Option::AllTracks,
             {},
             Option::Mixed,
             {},
             true,
             Option::All,
             0,
             -1,
         }},
        {tr("WAV - Separate all tracks"),
         {
             ".",
             "${projectName}",
             talcs::AudioFormatIO::WAV | talcs::AudioFormatIO::FLOAT,
             100,
             48000,
             "wav",
             Option::AllTracks,
             {},
             Option::SeparatedThroughMasterTrack,
             {},
             true,
             Option::All,
             0,
             -1,
         }},
        {tr("FLAC - Mix all tracks"),
         {
             ".",
             "${projectName}",
             talcs::AudioFormatIO::FLAC | talcs::AudioFormatIO::PCM_24,
             100,
             48000,
             "flac",
             Option::AllTracks,
             {},
             Option::Mixed,
             {},
             true,
             Option::All,
             0,
             -1,
         }},
        {tr("FLAC - Separate all tracks"),
         {
             ".",
             "${projectName}",
             talcs::AudioFormatIO::FLAC | talcs::AudioFormatIO::PCM_24,
             100,
             48000,
             "flac",
             Option::AllTracks,
             {},
             Option::SeparatedThroughMasterTrack,
             {},
             true,
             Option::All,
             0,
             -1,
         }},
        {tr("MP3 - Mix all tracks"),
         {
             ".",
             "${projectName}",
             talcs::AudioFormatIO::MPEG | talcs::AudioFormatIO::MPEG_LAYER_III,
             100,
             48000,
             "mp3",
             Option::AllTracks,
             {},
             Option::Mixed,
             {},
             true,
             Option::All,
             0,
             -1,
         }},
        {tr("MP3 - Separate all tracks"),
         {
             ".",
             "${projectName}",
             talcs::AudioFormatIO::MPEG | talcs::AudioFormatIO::MPEG_LAYER_III,
             100,
             48000,
             "mp3",
             Option::AllTracks,
             {},
             Option::SeparatedThroughMasterTrack,
             {},
             true,
             Option::All,
             0,
             -1,
         }},
    };
}

AudioExporter::AudioExporter(QObject *parent) : QObject(parent) {
}

AudioExporter::~AudioExporter() {
}

#define optionToPreset(key) preset[#key] = m_option.key
#define presetToOption(key) m_option.key = preset[#key].value<decltype(m_option.key)>()

void AudioExporter::savePreset(const QString &name) const {
    QSettings settings;
    settings.beginGroup("audio/exportPresets");
    QVariantMap preset;
    optionToPreset(fileName);
    optionToPreset(formatFlag);
    optionToPreset(vbrQuality);
    optionToPreset(sampleRate);
    optionToPreset(extensionName);
    optionToPreset(sourceOption);
    optionToPreset(mixingOption);
    optionToPreset(affix);
    optionToPreset(enableMuteSolo);
    optionToPreset(timeRangeOption);

    settings.setValue(name, preset);
}
bool AudioExporter::loadPreset(const QString &name) {
    QSettings settings;
    settings.beginGroup("audio/exportPresets");
    if (!settings.contains(name))
        return false;
    auto preset = settings.value(name).toMap();
    presetToOption(fileName);
    presetToOption(formatFlag);
    presetToOption(vbrQuality);
    presetToOption(sampleRate);
    presetToOption(extensionName);
    presetToOption(sourceOption);
    presetToOption(mixingOption);
    presetToOption(affix);
    presetToOption(enableMuteSolo);
    presetToOption(timeRangeOption);
    return true;
}
bool AudioExporter::deletePreset(const QString &name) const {
    QSettings settings;
    settings.beginGroup("audio/exportPresets");
    if (!settings.contains(name))
        return false;
    settings.remove(name);
    return true;
}
void AudioExporter::setOption(const AudioExporter::Option &option) {
    m_option = option;
}
AudioExporter::Option AudioExporter::option() const {
    return m_option;
}
QStringList AudioExporter::outputFileList() const {
    auto directory = QDir(m_option.fileDirectory); // TODO
    auto fileNameTemplate = m_option.fileName + "." + m_option.extensionName;
    fileNameTemplate.replace("${projectName}", "untitled"); // TODO
    fileNameTemplate.replace("${tempo}", QString::number(AppModel::instance()->tempo()));
    fileNameTemplate.replace("${timeSignature}", QString("%1-%2").arg(AppModel::instance()->numerator()).arg(AppModel::instance()->denominator()));
    fileNameTemplate.replace("${sampleRate}", QString::number(m_option.sampleRate));

    if (m_option.mixingOption == Option::Mixed) {
        return {directory.filePath(fileNameTemplate)};
    } else {
        QStringList list;
        QList<int> indices;
        if (m_option.sourceOption == Option::AllTracks) {
            for (int i = 0; i < AppModel::instance()->tracks().size(); i++) {
                indices.append(i);
            }
        } else if (m_option.sourceOption == Option::SelectedTracks) {

        } else {
            indices = m_option.selectedTrackIndices;
        }
        for (int i : indices) {
            auto track = AppModel::instance()->tracks()[i];
            auto trackFileNameTemplate = fileNameTemplate;
            auto trackAffix = m_option.affix;
            trackAffix.replace("${trackName}", track->name());
            trackAffix.replace("${trackIndex}", QString::number(i + 1));
            trackAffix.replace("${gain}", QString::number(track->control().gain()));
            trackAffix.replace("${pan}", QString::number(track->control().pan()));
            trackFileNameTemplate.replace("${trackAffix}", trackAffix);
            list.append(directory.filePath(trackFileNameTemplate));
        }
        return list;
    }
}

AudioExporter::Status AudioExporter::exec() {
    return AudioExporter::Success;
}
void AudioExporter::interrupt() {
}
