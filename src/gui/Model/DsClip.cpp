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
int DsClip::compareTo(DsClip *obj) const {
    auto curVisibleStart = start() + clipStart();
    auto other = dynamic_cast<DsClip *>(obj);
    auto otherVisibleStart = other->start() + other->clipStart();
    if (curVisibleStart < otherVisibleStart)
        return -1;
    if (curVisibleStart > otherVisibleStart)
        return 1;
    return 0;
}
bool DsClip::isOverlappedWith(DsClip *obj) const {
    auto curVisibleStart = start() + clipStart();
    auto curVisibleEnd = curVisibleStart + clipLen();
    auto other = dynamic_cast<DsClip *>(obj);
    auto otherVisibleStart = other->start() + other->clipStart();
    auto otherVisibleEnd = otherVisibleStart + other->clipLen();
    if (otherVisibleEnd <= curVisibleStart || curVisibleEnd <= otherVisibleStart)
        return false;
    return true;
}
const OverlapableSerialList<DsNote> &DsSingingClip::notes() const {
    return m_notes;
}
void DsSingingClip::insertNote(DsNote *note) {
    m_notes.add(note);
    emit noteChanged(Inserted, note->id(), note);
}
void DsSingingClip::removeNote(DsNote *note) {
    m_notes.remove(note);
    emit noteChanged(Removed, note->id(), nullptr);
}
void DsSingingClip::insertNoteQuietly(DsNote *note) {
    m_notes.add(note);
}
void DsSingingClip::removeNoteQuietly(DsNote *note) {
    m_notes.remove(note);
}
void DsSingingClip::notifyNotePropertyChanged(DsNote *note) {
    emit noteChanged(PropertyChanged, note->id(), note);
}
DsNote *DsSingingClip::findNoteById(int id) {
    for (int i = 0; i < m_notes.count(); i++) {
        auto note = m_notes.at(i);
        if (note->id() == id)
            return note;
    }
    return nullptr;
}
// const DsParams &DsSingingClip::params() const {
//     return m_params;
// }