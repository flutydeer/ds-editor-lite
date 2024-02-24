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
    qDebug() << "AppModel SingingClip::insertNote" << note->start() << note->length()
             << note->lyric();
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
void SingingClip::notifyParamChanged(ParamBundle::ParamName paramName, Param::ParamType paramType) {
    emit paramChanged(paramName, paramType);
}
QList<SingingClip::VocalPart> SingingClip::parts() {
    if (m_notes.count() == 0)
        return m_parts;

    m_parts.clear();
    QList<Note *> buffer;

    auto commit = [](VocalPart &part, QList<Note *> &buffer) {
        part.info.selectedNotes.clear();
        for (const auto note : buffer) {
            Note newNote;
            newNote.setStart(note->start());
            newNote.setLength(note->length());
            part.info.selectedNotes.append(newNote);
        }
        buffer.clear();
    };

    for (int i = 0; i < m_notes.count(); i++) {
        auto note = notes().at(i);
        buffer.append(note);
        bool commitFlag = i < m_notes.count() - 1 &&
                          m_notes.at(i + 1)->start() - (note->start() + note->length()) > 0;
        if (i == m_notes.count() - 1)
            commitFlag = true;
        if (commitFlag) {
            VocalPart part;
            commit(part, buffer);
            m_parts.append(part);
        }
    }
    return m_parts;
}
void SingingClip::copyCurves(const OverlapableSerialList<Curve> &source,
                             OverlapableSerialList<Curve> &target) {
    target.clear();
    for (const auto curve : source) {
        if (curve->type() == Curve::Draw)
            target.add(new DrawCurve(*dynamic_cast<DrawCurve *>(curve)));
        // TODO: copy anchor curve
        // else if (curve->type() == Curve::Anchor)
        //     target.append(new AnchorCurve)
    }
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
    return notes;
}
// const DsParams &DsSingingClip::params() const {
//     return m_params;
// }