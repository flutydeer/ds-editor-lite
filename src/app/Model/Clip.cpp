//
// Created by fluty on 2024/1/27.
//

#include "Clip.h"

#include <QDebug>

#include "Note.h"
#include "Curve.h"

QString Clip::name() const {
    return m_name;
}
void Clip::setName(const QString &text) {
    m_name = text;
    // emit propertyChanged();
}
int Clip::start() const {
    return m_start;
}
void Clip::setStart(int start) {
    m_start = start;
    // emit propertyChanged();
}
int Clip::length() const {
    return m_length;
}
void Clip::setLength(int length) {
    m_length = length;
    // emit propertyChanged();
}
int Clip::clipStart() const {
    return m_clipStart;
}
void Clip::setClipStart(int clipStart) {
    m_clipStart = clipStart;
    // emit propertyChanged();
}
int Clip::clipLen() const {
    return m_clipLen;
}
void Clip::setClipLen(int clipLen) {
    m_clipLen = clipLen;
    // emit propertyChanged();
}
double Clip::gain() const {
    return m_gain;
}
void Clip::setGain(double gain) {
    m_gain = gain;
    // emit propertyChanged();
}
bool Clip::mute() const {
    return m_mute;
}
void Clip::setMute(bool mute) {
    m_mute = mute;
    // emit propertyChanged();
}
void Clip::notifyPropertyChanged() {
    emit propertyChanged();
}
int Clip::endTick() const {
    return start() + clipStart() + clipLen();
}
int Clip::compareTo(Clip *obj) const {
    auto curVisibleStart = start() + clipStart();
    auto other = obj;
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
    auto other = obj;
    auto otherVisibleStart = other->start() + other->clipStart();
    auto otherVisibleEnd = otherVisibleStart + other->clipLen();
    if (otherVisibleEnd <= curVisibleStart || curVisibleEnd <= otherVisibleStart)
        return false;
    return true;
}
Clip::ClipCommonProperties::ClipCommonProperties(const IClip &clip) {
    applyPropertiesFromClip(*this, clip);
}
void Clip::applyPropertiesFromClip(ClipCommonProperties &args, const IClip &clip) {
    args.name = clip.name();
    args.id = clip.id();
    args.start = clip.start();
    args.clipStart = clip.clipStart();
    args.length = clip.length();
    args.clipLen = clip.clipLen();
    args.gain = clip.gain();
    args.mute = clip.mute();
}
AudioClip::AudioClipProperties::AudioClipProperties(const AudioClip &clip) {
    applyPropertiesFromClip(*this, clip);
    path = clip.path();
}
AudioClip::AudioClipProperties::AudioClipProperties(const IClip &clip) {
    applyPropertiesFromClip(*this, clip);
}
const OverlappableSerialList<Note> &SingingClip::notes() const {
    return m_notes;
}
void SingingClip::insertNote(Note *note) {
    note->setClip(this);
    m_notes.add(note);
}
void SingingClip::removeNote(Note *note) {
    note->setClip(nullptr);
    m_notes.remove(note);
}
void SingingClip::notifyNoteSelectionChanged() {
    emit noteSelectionChanged();
}
void SingingClip::notifyParamChanged(ParamBundle::ParamName name, Param::ParamType type) {
    emit paramChanged(name, type);
}
// QList<SingingClip::VocalPart> SingingClip::parts() {
//     if (m_notes.count() == 0)
//         return m_parts;
//
//     m_parts.clear();
//     QList<Note *> buffer;
//
//     auto commit = [](VocalPart &part, QList<Note *> &buffer) {
//         part.info.selectedNotes.clear();
//         for (const auto note : buffer) {
//             Note newNote;
//             newNote.setStart(note->start());
//             newNote.setLength(note->length());
//             part.info.selectedNotes.append(newNote);
//         }
//         buffer.clear();
//     };
//
//     for (int i = 0; i < m_notes.count(); i++) {
//         auto note = notes().at(i);
//         buffer.append(note);
//         bool commitFlag = i < m_notes.count() - 1 &&
//                           m_notes.at(i + 1)->start() - (note->start() + note->length()) > 0;
//         if (i == m_notes.count() - 1)
//             commitFlag = true;
//         if (commitFlag) {
//             VocalPart part;
//             commit(part, buffer);
//             m_parts.append(part);
//         }
//     }
//     return m_parts;
// }
void SingingClip::copyCurves(const OverlappableSerialList<Curve> &source,
                             OverlappableSerialList<Curve> &target) {
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
void SingingClip::notifyNoteChanged(NoteChangeType type, Note *note) {
    emit noteChanged(type, note);
}
// const DsParams &DsSingingClip::params() const {
//     return m_params;
// }