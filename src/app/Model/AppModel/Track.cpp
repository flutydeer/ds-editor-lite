//
// Created by FlutyDeer on 2023/12/1.
//

#include <QDebug>

#include "Track.h"
#include "Clip.h"
#include "Utils/MathUtils.h"

#include <QJsonArray>

Track::~Track() {
    qDebug() << "Track::~Track()";
    for (auto clip : m_clips)
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

void Track::insertClip(Clip *clip) {
    m_clips.add(clip);
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

AppGlobal::LanguageType Track::defaultLanguage() const {
    return m_defaultLanguage;
}

void Track::setDefaultLanguage(AppGlobal::LanguageType language) {
    m_defaultLanguage = language;
}

void Track::notifyClipChanged(ClipChangeType type, Clip *clip) {
    emit clipChanged(type, clip);
}

Clip *Track::findClipById(int id) const {
    return MathUtils::findItemById<Clip *>(m_clips, id);
}

Track::TrackProperties::TrackProperties(const ITrack &track) {
    auto control = track.control();
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