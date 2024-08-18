//
// Created by fluty on 2024/2/8.
//

#include "EditTrackPropertiesAction.h"

EditTrackPropertiesAction *EditTrackPropertiesAction::build(const Track::TrackProperties &oldArgs,
                                                            const Track::TrackProperties &newArgs,
                                                            Track *track) {
    auto a = new EditTrackPropertiesAction;
    a->m_oldArgs = oldArgs;
    a->m_newArgs = newArgs;
    a->m_track = track;
    return a;
}

void EditTrackPropertiesAction::execute() {
    m_track->setName(m_newArgs.name);
    auto control = m_track->control();
    control.setGain(m_newArgs.gain);
    control.setPan(m_newArgs.pan);
    control.setMute(m_newArgs.mute);
    control.setSolo(m_newArgs.solo);
    m_track->setControl(control);
}

void EditTrackPropertiesAction::undo() {
    m_track->setName(m_oldArgs.name);
    auto control = m_track->control();
    control.setGain(m_oldArgs.gain);
    control.setPan(m_oldArgs.pan);
    control.setMute(m_oldArgs.mute);
    control.setSolo(m_oldArgs.solo);
    m_track->setControl(control);
}