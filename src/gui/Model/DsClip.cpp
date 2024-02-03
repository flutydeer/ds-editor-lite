//
// Created by fluty on 2024/1/27.
//

#include "DsClip.h"

QString DsClip::name() const {
    return m_name;
}
void DsClip::setName(const QString &text) {
    m_name = text;
}
int DsClip::start() const {
    return m_start;
}
void DsClip::setStart(int start) {
    m_start = start;
}
int DsClip::length() const {
    return m_length;
}
void DsClip::setLength(int length) {
    m_length = length;
}
int DsClip::clipStart() const {
    return m_clipStart;
}
void DsClip::setClipStart(int clipStart) {
    m_clipStart = clipStart;
}
int DsClip::clipLen() const {
    return m_clipLen;
}
void DsClip::setClipLen(int clipLen) {
    m_clipLen = clipLen;
}
double DsClip::gain() const {
    return m_gain;
}
void DsClip::setGain(double gain) {
    m_gain = gain;
}
bool DsClip::mute() const {
    return m_mute;
}
void DsClip::setMute(bool mute) {
    m_mute = mute;
}
int DsClip::compareTo(IOverlapable *obj) {
    auto curVisibleStart = start() + clipStart();
    auto other = dynamic_cast<DsClip *>(obj);
    auto otherVisibleStart = other->start() + other->clipStart();
    if (curVisibleStart < otherVisibleStart)
        return -1;
    if (curVisibleStart > otherVisibleStart)
        return 1;
    return 0;
}
bool DsClip::isOverlappedWith(IOverlapable *obj) {
    auto curVisibleStart = start() + clipStart();
    auto curVisibleEnd = curVisibleStart + clipLen();
    auto other = dynamic_cast<DsClip *>(obj);
    auto otherVisibleStart = other->start() + other->clipStart();
    auto otherVisibleEnd = otherVisibleStart + other->clipLen();
    if (otherVisibleEnd <= curVisibleStart || curVisibleEnd <= otherVisibleStart)
        return false;
    return true;
}