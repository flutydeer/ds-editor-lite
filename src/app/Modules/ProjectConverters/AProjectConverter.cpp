//
// Created by fluty on 2024/2/7.
//

#include "AProjectConverter.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>

#include "Model/Track.h"
#include "Model/Clip.h"
#include "Model/Note.h"

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

    auto decodeNotes = [&](const QJsonArray &arrNotes) {
        QList<Note *> notes;
        QList<Phoneme> phonemes;
        Phoneme phoneme;
        for (const auto valNote : arrNotes) {
            auto objNote = valNote.toObject();
            auto note = new Note;
            note->setStart(objNote.value("pos").toInt());
            note->setLength(objNote.value("dur").toInt());
            note->setKeyIndex(objNote.value("pitch").toInt());
            note->setLyric(objNote.value("lyric").toString());
            note->setPronunciation(Pronunciation("la", ""));

            auto headPhonemes = objNote.value("headConsonants").toArray();
            if (headPhonemes.count() == 0) {
                phoneme.type = Phoneme::Normal;
                phoneme.start = 0;
                phonemes.append(phoneme);
            } else if (headPhonemes.count() == 1) {
                phoneme.type = Phoneme::Ahead;
                auto startTick = headPhonemes.first().toInt();
                auto startMs = startTick * 60000 / m_tempo / 480;
                phoneme.start = qRound(startMs);
                phonemes.append(phoneme);

                phoneme.type = Phoneme::Normal;
                phoneme.start = 0;
                phonemes.append(phoneme);
            }

            note->setPhonemes(Phonemes::Edited, phonemes);
            notes.append(note);
            phonemes.clear();
        }
        return notes;
    };

    auto decodeClips = [&](const QJsonArray &arrClips, Track *dsTack, const QString &type,
                           int trackIndex) {
        for (const auto &valClip : arrClips) {
            auto objClip = valClip.toObject();
            if (type == "sing") {
                auto singingClip = new SingingClip;
                singingClip->setName(objClip.value("name").toString());
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
                auto audioClip = new AudioClip;
                audioClip->setStart(objClip.value("pos").toInt());
                audioClip->setClipStart(objClip.value("clipPos").toInt());
                audioClip->setLength(objClip.value("dur").toInt());
                audioClip->setClipLen(objClip.value("clipDur").toInt());
                audioClip->setPath(objClip.value("path").toString());
                audioClip->setName(QFileInfo(audioClip->path()).fileName());
                dsTack->insertClip(audioClip);
            }
        }
    };
    auto decodeTracks = [&](const QJsonArray &arrTracks, AppModel *appModel) {
        int i = 0;
        for (const auto &valTrack : arrTracks) {
            auto objTrack = valTrack.toObject();
            auto type = objTrack.value("type").toString();
            auto track = new Track;
            decodeClips(objTrack.value("patterns").toArray(), track, type, i);
            appModel->insertTrack(track, i);
            i++;
        }
    };

    QJsonObject objAProject;
    if (openJsonFile(path, &objAProject)) {
        model->setTimeSignature(
            AppModel::TimeSignature(objAProject.value("beatsPerBar").toInt(), 4));
        auto tempo =
            objAProject.value("tempos").toArray().first().toObject().value("bpm").toDouble();
        m_tempo = tempo;
        model->setTempo(tempo);
        decodeTracks(objAProject.value("tracks").toArray(), model);
        return true;
    }
    return false;
}
bool AProjectConverter::save(const QString &path, AppModel *model, QString &errMsg) {
    return false;
}