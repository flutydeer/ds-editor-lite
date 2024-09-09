//
// Created by fluty on 2024/1/27.
//

#include "Clip.h"

#include <QDebug>

#include "Note.h"
#include "Curve.h"
#include "DrawCurve.h"
#include "Utils/Linq.h"
#include "Utils/MathUtils.h"

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

int Clip::compareTo(const Clip *obj) const {
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

std::tuple<qsizetype, qsizetype> Clip::interval() const {
    auto visibleStart = start() + clipStart();
    auto visibleEnd = visibleStart + clipLen();
    return std::make_tuple(visibleStart, visibleEnd);
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

AudioClip::~AudioClip() {
    qDebug() << "~AudioClip()" << id() << m_name;
}

SingingClip::SingingClip() : Clip() {
    defaultLanguage.setNotifyCallback(
        [=](const AppGlobal::LanguageType value) { emit defaultLanguageChanged(value); });
}

SingingClip::~SingingClip() {
    qDebug() << "~SingingClip() " << id() << m_name;
    auto notes = m_notes.toList();
    for (int i = 0; i < notes.count(); i++) {
        delete notes[i];
    }
}

const OverlappableSerialList<Note> &SingingClip::notes() const {
    return m_notes;
}

void SingingClip::insertNote(Note *note) {
    note->setClip(this);
    m_notes.add(note);
    qDebug() << "insert note #" << note->id() << note->lyric() << "clip #" << id();
}

void SingingClip::removeNote(Note *note) {
    note->setClip(nullptr);
    m_notes.remove(note);
    qDebug() << "remove note #" << note->id() << note->lyric() << "clip #" << id();
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
void SingingClip::copyCurves(const QList<Curve *> &source, QList<Curve *> &target) {
    target.clear();
    for (const auto curve : source) {
        if (curve->type() == Curve::Draw)
            target.append(new DrawCurve(*dynamic_cast<DrawCurve *>(curve)));
        // TODO: copy anchor curve
        // else if (curve->type() == Curve::Anchor)
        //     target.append(new AnchorCurve)
    }
}

void SingingClip::copyCurves(const QList<DrawCurve *> &source, QList<DrawCurve *> &target) {
    target.clear();
    for (const auto curve : source)
        target.append(new DrawCurve(*curve));
}

Note *SingingClip::findNoteById(int id) const {
    return MathUtils::findItemById<Note *>(m_notes, id);
}

void SingingClip::notifyNoteChanged(NoteChangeType type, const QList<Note *> &notes) {
    emit noteChanged(type, notes);
}

// const DsParams &DsSingingClip::params() const {
//     return m_params;
// }