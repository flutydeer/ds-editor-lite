//
// Created by FlutyDeer on 2023/12/1.
//

#include <QDebug>

#include "Track.h"
#include "Clip.h"

Track::~Track() {
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
OverlapableSerialList<Clip> Track::clips() const {
    return m_clips;
}
void Track::insertClip(Clip *clip) {
    m_clips.add(clip);
    qDebug() << "DsTrack emit clipChanged Inserted" << clip->id();
    emit clipChanged(Inserted, clip->id(), clip);
}
void Track::removeClip(Clip *clip) {
    m_clips.remove(clip);
    qDebug() << "DsTrack emit clipChanged Removed" << clip->id();
    emit clipChanged(Removed, clip->id(), clip);
}
QColor Track::color() const {
    return m_color;
}
void Track::setColor(const QColor &color) {
    m_color = color;
    emit propertyChanged();
}
void Track::removeClipQuietly(Clip *clip) {
    m_clips.remove(clip);
}
void Track::insertClipQuietly(Clip *clip) {
    m_clips.add(clip);
}

Clip *Track::findClipById(int id) {
    for (int i = 0; i < m_clips.count(); i++) {
        auto clip = m_clips.at(i);
        if (clip->id() == id)
            return clip;
    }
    return nullptr;
}