//
// Created by fluty on 2024/1/27.
//

#include "AppModel.h"

#include "AppModel_p.h"
#include "SingingClip.h"
#include "Track.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/ProjectConverters/AProjectConverter.h"
#include "Modules/ProjectConverters/DspxProjectConverter.h"
#include "Modules/ProjectConverters/MidiConverter.h"
#include "Utils/Log.h"
#include "Utils/MathUtils.h"

#include <QJsonArray>

AppModel::AppModel() : d_ptr(new AppModelPrivate(this)) {
}

AppModel::~AppModel() {
    delete d_ptr;
}

TimeSignature AppModel::timeSignature() const {
    Q_D(const AppModel);
    return d->m_timeSignature;
}

void AppModel::setTimeSignature(const TimeSignature &signature) {
    Q_D(AppModel);
    d->m_timeSignature = signature;
    emit timeSignatureChanged(d->m_timeSignature.numerator, d->m_timeSignature.denominator);
}

double AppModel::tempo() const {
    Q_D(const AppModel);
    return d->m_tempo;
}

void AppModel::setTempo(double tempo) {
    Q_D(AppModel);
    d->m_tempo = tempo;
    emit tempoChanged(d->m_tempo);
}

const QList<Track *> &AppModel::tracks() const {
    Q_D(const AppModel);
    return d->m_tracks;
}

void AppModel::insertTrack(Track *track, qsizetype index) {
    Q_D(AppModel);
    d->m_tracks.insert(index, track);
    emit trackChanged(Insert, index, track);
}

void AppModel::appendTrack(Track *track) {
    Q_D(AppModel);
    insertTrack(track, d->m_tracks.count());
}

void AppModel::removeTrackAt(qsizetype index) {
    Q_D(AppModel);
    auto track = d->m_tracks[index];
    d->m_tracks.removeAt(index);
    emit trackChanged(Remove, index, track);
}

void AppModel::removeTrack(Track *track) {
    Q_D(AppModel);
    auto index = d->m_tracks.indexOf(track);
    removeTrackAt(index);
}

void AppModel::clearTracks() {
    Q_D(AppModel);
    while (d->m_tracks.count() > 0)
        removeTrackAt(0);
}

// QJsonObject AppModel::globalWorkspace() const {
//     Q_D(const AppModel);
//     return d->m_workspace;
// }
// bool AppModel::isWorkspaceExist(const QString &id) const {
//     Q_D(const AppModel);
//     return d->m_workspace.contains(id);
// }
// QJsonObject AppModel::getPrivateWorkspaceById(const QString &id) const {
//     Q_D(const AppModel);
//     auto obj = d->m_workspace.value(id).toObject();
//     return obj;
// }
// std::unique_ptr<WorkspaceEditor> AppModel::workspaceEditor(const QString &id) {
//     Q_D(const AppModel);
//     return std::make_unique<WorkspaceEditor>(d->m_workspace, id);
// }

void AppModel::newProject() {
    Q_D(AppModel);
    d->reset();

    auto singingClip = new SingingClip;
    int bars = 4;
    auto timeSig = appModel->timeSignature();
    int length = 1920 * timeSig.numerator / timeSig.denominator * bars;
    singingClip->setName(tr("New Singing Clip"));
    singingClip->setStart(0);
    singingClip->setClipStart(0);
    singingClip->setLength(length);
    singingClip->setClipLen(length);
    auto newTrack = new Track;
    newTrack->setName(tr("New Track"));
    newTrack->insertClip(singingClip);
    d->m_tracks.append(newTrack);

    emit modelChanged();
    d->dispose();
}

bool AppModel::loadProject(const QString &filename) {
    Q_D(AppModel);
    d->reset();
    DspxProjectConverter converter;
    QString errMsg;
    AppModel resultModel;
    auto ok = converter.load(filename, &resultModel, errMsg, ImportMode::NewProject);
    if (ok)
        loadFromAppModel(resultModel);
    return ok;
}

bool AppModel::saveProject(const QString &filename) {
    Q_D(AppModel);
    DspxProjectConverter converter;
    QString errMsg;
    auto ok = converter.save(filename, this, errMsg);
    return ok;
}

bool AppModel::importAceProject(const QString &filename) {
    Q_D(AppModel);
    AProjectConverter converter;
    QString errMsg;
    AppModel resultModel;
    auto ok = converter.load(filename, &resultModel, errMsg, ImportMode::NewProject);
    if (ok) {
        for (const auto track : resultModel.tracks()) {
            track->setDefaultLanguage(appOptions->general()->defaultSingingLanguage);
            for (const auto clip : track->clips()) {
                if (clip->clipType() == Clip::Singing) {
                    auto singingClip = reinterpret_cast<SingingClip *>(clip);
                    singingClip->defaultLanguage = track->defaultLanguage();
                    // NoteWordUtils::fillEditedPhonemeNames(singingClip->notes().toList());
                }
            }
        }
        loadFromAppModel(resultModel);
    }
    return ok;
}

void AppModel::loadFromAppModel(const AppModel &model) {
    Q_D(AppModel);
    d->reset();
    d->m_tempo = model.tempo();
    d->m_timeSignature = model.timeSignature();
    d->m_tracks = model.tracks();
    emit modelChanged();
    d->dispose();
    // emit tempoChanged(d->m_tempo);
}

QJsonObject AppModel::serialize() const {
    Q_D(const AppModel);
    const QJsonObject objGlobal{
        {"author",    QString()},
        {"centShift", 0        },
        {"name",      QString()}
    };

    const QJsonObject objControl{
        {"gain", 0    },
        {"mute", false},
        {"pan",  0    }
    };

    QJsonObject objMaster{
        {"control", objControl}
    };

    const QJsonObject objTempo{
        {"pos",   0         },
        {"value", d->m_tempo}
    };

    QJsonArray arrTempos = {objTempo};

    const auto objTimeSignature = d->m_timeSignature.serialize();
    QJsonArray arrTimeSignatures = {objTimeSignature};

    QJsonObject objTimeLine{
        {"labels",         QJsonArray()     },
        {"tempos",         arrTempos        },
        {"timeSignatures", arrTimeSignatures}
    };

    QJsonArray arrTracks;
    for (const auto track : d->m_tracks)
        arrTracks.append(track->serialize());

    QJsonObject objContent{
        {"global",    objGlobal    },
        {"master",    objMaster    },
        {"timeline",  objTimeLine  },
        {"tracks",    arrTracks    },
        {"workspace", QJsonObject()}
    };

    return QJsonObject{
        {"content", objContent},
        {"version", "0.0.1"   }
    };
}

bool AppModel::deserialize(const QJsonObject &obj) {
    return false;
}

bool AppModel::importMidiFile(const QString &filename) {
    Q_D(AppModel);
    QString errMsg;
    int midiImport = MidiConverter::midiImportHandler();

    if (midiImport == ImportMode::NewProject) {
        d->reset();
    } else if (midiImport == -1) {
        errMsg = "User canceled the import.";
        return false;
    }

    AppModel resultModel;
    MidiConverter converter(appModel->timeSignature(), appModel->tempo());
    const auto ok = converter.load(filename, &resultModel, errMsg,
                                   static_cast<IProjectConverter::ImportMode>(midiImport));
    Log::i("Midi importer", errMsg);
    if (ok) {
        if (midiImport == ImportMode::NewProject) {
            setTimeSignature(resultModel.timeSignature());
            setTempo(resultModel.tempo());
            loadFromAppModel(resultModel);
        } else if (midiImport == ImportMode::AppendToProject) {
            setTimeSignature(resultModel.timeSignature());
            setTempo(resultModel.tempo());
            for (const auto track : resultModel.tracks()) {
                appendTrack(track);
            }
        }
    }
    return ok;
}

bool AppModel::exportMidiFile(const QString &filename) {
    MidiConverter converter(appModel->timeSignature(), appModel->tempo());
    QString errMsg;
    Log::i("Midi exporter", errMsg);
    return converter.save(filename, this, errMsg);
}

Clip *AppModel::findClipById(int clipId, Track *&trackRef) const {
    Q_D(const AppModel);
    for (const auto track : d->m_tracks) {
        if (const auto result = track->findClipById(clipId)) {
            trackRef = track;
            return result;
        }
    }
    trackRef = nullptr;
    return nullptr;
}

Clip *AppModel::findClipById(int clipId, int &trackIndex) {
    Q_D(const AppModel);
    int i = 0;
    for (const auto track : d->m_tracks) {
        if (auto result = track->findClipById(clipId)) {
            trackIndex = i;
            return result;
        }
        i++;
    }
    return nullptr;
}

Clip *AppModel::findClipById(int clipId) {
    Q_D(const AppModel);
    if (clipId == -1)
        return nullptr;

    for (const auto track : d->m_tracks) {
        if (const auto result = track->findClipById(clipId))
            return result;
    }
    return nullptr;
}

Track *AppModel::findTrackById(int id, int &trackIndex) {
    Q_D(const AppModel);
    int i = 0;
    for (auto track : d->m_tracks) {
        if (track->id() == id) {
            trackIndex = i;
            return track;
        }
        i++;
    }
    trackIndex = -1;
    return nullptr;
}

Track *AppModel::findTrackById(int id) {
    Q_D(const AppModel);
    return MathUtils::findItemById<Track *>(d->m_tracks, id);
}

double AppModel::tickToMs(double tick) const {
    Q_D(const AppModel);
    return tick * 60 / d->m_tempo / 480 * 1000;
}

double AppModel::msToTick(double ms) const {
    Q_D(const AppModel);
    return ms * 480 * d->m_tempo / 60000;
}

int AppModel::projectLengthInTicks() const {
    Q_D(const AppModel);
    int length = 0;
    for (const auto track : d->m_tracks)
        for (const auto clip : track->clips())
            if (clip->endTick() > length)
                length = clip->endTick();
    return length;
}

void AppModelPrivate::reset() {
    m_tempo = 120;
    m_timeSignature.numerator = 4;
    m_timeSignature.denominator = 4;
    m_previousTracks = m_tracks;
    m_tracks.clear();
}

void AppModelPrivate::dispose() const {
    qDebug() << "dispose";
    for (int i = 0; i < m_previousTracks.count(); i++) {
        auto track = m_previousTracks.at(i);
        delete track;
    }
}