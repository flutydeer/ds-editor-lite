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

static void setTrackSingerIdentifierForClip(Clip *clip, const SingerIdentifier &identifier) {
    if (!clip) {
        return;
    }
    if (clip->clipType() == IClip::Singing) {
        // NOLINTNEXTLINE(*-pro-type-static-cast-downcast)
        const auto singingClip = static_cast<SingingClip *>(clip);
        singingClip->trackSingerIdentifier = identifier;
    }
}

static void setTrackSpeakerForClip(Clip *clip, const QString &speaker) {
    if (!clip) {
        return;
    }
    if (clip->clipType() == IClip::Singing) {
        // NOLINTNEXTLINE(*-pro-type-static-cast-downcast)
        const auto singingClip = static_cast<SingingClip *>(clip);
        singingClip->trackSpeaker = speaker;
    }
}

void Track::insertClip(Clip *clip) {
    setTrackSingerIdentifierForClip(clip, m_identifier);
    setTrackSpeakerForClip(clip, m_speaker);
    m_clips.add(clip);
    connect(this, &Track::singerIdentifierChanged, clip, [clip, this]() {
        setTrackSingerIdentifierForClip(clip, m_identifier);
    });
    connect(this, &Track::speakerChanged, clip, [clip, this]() {
        setTrackSpeakerForClip(clip, m_speaker);
    });
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
}

QString Track::defaultG2pId() const {
    return m_defaultG2pId;
}

void Track::setDefaultG2pId(const QString &g2pId) {
    // TODO: validate
    // if (!AppGlobal::languageNames.contains(g2pId))
    //     qFatal() << "Track::G2p Name Error";
    m_defaultG2pId = g2pId;
}

const SingerIdentifier &Track::singerIdentifier() const {
    return m_identifier;
}

void Track::setSingerIdentifier(SingerIdentifier identifier) {
    if (m_identifier == identifier) {
        return;
    }
    m_identifier = std::move(identifier);
    emit singerIdentifierChanged(m_identifier);
}

QString Track::speaker() const {
    return m_speaker;
}

void Track::setSpeaker(const QString &speaker) {
    if (m_speaker == speaker) {
        return;
    }
    m_speaker = speaker;
    emit speakerChanged(m_speaker);
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