//
// Created by fluty on 2024/1/27.
//

#include <QDebug>

#include "Clip.h"

QString Clip::name() const {
    return m_name;
}
void Clip::setName(const QString &text) {
    m_name = text;
}
int Clip::start() const {
    return m_start;
}
void Clip::setStart(int start) {
    m_start = start;
}
int Clip::length() const {
    return m_length;
}
void Clip::setLength(int length) {
    m_length = length;
}
int Clip::clipStart() const {
    return m_clipStart;
}
void Clip::setClipStart(int clipStart) {
    m_clipStart = clipStart;
}
int Clip::clipLen() const {
    return m_clipLen;
}
void Clip::setClipLen(int clipLen) {
    m_clipLen = clipLen;
}
double Clip::gain() const {
    return m_gain;
}
void Clip::setGain(double gain) {
    m_gain = gain;
}
bool Clip::mute() const {
    return m_mute;
}
void Clip::setMute(bool mute) {
    m_mute = mute;
}
int Clip::compareTo(Clip *obj) const {
    auto curVisibleStart = start() + clipStart();
    auto other = dynamic_cast<Clip *>(obj);
    auto otherVisibleStart = other->start() + other->clipStart();
    if (curVisibleStart < otherVisibleStart)
        return -1;
    if (curVisibleStart > otherVisibleStart)
        return 1;
    return 0;
}
bool Clip::isOverlappedWith(Clip *obj) const {
    auto curVisibleStart = start() + clipStart();
    auto curVisibleEnd = curVisibleStart + clipLen();
    auto other = dynamic_cast<Clip *>(obj);
    auto otherVisibleStart = other->start() + other->clipStart();
    auto otherVisibleEnd = otherVisibleStart + other->clipLen();
    if (otherVisibleEnd <= curVisibleStart || curVisibleEnd <= otherVisibleStart)
        return false;
    return true;
}
const OverlapableSerialList<Note> &SingingClip::notes() const {
    return m_notes;
}
void SingingClip::insertNote(Note *note) {
    qDebug() << "AppModel SingingClip::insertNote" << note->start() << note->length() << note->lyric();
    m_notes.add(note);
    emit noteListChanged(Inserted, note->id(), note);
}
void SingingClip::removeNote(Note *note) {
    m_notes.remove(note);
    emit noteListChanged(Removed, note->id(), nullptr);
}
void SingingClip::insertNoteQuietly(Note *note) {
    m_notes.add(note);
}
void SingingClip::removeNoteQuietly(Note *note) {
    m_notes.remove(note);
}
void SingingClip::notifyNoteSelectionChanged() {
    emit noteSelectionChanged();
}
void SingingClip::notifyNotePropertyChanged(NotePropertyType type, Note *note) {
    emit notePropertyChanged(type, note);
}
Note *SingingClip::findNoteById(int id) {
    for (int i = 0; i < m_notes.count(); i++) {
        auto note = m_notes.at(i);
        if (note->id() == id)
            return note;
    }
    return nullptr;
}
QList<Note *> SingingClip::selectedNotes() const {
    QList<Note *> notes;
    for (int i = 0; i < m_notes.count(); i++) {
        auto note = m_notes.at(i);
        if (note->selected())
            notes.append(note);
    }
    return  notes;
}
// const DsParams &DsSingingClip::params() const {
//     return m_params;
// }