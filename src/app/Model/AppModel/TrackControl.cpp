//
// Created by fluty on 2024/1/27.
//

#include "TrackControl.h"

double TrackControl::gain() const {
    return m_gain;
}

void TrackControl::setGain(const double gain) {
    m_gain = gain;
}

double TrackControl::pan() const {
    return m_pan;
}

void TrackControl::setPan(const double pan) {
    m_pan = pan;
}

bool TrackControl::mute() const {
    return m_mute;
}

void TrackControl::setMute(const bool mute) {
    m_mute = mute;
}

bool TrackControl::solo() const {
    return m_solo;
}

void TrackControl::setSolo(const bool solo) {
    m_solo = solo;
}

QJsonObject TrackControl::serialize() const {
    return QJsonObject{
        {"gain", m_gain},
        {"pan",  m_pan },
        {"mute", m_mute},
        {"solo", m_solo}
    };
}

bool TrackControl::deserialize(const QJsonObject &obj) {
    m_gain = obj["gain"].toDouble();
    m_pan = obj["pan"].toDouble();
    m_mute = obj["mute"].toBool();
    m_solo = obj["solo"].toBool();
    return true;
}