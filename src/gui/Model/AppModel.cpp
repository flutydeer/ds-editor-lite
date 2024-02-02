//
// Created by fluty on 2024/1/27.
//

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "AppModel.h"

double AppModel::tempo() const {
    return m_tempo;
}
void AppModel::setTempo(double tempo) {
    m_tempo = tempo;
    emit tempoChanged(m_tempo);
}
const QList<DsTrack *> &AppModel::tracks() const {
    return m_tracks;
}

void AppModel::insertTrack(DsTrack *track, int index) {
    connect(track, &DsTrack::clipChanged, this, [=] {
        auto trackIndex = m_tracks.indexOf(track);
        emit tracksChanged(Update, trackIndex);
    });
    m_tracks.insert(index, track);
    emit tracksChanged(Insert, index);
}
void AppModel::removeTrack(int index) {
    m_tracks.removeAt(index);
    emit tracksChanged(Remove, index);
}
bool AppModel::loadAProject(const QString &filename) {
    reset();

    auto openJsonFile = [](const QString &filename, QJsonObject *jsonObj) {
        QFile loadFile(filename);
        if (!loadFile.open(QIODevice::ReadOnly)) {
            qDebug() << "Failed to open project file";
            return false;
        }
        QByteArray allData = loadFile.readAll();
        loadFile.close();
        QJsonParseError err;
        QJsonDocument json = QJsonDocument::fromJson(allData, &err);
        if (err.error != QJsonParseError::NoError)
            return false;
        if (json.isObject()) {
            *jsonObj = json.object();
        }
        return true;
    };

    auto decodeNotes = [](const QJsonArray &arrNotes) {
        QList<DsNote> notes;
        for (const auto valNote : qAsConst(arrNotes)) {
            auto objNote = valNote.toObject();
            DsNote note;
            note.setStart(objNote.value("pos").toInt());
            note.setLength(objNote.value("dur").toInt());
            note.setKeyIndex(objNote.value("pitch").toInt());
            note.setLyric(objNote.value("lyric").toString());
            note.setPronunciation("la");
            notes.append(note);
        }
        return notes;
    };

    auto decodeClips = [&](const QJsonArray &arrClips, DsTrack *dsTack, const QString &type,
                           int trackIndex) {
        for (const auto &valClip : qAsConst(arrClips)) {
            auto objClip = valClip.toObject();
            if (type == "sing") {
                auto singingClip = new DsSingingClip;
                singingClip->trackIdex = trackIndex;
                singingClip->setName("Clip");
                singingClip->setStart(objClip.value("pos").toInt());
                singingClip->setClipStart(objClip.value("clipPos").toInt());
                singingClip->setLength(objClip.value("dur").toInt());
                singingClip->setClipLen(objClip.value("clipDur").toInt());
                auto arrNotes = objClip.value("notes").toArray();
                singingClip->notes.append(decodeNotes(arrNotes));
                dsTack->insertClip(singingClip);
            } else if (type == "audio") {
                auto audioClip = new DsAudioClip;
                audioClip->trackIdex = trackIndex;
                audioClip->setName("Clip");
                audioClip->setStart(objClip.value("pos").toInt());
                audioClip->setClipStart(objClip.value("clipPos").toInt());
                audioClip->setLength(objClip.value("dur").toInt());
                audioClip->setClipLen(objClip.value("clipDur").toInt());
                audioClip->setPath(objClip.value("path").toString());
                dsTack->insertClip(audioClip);
            }
        }
    };

    auto decodeTracks = [&](const QJsonArray &arrTracks, QList<DsTrack *> &tracks) {
        int i = 0;
        for (const auto &valTrack : qAsConst(arrTracks)) {
            auto objTrack = valTrack.toObject();
            auto type = objTrack.value("type").toString();
            auto track = new DsTrack;
            decodeClips(objTrack.value("patterns").toArray(), track, type, i);
            tracks.append(track);
            i++;
        }
    };

    QJsonObject objAProject;
    if (openJsonFile(filename, &objAProject)) {
        numerator = objAProject.value("beatsPerBar").toInt();
        m_tempo = objAProject.value("tempos").toArray().first().toObject().value("bpm").toDouble();
        decodeTracks(objAProject.value("tracks").toArray(), m_tracks);
        // auto clip = tracks().first().clips.first().dynamicCast<DsSingingClip>();
        // qDebug() << clip->notes.count();
        emit modelChanged();
        return true;
    }
    return false;
}
void AppModel::onTrackUpdated(int index) {
    emit tracksChanged(Update, index);
}
void AppModel::onSelectedClipChanged(int trackIndex, int clipIndex) {
    m_selectedClipTrackIndex = trackIndex;
    m_selectedClipIndex = clipIndex;
    emit selectedClipChanged(m_selectedClipTrackIndex, m_selectedClipIndex);
}
void AppModel::reset() {
    m_tracks.clear();
}