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
    qDebug() << "model name " << name;
}
DsTrackControl DsTrack::control() const {
    return m_control;
}
void DsTrack::setControl(const DsTrackControl &control) {
    m_control = control;
}
OverlapableSerialList<DsClip> DsTrack::clips() const {
    return m_clips;
}
void DsTrack::insertClip(DsClip *clip) {
    m_clips.add(clip);
    int index = m_clips.indexOf(clip);
    emit clipChanged(Insert, index);
}
void DsTrack::removeClip(DsClip *clip) {
    int index = m_clips.indexOf(clip);
    m_clips.remove(clip);
    emit clipChanged(Remove, index);
}