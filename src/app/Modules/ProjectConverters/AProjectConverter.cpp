//
// Created by fluty on 2024/2/7.
//

#include "AProjectConverter.h"

#include "Model/AppModel/AudioClip.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>

#include "Model/AppModel/Track.h"

#include "Model/AppModel/Note.h"
#include "Model/AppModel/SingingClip.h"

static QMap<QString, QString> languageMapping = {
    {"CHN", "cmn"}
};

QString langMapping(const QString &lang) {
    if (languageMapping.contains(lang))
        return languageMapping[lang];
    return "unknown";
}

bool AProjectConverter::load(const QString &path, AppModel *model, QString &errMsg,
                             ImportMode mode) {
    auto openJsonFile = [](const QString &filename, QJsonObject *jsonObj) {
        QFile loadFile(filename);
        if (!loadFile.open(QIODevice::ReadOnly)) {
            qDebug() << "Failed to open project file";
            return false;
        }
        const QByteArray allData = loadFile.readAll();
        loadFile.close();
        QJsonParseError err;
        const QJsonDocument json = QJsonDocument::fromJson(allData, &err);
        if (err.error != QJsonParseError::NoError)
            return false;
        if (json.isObject()) {
            *jsonObj = json.object();
        }
        return true;
    };

    auto decodeNotes = [&](const QJsonArray &arrNotes) {
        QList<Note *> notes;
        // QList<Phoneme> phonemes;
        // Phoneme phoneme;
        for (const auto valNote : arrNotes) {
            auto objNote = valNote.toObject();
            const auto note = new Note;
            note->setGlobalStart(objNote.value("pos").toInt());
            note->setLength(objNote.value("dur").toInt());
            note->setKeyIndex(objNote.value("pitch").toInt());
            note->setLyric(objNote.value("lyric").toString());
            note->setLanguage(langMapping(objNote.value("language").toString()));
            note->setPronunciation(Pronunciation("", ""));

            // const auto headPhonemes = objNote.value("headConsonants").toArray();
            // if (headPhonemes.count() == 0) {
            //     phoneme.type = Phoneme::Normal;
            //     phoneme.start = 0;
            //     phonemes.append(phoneme);
            // } else if (headPhonemes.count() == 1) {
            //     phoneme.type = Phoneme::Ahead;
            //     const auto startTick = headPhonemes.first().toInt();
            //     const auto startMs = startTick * 60000 / m_tempo / 480;
            //     phoneme.start = qRound(startMs);
            //     phonemes.append(phoneme);
            //
            //     phoneme.type = Phoneme::Normal;
            //     phoneme.start = 0;
            //     phonemes.append(phoneme);
            // }
            //
            // note->setPhonemeInfo(Note::Edited, phonemes);
            notes.append(note);
            // phonemes.clear();
        }
        return notes;
    };

    auto decodeClips = [&](const QJsonArray &arrClips, Track *dsTack, const QString &type) {
        for (const auto &valClip : arrClips) {
            auto objClip = valClip.toObject();
            if (type == "sing") {
                const auto singingClip = new SingingClip;
                singingClip->setName(objClip.value("name").toString());
                singingClip->setStart(objClip.value("pos").toInt());
                singingClip->setClipStart(objClip.value("clipPos").toInt());
                singingClip->setLength(objClip.value("dur").toInt());
                singingClip->setClipLen(objClip.value("clipDur").toInt());
                auto arrNotes = objClip.value("notes").toArray();
                auto notes = decodeNotes(arrNotes);
                for (const auto &note : notes)
                    singingClip->insertNote(note);
                dsTack->insertClip(singingClip);
            } else if (type == "audio") {
                const auto audioClip = new AudioClip;
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
    auto decodeTracks = [&](const QJsonArray &arrTracks, AppModel *model) {
        int i = 0;
        for (const auto &valTrack : arrTracks) {
            auto objTrack = valTrack.toObject();
            auto type = objTrack.value("type").toString();
            const auto track = new Track;
            decodeClips(objTrack.value("patterns").toArray(), track, type);
            model->insertTrack(track, i);
            i++;
        }
    };

    QJsonObject objAProject;
    if (openJsonFile(path, &objAProject)) {
        model->setTimeSignature(TimeSignature(objAProject.value("beatsPerBar").toInt(), 4));
        const auto tempo =
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