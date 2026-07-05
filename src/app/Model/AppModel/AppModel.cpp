//
// Created by fluty on 2024/1/27.
//

#include "AppModel.h"

#include "AppModel_p.h"
#include "LevelMeterViewModel.h"
#include "SingingClip.h"
#include "Track.h"
#include "Model/AppOptions/AppOptions.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/ProjectConverters/AProjectConverter.h"
#include "Modules/ProjectConverters/DspxProjectConverter.h"
#include "Modules/ProjectConverters/MidiConverter.h"
#include "Utils/Log.h"
#include "Utils/MathUtils.h"
#include "Utils/MusicTimeConverter.h"
#include "Global/AppGlobal.h"

#include <QJsonArray>

AppModel::AppModel(QObject *parent) : QObject(parent), d_ptr(new AppModelPrivate(this)) {
}

AppModel::~AppModel() {
    delete d_ptr;
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(AppModel)

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

void AppModel::setTempo(const double tempo) {
    Q_D(AppModel);
    d->m_tempo = tempo;
    for (const auto track : d->m_tracks) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() == IClip::Singing)
                static_cast<SingingClip *>(clip)->bumpInferenceRevision();
        }
    }
    emit tempoChanged(d->m_tempo);
}

TrackControl AppModel::masterControl() const {
    Q_D(const AppModel);
    return d->m_masterControl;
}

void AppModel::setMasterControl(const TrackControl &control) {
    Q_D(AppModel);
    d->m_masterControl = control;
    emit masterControlChanged(d->m_masterControl);
}

const QList<Track *> &AppModel::tracks() const {
    Q_D(const AppModel);
    return d->m_tracks;
}

void AppModel::insertTrack(Track *track, const qsizetype index) {
    Q_D(AppModel);
    if (track->colorIndex() == 0) {
        int prev = -1;
        if (index > 0 && index - 1 < d->m_tracks.size())
            prev = d->m_tracks[index - 1]->colorIndex();
        else if (index >= d->m_tracks.size() && !d->m_tracks.isEmpty())
            prev = d->m_tracks.last()->colorIndex();
        const int newIdx = (prev < 0) ? 0 : (prev + 1) % AppGlobal::paletteColorCount;
        track->setColorIndex(newIdx);
    }
    d->m_tracks.insert(index, track);

    auto vm = new LevelMeterViewModel(this);
    d->m_levelMeterViewModels.insert(index, vm);

    emit trackChanged(Insert, index, track);
}

void AppModel::appendTrack(Track *track) {
    Q_D(AppModel);
    insertTrack(track, d->m_tracks.count());
}

void AppModel::removeTrackAt(const qsizetype index) {
    Q_D(AppModel);
    const auto track = d->m_tracks[index];
    d->m_tracks.removeAt(index);

    if (index < d->m_levelMeterViewModels.size()) {
        auto vm = d->m_levelMeterViewModels.takeAt(index);
        delete vm;
    }

    emit trackChanged(Remove, index, track);
}

void AppModel::removeTrack(Track *track) {
    Q_D(AppModel);
    const auto index = d->m_tracks.indexOf(track);
    removeTrackAt(index);
}

void AppModel::clearTracks() {
    Q_D(AppModel);
    while (d->m_tracks.count() > 0)
        removeTrackAt(0);
}

LevelMeterViewModel *AppModel::levelMeterViewModelForTrack(const Track *track) const {
    Q_D(const AppModel);
    const auto index = d->m_tracks.indexOf(const_cast<Track *>(track));
    if (index < 0 || index >= d->m_levelMeterViewModels.size())
        return nullptr;
    return d->m_levelMeterViewModels.at(index);
}

LevelMeterViewModel *AppModel::levelMeterViewModelAt(qsizetype index) const {
    Q_D(const AppModel);
    if (index < 0 || index >= d->m_levelMeterViewModels.size())
        return nullptr;
    return d->m_levelMeterViewModels.at(index);
}

LevelMeterViewModel *AppModel::masterLevelMeterViewModel() const {
    Q_D(const AppModel);
    return d->m_masterLevelMeterViewModel;
}

void AppModel::clearAllClipStates() {
    Q_D(AppModel);
    for (auto vm : d->m_levelMeterViewModels)
        vm->resetClip();
    if (d->m_masterLevelMeterViewModel)
        d->m_masterLevelMeterViewModel->resetClip();
}

void AppModel::newProject() {
    Q_D(AppModel);
    d->reset();

    const auto singingClip = new SingingClip;
    constexpr int bars = 4;
    const auto timeSig = appModel->timeSignature();
    const int length = AppGlobal::ticksPerWholeNote * timeSig.numerator / timeSig.denominator * bars;
    singingClip->setName(tr("New Singing Clip"));
    singingClip->setStart(0);
    singingClip->setClipStart(0);
    singingClip->setLength(length);
    singingClip->setClipLen(length);
    const auto newTrack = new Track;
    newTrack->setName(tr("New Track"));
    newTrack->setDefaultLanguage(appOptions->general()->defaultSingingLanguage);
    newTrack->setColorIndex(0);

    newTrack->insertClip(singingClip);
    d->m_tracks.append(newTrack);

    auto vm = new LevelMeterViewModel(this);
    d->m_levelMeterViewModels.append(vm);

    d->m_masterLevelMeterViewModel = new LevelMeterViewModel(this);

    emit modelChanged();
    d->dispose();
}

bool AppModel::loadProject(const QString &path, QString &errorMessage) {
    Q_D(AppModel);
    d->reset();
    DspxProjectConverter converter;
    AppModel resultModel;
    const auto ok = converter.load(path, &resultModel, errorMessage, ImportMode::NewProject);
    if (ok)
        loadFromAppModel(resultModel);
    return ok;
}

bool AppModel::saveProject(const QString &path, QString &errorMessage) {
    Q_D(AppModel);
    DspxProjectConverter converter;
    const auto ok = converter.save(path, this, errorMessage);
    return ok;
}

bool AppModel::importAceProject(const QString &filename) {
    Q_D(AppModel);
    AProjectConverter converter;
    QString errMsg;
    AppModel resultModel;
    const auto ok = converter.load(filename, &resultModel, errMsg, ImportMode::NewProject);
    if (ok) {
        for (const auto track : resultModel.tracks()) {
            track->setDefaultLanguage(appOptions->general()->defaultSingingLanguage);
            for (const auto clip : track->clips()) {
                if (clip->clipType() == Clip::Singing) {
                    const auto singingClip = static_cast<SingingClip *>(clip);
                    singingClip->setDefaultLanguage(track->defaultLanguage());
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
    d->m_masterControl = model.masterControl();
    d->m_tracks = model.tracks();

    for (int i = 0; i < d->m_tracks.size(); i++) {
        auto vm = new LevelMeterViewModel(this);
        d->m_levelMeterViewModels.append(vm);
    }

    d->m_masterLevelMeterViewModel = new LevelMeterViewModel(this);

    emit modelChanged();
    d->dispose();
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

    if (midiImport == -1) {
        errMsg = "User canceled the import.";
        return false;
    }

    AppModel resultModel;
    MidiConverter converter;
    const auto ok = converter.load(filename, &resultModel, errMsg,
                                   static_cast<IProjectConverter::ImportMode>(midiImport));
    Log::i("Midi importer", errMsg);
    if (ok) {
        if (midiImport == ImportMode::NewProject) {
            d->reset();
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
    MidiConverter converter;
    QString errMsg;
    Log::i("Midi exporter", errMsg);
    return converter.save(filename, this, errMsg);
}

Clip *AppModel::findClipById(const int clipId, Track *&trackRef) const {
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

Clip *AppModel::findClipById(const int clipId, int &trackIndex) {
    Q_D(const AppModel);
    int i = 0;
    for (const auto track : d->m_tracks) {
        if (const auto result = track->findClipById(clipId)) {
            trackIndex = i;
            return result;
        }
        i++;
    }
    return nullptr;
}

Clip *AppModel::findClipById(const int clipId) {
    Q_D(const AppModel);
    if (clipId == -1)
        return nullptr;

    for (const auto track : d->m_tracks) {
        if (const auto result = track->findClipById(clipId))
            return result;
    }
    return nullptr;
}

Track *AppModel::findTrackById(const int id, int &trackIndex) {
    Q_D(const AppModel);
    int i = 0;
    for (const auto track : d->m_tracks) {
        if (track->id() == id) {
            trackIndex = i;
            return track;
        }
        i++;
    }
    trackIndex = -1;
    return nullptr;
}

Track *AppModel::findTrackById(const int id) {
    Q_D(const AppModel);
    return MathUtils::findItemById<Track *>(d->m_tracks, id);
}

double AppModel::tickToMs(const double tick) const {
    Q_D(const AppModel);
    return MusicTimeConverter::tickToMs(tick, d->m_tempo);
}

double AppModel::msToTick(const double ms) const {
    Q_D(const AppModel);
    return MusicTimeConverter::msToTick(ms, d->m_tempo);
}

QString AppModel::getBarBeatTickTime(const int ticks) const {
    Q_D(const AppModel);
    return MusicTimeConverter::getBarBeatTickTime(
        ticks, d->m_timeSignature.numerator, d->m_timeSignature.denominator);
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
    m_masterControl = TrackControl();
    m_previousTracks = m_tracks;
    m_tracks.clear();

    for (auto vm : m_levelMeterViewModels)
        delete vm;
    m_levelMeterViewModels.clear();

    delete m_masterLevelMeterViewModel;
    m_masterLevelMeterViewModel = nullptr;
}

void AppModelPrivate::dispose() const {
    qDebug() << "dispose";
    for (int i = 0; i < m_previousTracks.count(); i++) {
        const auto track = m_previousTracks.at(i);
        delete track;
    }
}
