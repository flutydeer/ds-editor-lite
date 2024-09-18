//
// Created by fluty on 24-9-18.
//

#include "SingingClip.h"

#include "DrawCurve.h"
#include "Model/Inference/InferPiece.h"
#include "Utils/AppModelUtils.h"
#include "Utils/MathUtils.h"

SingingClip::SingingClip() : Clip() {
    defaultLanguage.setNotify(
        [=](const AppGlobal::LanguageType value) { emit defaultLanguageChanged(value); });
}

SingingClip::~SingingClip() {
    auto notes = m_notes.toList();
    for (int i = 0; i < notes.count(); i++) {
        delete notes[i];
    }
}

IClip::ClipType SingingClip::clipType() const {
    return Singing;
}

const OverlappableSerialList<Note> &SingingClip::notes() const {
    return m_notes;
}

void SingingClip::insertNote(Note *note) {
    note->setClip(this);
    m_notes.add(note);
}

void SingingClip::removeNote(Note *note) {
    m_notes.remove(note);
    note->setClip(nullptr);
}

Note *SingingClip::findNoteById(int id) const {
    return MathUtils::findItemById<Note *>(m_notes, id);
}

void SingingClip::notifyNoteChanged(NoteChangeType type, const QList<Note *> &notes) {
    emit noteChanged(type, notes);
}

void SingingClip::notifyParamChanged(ParamInfo::Name name, Param::Type type) {
    emit paramChanged(name, type);
}

const QList<InferPiece *> &SingingClip::pieces() const {
    return m_pieces;
}

void SingingClip::reSegment() {
    auto newSegments = AppModelUtils::simpleSegment(m_notes.toList());
    QList<InferPiece *> newPieces;
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