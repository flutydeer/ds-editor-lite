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

    auto decodeCurve = [&](const QJsonArray &arrCurve) {
        QVector<DsCurve> dsCurves;
        for (const auto valCurve : qAsConst(arrCurve)) {
            auto objCurve = valCurve.toObject();
            auto type = objCurve.value("type").toString();
            if (type == "free") {
                DsDrawCurve drawCurve;
                drawCurve.setStart(objCurve.value("start").toInt());
                drawCurve.step = objCurve.value("step").toInt();
                auto arrValues = objCurve.value("values").toArray();
                for (const auto valValue : qAsConst(arrValues)) {
                    drawCurve.insertValue(valValue.toInt());
                }
                dsCurves.append(drawCurve);
            } else if (type == "anchor") {
                DsAnchorCurve anchorCurve;
                anchorCurve.setStart(objCurve.value("start").toInt());
                auto arrNodes = objCurve.value("nodes").toArray();
                for (const auto &valNode : qAsConst(arrNodes)) {
                    auto objNode = valNode.toObject();
                    DsAnchorNode node(objNode.value("x").toInt(), objNode.value("y").toInt());
                    if (objNode.value("interpMode").toString() == "linear") {
                        node.setInterpMode(DsAnchorNode::Linear);
                    } else if (objNode.value("interpMode").toString() == "hermite") {
                        node.setInterpMode(DsAnchorNode::Hermite);
                    } else if (objNode.value("interpMode").toString() == "cubic") {
                        node.setInterpMode(DsAnchorNode::Cubic);
                    } else {
                        node.setInterpMode(DsAnchorNode::None);
                    }
                    anchorCurve.insertNode(&node);
                }
                dsCurves.append(anchorCurve);
            }
        }
        return dsCurves;
    };

    auto decodeSingingParam = [&](const QJsonObject &objParam) {
        DsParam dsParams;
        if (!(objParam.value("original").isUndefined() || objParam.value("original").isNull())) {
            auto objOriginal = objParam.value("original").toArray();
            auto curves = decodeCurve(objOriginal);
            for (auto &curve : curves)
                dsParams.original.add(&curve);
        }
        if (!(objParam.value("edited").isUndefined() || objParam.value("edited").isNull())) {
            auto objEdited = objParam.value("edited").toArray();
            auto curves = decodeCurve(objEdited);
            for (auto &curve : curves)
                dsParams.edited.add(&curve);
        }
        if (!(objParam.value("envelope").isUndefined() || objParam.value("envelope").isNull())) {
            auto objEnvelope = objParam.value("envelope").toArray();
            auto curves = decodeCurve(objEnvelope);
            for (auto &curve : curves)
                dsParams.envelope.add(&curve);
        }
        return dsParams;
    };

    auto decodeSingingParams = [&](const QJsonObject &objParams) {
        DsParams dsParams;
        if (!(objParams.value("pitch").isUndefined() || objParams.value("pitch").isNull())) {
            auto objPitch = objParams.value("pitch").toObject();
            dsParams.pitch = decodeSingingParam(objPitch);
        }
        if (!(objParams.value("energy").isUndefined() || objParams.value("energy").isNull())) {
            auto objEnergy = objParams.value("energy").toObject();
            dsParams.energy = decodeSingingParam(objEnergy);
        }
        if (!(objParams.value("tension").isUndefined() || objParams.value("tension").isNull())) {
            auto objTension = objParams.value("tension").toObject();
            dsParams.tension = decodeSingingParam(objTension);
        }
        if (!(objParams.value("breathiness").isUndefined() ||
              objParams.value("breathiness").isNull())) {
            auto objBreathiness = objParams.value("breathiness").toObject();
            dsParams.breathiness = decodeSingingParam(objBreathiness);
        }
        return dsParams;
    };

    auto decodePhonemes = [&](const QJsonArray &arrPhonemes) {
        QList<DsPhoneme> phonemes;
        for (const auto &valPhoneme : qAsConst(arrPhonemes)) {
            auto objPhoneme = valPhoneme.toObject();
            auto type = DsPhoneme::DsPhonemeType::Normal;
            if (objPhoneme.value("type").toString() == "ahead") {
                type = DsPhoneme::DsPhonemeType::Ahead;
            } else if (objPhoneme.value("type").toString() == "final") {
                type = DsPhoneme::DsPhonemeType::Final;
            } else {
                type = DsPhoneme::DsPhonemeType::Normal;
            }
            auto token = objPhoneme.value("token").toString();
            auto start = objPhoneme.value("start").toInt();
            phonemes.append(DsPhoneme(type, token, start));
        }
        return phonemes;
    };

    auto decodeNotes = [&](const QJsonArray &arrNotes) {
        QList<DsNote> notes;
        for (const auto &valNote : qAsConst(arrNotes)) {
            auto objNote = valNote.toObject();
            auto objPhonemes = objNote.value("phonemes").toObject();
            DsNote note;
            note.setStart(objNote.value("pos").toInt());
            note.setLength(objNote.value("length").toInt());
            note.setKeyIndex(objNote.value("keyNum").toInt());
            note.setLyric(objNote.value("lyric").toString());
            note.setPronunciation("la");
            if (!(objPhonemes.value("original").isUndefined() ||
                  objPhonemes.value("original").isNull())) {
                note.phonemes.original.append(
                    decodePhonemes(objPhonemes.value("original").toArray()));
            }
            if (!(objPhonemes.value("edited").isUndefined() ||
                  objPhonemes.value("edited").isNull())) {
                note.phonemes.edited.append(decodePhonemes(objPhonemes.value("edited").toArray()));
            }
            notes.append(note);
        }
        return notes;
    };

    auto decodeClips = [&](const QJsonArray &arrClips, DsTrack *dsTack) {
        for (const auto &valClip : qAsConst(arrClips)) {
            auto objClip = valClip.toObject();
            auto type = objClip.value("type").toString();
            auto objTime = objClip.value("time").toObject();
            auto objControl = objClip.value("control").toObject();
            if (type == "singing") {
                auto singingClip = new DsSingingClip;
                singingClip->setName(objClip.value("name").toString());
                singingClip->setStart(objTime.value("start").toInt());
                singingClip->setClipStart(objTime.value("clipStart").toInt());
                singingClip->setLength(objTime.value("length").toInt());
                singingClip->setClipLen(objTime.value("clipLen").toInt());
                singingClip->setGain(objControl.value("gain").toDouble());
                singingClip->setMute(objControl.value("mute").toBool());
                auto arrNotes = objClip.value("notes").toArray();
                auto objParams = objClip.value("params").toObject();
                // singingClip->notes.append(decodeNotes(arrNotes));
                auto notes = decodeNotes(arrNotes);
                for (auto &note : notes)
                    singingClip->insertNote(&note);
                singingClip->params = decodeSingingParams(objParams);
                dsTack->insertClip(singingClip);
            } else if (type == "audio") {
                auto audioClip = new DsAudioClip;
                audioClip->setName(objClip.value("name").toString());
                audioClip->setStart(objTime.value("start").toInt());
                audioClip->setClipStart(objTime.value("clipStart").toInt());
                audioClip->setLength(objTime.value("length").toInt());
                audioClip->setClipLen(objTime.value("clipLen").toInt());
                audioClip->setGain(objControl.value("gain").toDouble());
                audioClip->setMute(objControl.value("mute").toBool());
                audioClip->setPath(objClip.value("path").toString());
                dsTack->insertClip(audioClip);
            }
        }
    };

    auto decodeTracks = [&](const QJsonArray &arrTracks, QList<DsTrack *> &tracks) {
        int i = 0;
        for (const auto &valTrack : qAsConst(arrTracks)) {
            auto objTrack = valTrack.toObject();
            auto objControl = objTrack.value("control").toObject();
            auto track = new DsTrack;
            auto trackControl = DsTrackControl();
            trackControl.setGain(objControl.value("gain").toDouble());
            trackControl.setPan(objControl.value("pan").toDouble());
            trackControl.setMute(objControl.value("mute").toBool());
            trackControl.setSolo(objControl.value("solo").toBool());
            track->setName(objTrack.value("name").toString());
            track->setControl(trackControl);
            decodeClips(objTrack.value("clips").toArray(), track);
            tracks.append(track);
            i++;
        }
    };

    QJsonObject objProject;
    if (openJsonFile(filename, &objProject)) {
        auto projContent = objProject.value("content").toObject();
        auto projTimeline = projContent.value("timeline").toObject();
        auto projTimesig = projTimeline.value("timeSignatures").toArray().first().toObject();
        m_timeSignature.numerator = projTimesig.value("numerator").toInt();
        m_timeSignature.denominator = projTimesig.value("denominator").toInt();
        m_tempo =
            projTimeline.value("tempos").toArray().first().toObject().value("value").toDouble();
        decodeTracks(projContent.value("tracks").toArray(), m_tracks);
        // auto clip = tracks().first().clips.first().dynamicCast<DsSingingClip>();
        // qDebug() << clip->notes.count();
        emit modelChanged();
        return true;
    }
    return false;
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