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

static void setTrackVoiceContextForClip(Clip *clip, const SingerInfo &singerInfo,
                                        const SpeakerInfo &speakerInfo,
                                        const SpeakerMixModel::SpeakerMixData &data) {
    if (!clip) {
        return;
    }
    if (clip->clipType() == IClip::Singing) {
        const auto singingClip = static_cast<SingingClip *>(clip);
        singingClip->setTrackVoiceContext(singerInfo, speakerInfo, data);
    }
}

void Track::insertClip(Clip *clip) {
    m_clips.add(clip);
    setTrackVoiceContextForClip(clip, m_singerInfo, m_speakerInfo, m_speakerMixData);
    connect(this, &Track::singerOrSpeakerChanged, clip,
            [clip, this] {
                setTrackVoiceContextForClip(clip, m_singerInfo, m_speakerInfo, m_speakerMixData);
            });
    connect(this, &Track::speakerMixChanged, clip,
            [clip, this](const SpeakerMixModel::SpeakerMixData &) {
                setTrackVoiceContextForClip(clip, m_singerInfo, m_speakerInfo, m_speakerMixData);
            });
}

void Track::insertClips(const QList<Clip *> &clips) {
    for (const auto clip : std::as_const(clips)) {
        insertClip(clip);
    }
}

void Track::removeClip(Clip *clip) {
    disconnect(this, &Track::singerOrSpeakerChanged, clip, nullptr);
    disconnect(this, &Track::speakerMixChanged, clip, nullptr);
    m_clips.remove(clip);
}

int Track::colorIndex() const {
    return m_colorIndex;
}

void Track::setColorIndex(int colorIndex) {
    m_colorIndex = colorIndex;
    emit propertyChanged();
}

QString Track::defaultLanguage() const {
    return m_defaultLanguage;
}

void Track::setDefaultLanguage(const QString &language) {
    // TODO: validate
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

QString Track::speakerId() const {
    return speakerInfo().id();
}

SpeakerInfo Track::speakerInfo() const {
    return m_speakerInfo;
}

SpeakerMixModel::SpeakerMixData Track::speakerMixData() const {
    return m_speakerMixData;
}

EffectiveVoiceContext Track::voiceContext() const {
    return {m_singerInfo, m_speakerInfo, m_speakerMixData, false};
}

void Track::setVoiceContext(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                            const SpeakerMixModel::SpeakerMixData &speakerMixData) {
    const auto oldContext = voiceContext();
    const auto normalizedSpeakerMixData = SpeakerMixModel::normalizeSpeakerMixData(speakerMixData);
    if (oldContext.singer == singerInfo && oldContext.speaker == speakerInfo &&
        oldContext.speakerMix == normalizedSpeakerMixData)
        return;

    m_singerInfo = singerInfo;
    m_speakerInfo = speakerInfo;
    m_speakerMixData = normalizedSpeakerMixData;
    this->updateDefaultG2pId(m_defaultLanguage);

    const auto newContext = voiceContext();
    if (oldContext.speakerMix != newContext.speakerMix)
        emit speakerMixChanged(m_speakerMixData);
    if (oldContext.singer != newContext.singer || oldContext.speaker != newContext.speaker)
        emit singerOrSpeakerChanged();
}

void Track::selectSingleSpeaker(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo) {
    setVoiceContext(singerInfo, speakerInfo, {});
}

void Track::setSingerAndSpeakerInfo(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo) {
    selectSingleSpeaker(singerInfo, speakerInfo);
}

void Track::setSpeakerMixData(const SpeakerMixModel::SpeakerMixData &data) {
    setVoiceContext(m_singerInfo, m_speakerInfo, data);
}

void Track::resetSpeakerMixToSingle() {
    setSpeakerMixData({});
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
