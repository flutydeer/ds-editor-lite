//
// Created by fluty on 2024/1/27.
//

#include "Clip.h"

#include <QDebug>

#include "Note.h"
#include "Curve.h"
#include "DrawCurve.h"
#include "Model/Inference/InferPiece.h"
#include "Utils/AppModelUtils.h"
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
    defaultLanguage.setNotify(
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
    // qDebug() << "insert note #" << note->id() << note->lyric() << "clip #" << id();
}

void SingingClip::removeNote(Note *note) {
    m_notes.remove(note);
    note->setClip(nullptr);
    // qDebug() << "remove note #" << note->id() << note->lyric() << "clip #" << id();
}

void SingingClip::notifyParamChanged(ParamInfo::Name name, Param::Type type) {
    emit paramChanged(name, type);
}

const QList<InferPiece *> &SingingClip::pieces() const {
    return m_pieces;
}

void SingingClip::reSegment() {
    QList<Note *> notes;
    for (auto note : m_notes)
        notes.append(note);

    auto newSegments = AppModelUtils::simpleSegment(notes);
    QList<InferPiece*> newPieces;
    for (const auto &segment : newSegments) {
        bool exists = false;
        for (int i = 0; i < m_pieces.count(); i++) {
            auto piece = m_pieces[i];
            if (piece->notes == segment) {
                exists = true;
                newPieces.append(piece);
                m_pieces.removeAt(i);
                break;
            }
        }
        if (!exists) {
            auto newPiece = new InferPiece();
            newPiece->notes = segment;
            newPieces.append(newPiece);
        }
    }
    QList<InferPiece *> temp = m_pieces;
    m_pieces.clear();
    m_pieces = newPieces;
    emit piecesChanged(m_pieces);
    qInfo() << "piecesChanged";
    for (const auto piece : temp)
        delete piece;
}

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