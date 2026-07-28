//
// Created by fluty on 2024/1/27.
//

#include "AppModel.h"

#include "AppModel_p.h"
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/Support/MathUtils.h>
#include <lite/MusicBase/MusicTime.h>

#include <QJsonArray>

#include <algorithm>

namespace {
    bool intersectsTempoChange(const QList<TempoChangeRange> &ranges, const Clip &clip) {
        const int start = clip.start() + clip.clipStart();
        const int end = start + clip.clipLen();
        return std::any_of(
            ranges.cbegin(), ranges.cend(),
            [start, end](const TempoChangeRange &range) { return range.intersects(start, end); });
    }
}

AppModel::AppModel(QObject *parent) : QObject(parent), d_ptr(new AppModelPrivate(this)) {
}

AppModel::~AppModel() {
    Q_D(AppModel);
    d->reset();
    d->dispose();
    delete d_ptr;
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(AppModel)

const Timeline &AppModel::timeline() const {
    Q_D(const AppModel);
    return d->m_timeline;
}

void AppModel::setTimeline(Timeline timeline) {
    Q_D(AppModel);
    const bool temposChanged = d->m_timeline.tempos() != timeline.tempos();
    const auto tempoRanges = temposChanged ? Timeline::tempoChangeRanges(d->m_timeline, timeline)
                                           : QList<TempoChangeRange>{};
    d->m_timeline = std::move(timeline);
    if (!tempoRanges.isEmpty()) {
        // Audio clips are anchored in real time; their tick caches follow the map
        d->updateAudioClipTickCaches();
        for (const auto track : d->m_tracks) {
            for (const auto clip : track->clips()) {
                if (clip->clipType() == IClip::Singing && intersectsTempoChange(tempoRanges, *clip))
                    static_cast<SingingClip *>(clip)->bumpInferenceRevision();
            }
        }
    }
    emit timelineChanged();
}

void AppModel::setTimeSignature(const TimeSignature &signature) {
    Q_D(AppModel);
    d->m_timeline.addTimeSignature(TimeSignature(0, signature.numerator, signature.denominator));
    emit timelineChanged();
}

void AppModel::setTempo(const double tempo) {
    auto newTimeline = timeline();
    newTimeline.addTempo({0, tempo});
    setTimeline(std::move(newTimeline));
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

void AppModel::setDefaultSingingLanguage(const QString &language) {
    Q_D(AppModel);
    d->m_defaultSingingLanguage = language;
}

void AppModel::setPaletteColorCount(const int count) {
    Q_D(AppModel);
    if (count > 0)
        d->m_paletteColorCount = count;
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
        const int newIdx = (prev < 0) ? 0 : (prev + 1) % d->m_paletteColorCount;
        track->setColorIndex(newIdx);
    }
    d->m_tracks.insert(index, track);

    emit trackChanged(Insert, index, track);
}

void AppModel::appendTrack(Track *track) {
    Q_D(AppModel);
    insertTrack(track, d->m_tracks.count());
}

void AppModel::moveTrack(const qsizetype from, const qsizetype to) {
    Q_D(AppModel);
    if (from == to)
        return;
    const auto size = d->m_tracks.size();
    if (from < 0 || from >= size || to < 0 || to >= size)
        return;

    const auto track = d->m_tracks.at(from);
    d->m_tracks.move(from, to);

    // 先重排、后通知：发信号时模型已处于一致状态，监听者据此判定这是移动而非删除
    emit trackChanged(Remove, from, track);
    emit trackChanged(Insert, to, track);
}

void AppModel::removeTrackAt(const qsizetype index) {
    takeTrackAt(index);
}

void AppModel::removeTrack(Track *track) {
    takeTrack(track);
}

Track *AppModel::takeTrackAt(const qsizetype index) {
    Q_D(AppModel);
    if (index < 0 || index >= d->m_tracks.count())
        return nullptr;
    const auto track = d->m_tracks.takeAt(index);
    emit trackChanged(Remove, index, track);
    return track;
}

Track *AppModel::takeTrack(Track *track) {
    Q_D(AppModel);
    return takeTrackAt(d->m_tracks.indexOf(track));
}

void AppModel::clearTracks() {
    Q_D(AppModel);
    while (d->m_tracks.count() > 0)
        delete takeTrackAt(0);
}

ProjectModelData AppModel::takeProjectData() {
    Q_D(AppModel);
    ProjectModelData data;
    data.timeline = d->m_timeline;
    data.masterControl = d->m_masterControl;
    data.tracks.reserve(static_cast<size_t>(d->m_tracks.size()));
    for (const auto track : std::as_const(d->m_tracks))
        data.tracks.emplace_back(track);
    d->m_tracks.clear();
    return data;
}

void AppModel::replaceProject(ProjectModelData &&data) {
    Q_D(AppModel);
    d->reset();
    d->m_timeline = std::move(data.timeline);
    d->m_masterControl = data.masterControl;
    d->m_tracks.reserve(static_cast<qsizetype>(data.tracks.size()));
    for (auto &track : data.tracks)
        d->m_tracks.append(track.release());

    const auto defaultLanguage = d->m_defaultSingingLanguage;
    for (const auto track : std::as_const(d->m_tracks)) {
        if (track->defaultLanguage().isEmpty() || track->defaultLanguage() == "unknown")
            track->setDefaultLanguage(defaultLanguage);
        for (const auto clip : track->clips()) {
            if (clip->clipType() == IClip::Audio) {
                // dspx stores ticks only; establish the realtime truth under
                // the timeline the ticks were saved with
                static_cast<AudioClip *>(clip)->syncTruthFromTicks(d->m_timeline);
                continue;
            }
            if (clip->clipType() != IClip::Singing)
                continue;
            const auto singingClip = static_cast<SingingClip *>(clip);
            if (singingClip->defaultLanguage().isEmpty() ||
                singingClip->defaultLanguage() == "unknown")
                singingClip->setDefaultLanguage(track->defaultLanguage());
        }
    }

    emit modelChanged();
    d->dispose();
}

void AppModel::newProject() {
    Q_D(AppModel);
    d->reset();

    const auto singingClip = new SingingClip;
    constexpr int bars = 4;
    const int length = d->m_timeline.barToTick(bars);
    singingClip->setName(tr("New Singing Clip"));
    singingClip->setStart(0);
    singingClip->setClipStart(0);
    singingClip->setLength(length);
    singingClip->setClipLen(length);
    const auto newTrack = new Track;
    newTrack->setName(tr("New Track"));
    newTrack->setDefaultLanguage(d->m_defaultSingingLanguage);
    newTrack->setColorIndex(0);

    newTrack->insertClip(singingClip);
    d->m_tracks.append(newTrack);

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

    QJsonArray arrTempos;
    for (const auto &tempo : d->m_timeline.tempos()) {
        arrTempos.append(QJsonObject{
            {"pos",   tempo.pos  },
            {"value", tempo.value}
        });
    }

    QJsonArray arrTimeSignatures;
    for (const auto &signature : d->m_timeline.timeSignatures())
        arrTimeSignatures.append(signature.serialize());

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
    return d->m_timeline.tickToMs(tick);
}

double AppModel::msToTick(const double ms) const {
    Q_D(const AppModel);
    return d->m_timeline.msToTick(ms);
}

QString AppModel::getBarBeatTickTime(const int ticks) const {
    Q_D(const AppModel);
    return d->m_timeline.getBarBeatTickTime(ticks);
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
    m_timeline = Timeline();
    m_masterControl = TrackControl();
    m_previousTracks = m_tracks;
    m_tracks.clear();
}

void AppModelPrivate::updateAudioClipTickCaches() const {
    for (const auto track : m_tracks) {
        for (const auto clip : track->clips()) {
            if (clip->clipType() != IClip::Audio)
                continue;
            const auto audioClip = static_cast<AudioClip *>(clip);
            if (!audioClip->hasRealTimeAnchor()) {
                // Clips inserted through paths that predate the realtime
                // anchor: adopt the current ticks as truth
                audioClip->syncTruthFromTicks(m_timeline);
                continue;
            }
            // Reindex around the mutation: the overlap list keys on the
            // interval captured at insertion time
            track->removeClip(audioClip);
            const bool changed = audioClip->updateTicksFromTruth(m_timeline);
            track->insertClip(audioClip);
            if (changed)
                audioClip->notifyPropertyChanged();
        }
    }
}

void AppModelPrivate::dispose() {
    qDebug() << "dispose";
    for (int i = 0; i < m_previousTracks.count(); i++) {
        const auto track = m_previousTracks.at(i);
        delete track;
    }
    m_previousTracks.clear();
}
