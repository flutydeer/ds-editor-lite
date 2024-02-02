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
    // qDebug() << "model name " << name;
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
    // connect(clip, &DsClip::propertyChanged, this, [=] {
    //     auto clipIndex = m_clips.indexOf(clip);
    //     emit clipChanged(Update, clipIndex);
    // });
    m_clips.add(clip);
    int index = m_clips.indexOf(clip);
    emit clipChanged(Insert, index);
}
void DsTrack::removeClip(DsClip *clip) {
    int index = m_clips.indexOf(clip);
    m_clips.remove(clip);
    emit clipChanged(Remove, index);
}