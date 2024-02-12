//
// Created by fluty on 2024/1/27.
//

#include "TrackControl.h"

double TrackControl::gain() const {
    return m_gain;
}
void TrackControl::setGain(double gain) {
    m_gain = gain;
}
double TrackControl::pan() const {
    return m_pan;
}
void TrackControl::setPan(double pan) {
    m_pan = pan;
}
bool TrackControl::mute() const {
    return m_mute;
}
void TrackControl::setMute(bool mute) {
    m_mute = mute;
}
bool TrackControl::solo() const {
    return m_solo;
}
void TrackControl::setSolo(bool solo) {
    m_solo = solo;
}