//
// Created by fluty on 2024/1/27.
//

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QTextCodec>
#include <QMessageBox>
#include <QPushButton>
#include <QDialog>
#include <QVBoxLayout>
#include <QCheckBox>
#include <QDialogButtonBox>

#include "AppModel.h"

#include "Controller/History/HistoryManager.h"
#include "Utils/IdGenerator.h"
#include "Utils/ProjectConverters/AProjectConverter.h"
#include "Utils/ProjectConverters/DspxProjectConverter.h"
#include "opendspx/qdspxtrack.h"
#include "opendspx/qdspxtimeline.h"
#include "opendspx/qdspxmodel.h"
#include "opendspx/converters/midi.h"
#include "Utils/ProjectConverters/MidiConverter.h"

AppModel::TimeSignature AppModel::timeSignature() const {
    return m_timeSignature;
}
void AppModel::setTimeSignature(const TimeSignature &signature) {
    m_timeSignature = signature;
    emit timeSignatureChanged(m_timeSignature.numerator, m_timeSignature.denominator);
}
double AppModel::tempo() const {
    return m_tempo;
}
void AppModel::setTempo(double tempo) {
    qDebug() << "AppModel::setTempo" << tempo;
    m_tempo = tempo;
    emit tempoChanged(m_tempo);
}
const QList<DsTrack *> &AppModel::tracks() const {
    return m_tracks;
}

void AppModel::insertTrack(DsTrack *track, int index) {
    connect(track, &DsTrack::propertyChanged, this, [=] {
        auto trackIndex = m_tracks.indexOf(track);
        emit tracksChanged(PropertyUpdate, trackIndex, track);
    });
    m_tracks.insert(index, track);
    emit tracksChanged(Insert, index, track);
}
void AppModel::insertTrackQuietly(DsTrack *track, int index) {
    connect(track, &DsTrack::propertyChanged, this, [=] {
        auto trackIndex = m_tracks.indexOf(track);
        emit tracksChanged(PropertyUpdate, trackIndex, track);
    });
    m_tracks.insert(index, track);
}
void AppModel::appendTrack(DsTrack *track) {
    insertTrack(track, m_tracks.count());
}
void AppModel::removeTrackAt(int index) {
    auto track = m_tracks[index];
    onSelectedClipChanged(-1, -1);
    m_tracks.removeAt(index);
    emit tracksChanged(Remove, index, track);
}
void AppModel::removeTrack(DsTrack *track) {
    auto index = m_tracks.indexOf(track);
    removeTrackAt(index);
}
void AppModel::clearTracks() {
    while (m_tracks.count() > 0)
        removeTrackAt(0);
}
int AppModel::quantize() const {
    return m_quantize;
}
void AppModel::setQuantize(int quantize) {
    m_quantize = quantize;
    emit quantizeChanged(quantize);
    qDebug() << "AppModel quantizeChanged" << quantize;
}
void AppModel::newProject() {
    reset();
    emit modelChanged();
}

bool AppModel::loadProject(const QString &filename) {
    reset();
    auto converter = new DspxProjectConverter;
    QString errMsg;
    auto ok = converter->load(filename, this, errMsg);
    emit modelChanged();
    return ok;
}
bool AppModel::saveProject(const QString &filename) {
    auto converter = new DspxProjectConverter;
    QString errMsg;
    auto ok = converter->save(filename, this, errMsg);
    return ok;
}
bool AppModel::importAProject(const QString &filename) {
    reset();
    auto converter = new AProjectConverter;
    QString errMsg;
    auto ok = converter->load(filename, this, errMsg);
    emit modelChanged();
    return ok;
}

bool AppModel::importMidiFile(const QString &filename) {
    QString errMsg;
    int midiImport = MidiConverter::midiImportHandler();

    if (midiImport == ImportMode::NewProject) {
        reset();
    } else if (midiImport == -1) {
        errMsg = "User canceled the import.";
        return false;
    }

    auto converter = new MidiConverter;
    auto ok = converter->load(filename, this, errMsg,
                              static_cast<IProjectConverter::ImportMode>(midiImport));
    if (midiImport == ImportMode::NewProject) {
        emit modelChanged();
    }
    return ok;
}

bool AppModel::exportMidiFile(const QString &filename) {
    auto converter = new MidiConverter;
    QString errMsg;
    auto ok = converter->save(filename, this, errMsg);
    return ok;
}

int AppModel::selectedTrackIndex() const {
    return m_selectedTrackIndex;
}
void AppModel::onSelectedClipChanged(int trackIndex, int clipId) {
    qDebug() << "AppModel::onSelectedClipChanged" << trackIndex << clipId;
    m_selectedClipTrackIndex = trackIndex;
    m_selectedClipId = clipId;
    emit selectedClipChanged(m_selectedClipTrackIndex, m_selectedClipId);
}
void AppModel::setSelectedTrack(int trackIndex) {
    m_selectedTrackIndex = trackIndex;
    emit selectedTrackChanged(trackIndex);
}
void AppModel::reset() {
    m_tempo = 120;
    m_timeSignature.numerator = 4;
    m_timeSignature.denominator = 4;
    m_tracks.clear();
}