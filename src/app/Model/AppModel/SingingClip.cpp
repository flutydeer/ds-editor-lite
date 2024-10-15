//
// Created by fluty on 24-9-18.
//

#include "SingingClip.h"

#include "DrawCurve.h"
#include "Note.h"
#include "InferPiece.h"
#include "Utils/AppModelUtils.h"
#include "Utils/MathUtils.h"

#include <QTimer>

SingingClip::SingingClip() : Clip() {
    defaultLanguage.setNotify(qSignalCallback(defaultLanguageChanged));
    configPath.setNotify(qSignalCallback(configPathChanged));
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
            // 忽略脏的分段
            if (!piece->dirty && piece->notes == segment) {
                exists = true;
                newPieces.append(piece);
                m_pieces.removeAt(i);
                break;
            }
        }
        if (!exists) {
            auto newPiece = new InferPiece(this);
            newPiece->singerName = configPath;
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

void SingingClip::updateOriginalParam(ParamInfo::Name name) {
    // 重新获取所有分段的所有相应自动参数，更新剪辑上的自动参数信息
    QList<Curve *> curves;
    for (const auto &piece : m_pieces) {
        // 只获取有推理结果的分段
        if (const auto curve = piece->getCurve(name); !curve->isEmpty())
            curves.append(new DrawCurve(*curve)); // 复制分段上的参数
    }
    auto param = params.getParamByName(name);
    param->setCurves(Param::Original, curves);
    notifyParamChanged(name, Param::Original);
}

InferPiece *SingingClip::findPieceById(int id) const {
    return MathUtils::findItemById<InferPiece *>(m_pieces, id);
}

QList<InferPiece *> SingingClip::findPiecesByNotes(const QList<Note *> &notes) const {
    QSet<InferPiece *> result;
    for (const auto &note : notes) {
        for (const auto &piece : m_pieces) {
            if (std::find(piece->notes.begin(), piece->notes.end(), note) != piece->notes.end()) {
                result.insert(piece);
                break;
            }
        }
    }
    return {result.begin(), result.end()}; // 将 QSet 转换为 QList 返回
}