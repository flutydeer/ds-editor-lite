//
// Created by fluty on 24-9-18.
//

#include "SingingClip.h"

#include "AppModel.h"
#include "DrawCurve.h"
#include "Note.h"
#include "InferPiece.h"
#include "Utils/AppModelUtils.h"
#include "Utils/MathUtils.h"

#include <QTimer>

SingingClip::SingingClip() : Clip(), params(this) {
    init();
}

SingingClip::SingingClip(const QList<Note *> &notes) : Clip(), params(this) {
    init();
    insertNotes(notes);
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

void SingingClip::insertNotes(const QList<Note *> &notes) {
    for (const auto note : notes)
        insertNote(note);
}

void SingingClip::removeNote(Note *note) {
    m_notes.remove(note);
    note->setClip(nullptr);
}

Note *SingingClip::findNoteById(const int id) const {
    return MathUtils::findItemById<Note *>(m_notes, id);
}

void SingingClip::notifyNoteChanged(const NoteChangeType type, const QList<Note *> &notes) {
    emit noteChanged(type, notes);
}

void SingingClip::notifyParamChanged(const ParamInfo::Name name, const Param::Type type) {
    emit paramChanged(name, type);
}

const PieceList &SingingClip::pieces() const {
    return m_pieces;
}

void SingingClip::reSegment() {
    auto newSegments = AppModelUtils::simpleSegment(m_notes.toList());
    PieceList newPieces;
    for (const auto &segment : newSegments) {
        bool exists = false;
        for (int i = 0; i < m_pieces.count(); i++) {
            const auto piece = m_pieces[i];
            // 忽略脏的分段
            if (!piece->dirty && piece->notes == segment) {
                exists = true;
                newPieces.append(piece);
                m_pieces.removeAt(i);
                break;
            }
        }
        if (!exists) {
            const auto newPiece = new InferPiece(this);
            newPiece->identifier = singerIdentifier;
            newPiece->speaker = getSpeaker();
            newPiece->notes = segment;
            newPieces.append(newPiece);
        }
    }
    PieceList temp = m_pieces;
    m_pieces.clear();
    m_pieces = newPieces;
    emit piecesChanged(m_pieces, newPieces, temp);
    qInfo() << "piecesChanged";
    for (const auto piece : temp)
        delete piece;
}

void SingingClip::updateOriginalParam(const ParamInfo::Name name) {
    // 重新获取所有分段的所有相应自动参数，更新剪辑上的自动参数信息
    QList<Curve *> curves;
    for (const auto &piece : m_pieces) {
        // 只获取有推理结果的分段
        if (const auto curve = piece->getOriginalCurve(name); !curve->isEmpty())
            curves.append(new DrawCurve(*curve)); // 复制分段上的参数
    }
    const auto param = params.getParamByName(name);
    param->setCurves(Param::Original, curves, this);
    notifyParamChanged(name, Param::Original);
}

InferPiece *SingingClip::findPieceById(const int id) const {
    return MathUtils::findItemById<InferPiece *>(m_pieces, id);
}

PieceList SingingClip::findPiecesByNotes(const QList<Note *> &notes) const {
    QSet<InferPiece *> result;
    for (const auto &note : notes) {
        for (const auto &piece : m_pieces) {
            if (std::ranges::find(piece->notes, note) != piece->notes.end()) {
                result.insert(piece);
                break;
            }
        }
    }
    return {result.begin(), result.end()}; // 将 QSet 转换为 QList 返回
}

const Property<SingerIdentifier> &SingingClip::getSingerIdentifier() const {
    if (useTrackSingerIdentifier) {
        return trackSingerIdentifier;
    }
    return singerIdentifier;
}

QString SingingClip::getSpeaker() const {
    if (useTrackSpeaker) {
        return trackSpeaker.get();
    }
    return speaker.get();
}

void SingingClip::init() {
    defaultLanguage.onChanged(qSignalCallback(defaultLanguageChanged));
    singerIdentifier.onChanged([this](const SingerIdentifier &) {
        if (!useTrackSingerIdentifier) {
            Q_EMIT identifierChanged(getSingerIdentifier());
        }
    });
    trackSingerIdentifier.onChanged([this](const SingerIdentifier &) {
        if (useTrackSingerIdentifier) {
            Q_EMIT identifierChanged(getSingerIdentifier());
        }
    });
    useTrackSingerIdentifier.onChanged([this](bool) {
        if (singerIdentifier.get() != trackSingerIdentifier.get()) {
            Q_EMIT identifierChanged(getSingerIdentifier());
        }
    });
    connect(this, &SingingClip::identifierChanged, this,
            [this](const SingerIdentifier &currentIdentifier) {
                bool needsResegment = false;
                for (const auto piece : std::as_const(m_pieces)) {
                    if (piece->identifier != currentIdentifier) {
                        piece->identifier = currentIdentifier;
                        piece->dirty = true;
                        needsResegment = true;
                    }
                }
                if (needsResegment) {
                    reSegment();
                }
            });
    speaker.onChanged([this](const QString &) {
        if (!useTrackSpeaker) {
            Q_EMIT speakerChanged(getSpeaker());
        }
    });
    trackSpeaker.onChanged([this](const QString &) {
        if (useTrackSpeaker) {
            Q_EMIT speakerChanged(getSpeaker());
        }
    });
    useTrackSpeaker.onChanged([this](bool) {
        if (speaker.get() != trackSpeaker.get()) {
            Q_EMIT speakerChanged(getSpeaker());
        }
    });
    connect(this, &SingingClip::speakerChanged, this,
        [this](const QString &currentSpeaker) {
            bool needsResegment = false;
            for (const auto piece : std::as_const(m_pieces)) {
                qDebug() << "changing speaker before" << piece->speaker << "after" << currentSpeaker;
                if (piece->speaker != currentSpeaker) {
                    piece->speaker = currentSpeaker;
                    piece->dirty = true;
                    needsResegment = true;
                }
            }
            if (needsResegment) {
                reSegment();
            }
        });
}