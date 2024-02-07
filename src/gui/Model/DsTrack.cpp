//
// Created by FlutyDeer on 2023/12/1.
//

#include <QDebug>

#include "DsTrack.h"

QString DsTrack::name() const {
    return m_name;
}
void DsTrack::setName(const QString &name) {
    m_name = name;
    emit propertyChanged();
}
DsTrackControl DsTrack::control() const {
    return m_control;
}
void DsTrack::setControl(const DsTrackControl &control) {
    m_control = control;
    emit propertyChanged();
}
OverlapableSerialList<DsClip> DsTrack::clips() const {
    return m_clips;
}
void DsTrack::insertClip(DsClip *clip) {
    m_clips.add(clip);
    emit clipChanged(Inserted, clip->id(), clip);
}
void DsTrack::removeClip(DsClip *clip) {
    m_clips.remove(clip);
    emit clipChanged(Removed, clip->id(), clip);
}
QColor DsTrack::color() const {
    return m_color;
}
void DsTrack::setColor(const QColor &color) {
    m_color = color;
    emit propertyChanged();
}
void DsTrack::removeClipQuietly(DsClip *clip) {
    m_clips.remove(clip);
}
void DsTrack::insertClipQuietly(DsClip *clip) {
    m_clips.add(clip);
}
void DsTrack::notityClipPropertyChanged(DsClip *clip) {
    emit clipChanged(PropertyChanged, clip->id(), clip);
}

DsClip *DsTrack::findClipById(int id) {
    for (int i = 0; i < m_clips.count(); i++) {
        auto clip = m_clips.at(i);
        if (clip->id() == id)
            return clip;
    }
    return nullptr;
}