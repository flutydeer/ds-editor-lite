//
// Created by fluty on 2024/2/7.
//

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "AProjectConverter.h"
bool AProjectConverter::load(const QString &path, AppModel *model, QString &errMsg,
                             ImportMode mode) {
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
        QList<DsNote *> notes;
        for (const auto valNote : qAsConst(arrNotes)) {
            auto objNote = valNote.toObject();
            auto note = new DsNote;
            note->setStart(objNote.value("pos").toInt());
            note->setLength(objNote.value("dur").toInt());
            note->setKeyIndex(objNote.value("pitch").toInt());
            note->setLyric(objNote.value("lyric").toString());
            note->setPronunciation("la");
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
                singingClip->setName("Clip");
                singingClip->setStart(objClip.value("pos").toInt());
                singingClip->setClipStart(objClip.value("clipPos").toInt());
                singingClip->setLength(objClip.value("dur").toInt());
                singingClip->setClipLen(objClip.value("clipDur").toInt());
                auto arrNotes = objClip.value("notes").toArray();
                auto notes = decodeNotes(arrNotes);
                for (auto &note : notes)
                    singingClip->insertNote(note);
                dsTack->insertClip(singingClip);
            } else if (type == "audio") {
                auto audioClip = new DsAudioClip;
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
    auto decodeTracks = [&](const QJsonArray &arrTracks, AppModel *model) {
        int i = 0;
        for (const auto &valTrack : qAsConst(arrTracks)) {
            auto objTrack = valTrack.toObject();
            auto type = objTrack.value("type").toString();
            auto track = new DsTrack;
            decodeClips(objTrack.value("patterns").toArray(), track, type, i);
            model->insertTrack(track, i);
            i++;
        }
    };

    QJsonObject objAProject;
    if (openJsonFile(path, &objAProject)) {
        model->setTimeSignature(
            AppModel::TimeSignature(objAProject.value("beatsPerBar").toInt(), 4));
        model->setTempo(
            objAProject.value("tempos").toArray().first().toObject().value("bpm").toDouble());
        decodeTracks(objAProject.value("tracks").toArray(), model);
        return true;
    }
    return false;
}
bool AProjectConverter::save(const QString &path, AppModel *model, QString &errMsg) {
    return false;
}