//
// Created by FlutyDeer on 2023/12/1.
//

#include <QDebug>

#include "Track.h"
#include "Clip.h"
#include "SingingClip.h"
#include "Utils/MathUtils.h"

#include <QJsonArray>

Track::Track(const QString &name, const QList<Clip *> &clips) {
    setName(name);
    insertClips(clips);
}

Track::~Track() {
    qDebug() << "Track::~Track()";
    for (const auto clip : m_clips)
        delete clip;
}

QString Track::name() const {
    return m_name;
}

void Track::setName(const QString &name) {
    m_name = name;
    emit propertyChanged();
}

TrackControl Track::control() const {
    return m_control;
}

void Track::setControl(const TrackControl &control) {
    m_control = control;
    emit propertyChanged();
}

OverlappableSerialList<Clip> Track::clips() const {
    return m_clips;
}

static void setTrackSingerForClip(Clip *clip, const SingerInfo &singerInfo) {
    if (!clip) {
        return;
    }
    if (clip->clipType() == IClip::Singing) {
        // NOLINTNEXTLINE(*-pro-type-static-cast-downcast)
        const auto singingClip = static_cast<SingingClip *>(clip);
        singingClip->setTrackSingerInfo(singerInfo);
    }
}

static void setTrackSpeakerForClip(Clip *clip, const SpeakerInfo &speakerInfo) {
    if (!clip) {
        return;
    }
    if (clip->clipType() == IClip::Singing) {
        // NOLINTNEXTLINE(*-pro-type-static-cast-downcast)
        const auto singingClip = static_cast<SingingClip *>(clip);
        singingClip->setTrackSpeakerInfo(speakerInfo);
    }
}

void Track::insertClip(Clip *clip) {
    setTrackSingerForClip(clip, m_singerInfo);
    setTrackSpeakerForClip(clip, m_speakerInfo);
    m_clips.add(clip);
    connect(this, &Track::singerChanged, clip,
            [clip, this] { setTrackSingerForClip(clip, m_singerInfo); });
    connect(this, &Track::speakerChanged, clip,
            [clip, this] { setTrackSpeakerForClip(clip, m_speakerInfo); });
}

void Track::insertClips(const QList<Clip *> &clips) {
    for (const auto clip : std::as_const(clips)) {
        m_clips.add(clip);
    }
}

void Track::removeClip(Clip *clip) {
    m_clips.remove(clip);
}

QColor Track::color() const {
    return m_color;
}

void Track::setColor(const QColor &color) {
    m_color = color;
    emit propertyChanged();
}

QString Track::defaultLanguage() const {
    return m_defaultLanguage;
}

void Track::setDefaultLanguage(const QString &language) {
    // TODO: validate
    // if (!AppGlobal::languageNames.contains(language))
    //     qFatal() << "Track::Language Name Error";
    m_defaultLanguage = language;
    this->updateDefaultG2pId(language);
}

QString Track::defaultG2pId() const {
    return m_singerInfo.g2pId(m_defaultLanguage);
}

SingerIdentifier Track::singerIdentifier() const {
    return m_singerInfo.identifier();
}

SingerInfo Track::singerInfo() const {
    return m_singerInfo;
}

void Track::setSingerInfo(const SingerInfo &singerInfo) {
    if (m_singerInfo == singerInfo) {
        return;
    }
    m_singerInfo = singerInfo;
    this->updateDefaultG2pId(m_defaultLanguage);
    emit singerChanged(m_singerInfo);
}

QString Track::speakerId() const {
    return speakerInfo().id();
}

SpeakerInfo Track::speakerInfo() const {
    return m_speakerInfo;
}

void Track::setSpeakerInfo(const SpeakerInfo &speakerInfo) {
    if (m_speakerInfo == speakerInfo) {
        return;
    }
    m_speakerInfo = speakerInfo;
    emit speakerChanged(m_speakerInfo);
}

void Track::notifyClipChanged(const ClipChangeType type, Clip *clip) {
    emit clipChanged(type, clip);
}

Clip *Track::findClipById(const int id) const {
    return MathUtils::findItemById<Clip *>(m_clips, id);
}

Track::TrackProperties::TrackProperties(const ITrack &track) {
    const auto control = track.control();
    id = track.id();
    name = track.name();
    gain = control.gain();
    pan = control.pan();
    mute = control.mute();
    solo = control.solo();
}

QJsonObject Track::serialize() const {
    QJsonArray arrClips;
    return QJsonObject{
        {"name",      m_name               },
        {"control",   m_control.serialize()},
        {"clips",     arrClips             },
        {"extra",     QJsonObject()        },
        {"workspace", QJsonObject()        }
    };
}

bool Track::deserialize(const QJsonObject &obj) {
    return true;
}

void Track::updateDefaultG2pId(const QString &language) {
    const auto languageInfos = this->singerInfo().languages();
    for (const auto &languageInfo : languageInfos) {
        if (language == languageInfo.id()) {
            m_defaultG2pId = languageInfo.g2p();
            break;
        }
    }
}