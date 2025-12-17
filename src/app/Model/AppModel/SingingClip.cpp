//
// Created by fluty on 24-9-18.
//

// ReSharper disable CppUseRangeAlgorithm

#include "SingingClip.h"

#include "AppModel.h"
#include "DrawCurve.h"
#include "Note.h"
#include "InferPiece.h"
#include "Timeline.h"
#include "Modules/Language/S2pMgr.h"
#include "Modules/SingingClipSlicer/SingingClipSlicer.h"
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
    // TODO 重构 AppModel 支持多曲速
    Timeline timeline;
    timeline.tempos = {{0, appModel->tempo()}};

    auto [segments] = SingingClipSlicer::slice(timeline, m_notes.toList());

    // 判断已有 piece 和 segment 是否相同
    // 1. 头部填充长度相同
    // 2. 音符序列相同
    // 3. 尾部填充长度相同
    auto isSamePiece = [](const InferPiece &left, const Segment &right) {
        if (left.notes.count() != right.notes.count())
            return false;

        if (qFuzzyCompare(left.paddingStartMs, right.paddingStartMs))
            return false;

        if (qFuzzyCompare(left.paddingEndMs, right.paddingEndMs))
            return false;

        for (int i = 0; i < left.notes.count(); i++) {
            if (left.notes[i] != right.notes[i])
                return false;
        }
        return true;
    };

    PieceList newPieces;
    for (const auto &segment : segments) {
        bool exists = false;
        for (int i = 0; i < m_pieces.count(); i++) {
            const auto piece = m_pieces[i];
            // 忽略脏的分段，保留未被标脏且与原来相同的片段
            if (!piece->dirty && isSamePiece(*piece, segment)) {
                exists = true;
                newPieces.append(piece);
                m_pieces.removeAt(i);
                break;
            }
        }
        if (!exists) {
            const auto newPiece = new InferPiece(this);
            newPiece->identifier = singerIdentifier();
            newPiece->speaker = speakerId();
            newPiece->notes = segment.notes;
            newPiece->headAvailableLengthMs = segment.headAvailableLengthMs;
            newPiece->paddingStartMs = segment.paddingStartMs;
            newPiece->paddingEndMs = segment.paddingEndMs;
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
            if (std::find(piece->notes.begin(), piece->notes.end(), note) != piece->notes.end()) {
                result.insert(piece);
                break;
            }
        }
    }
    return {result.begin(), result.end()}; // 将 QSet 转换为 QList 返回
}

void SingingClip::setDefaultLanguage(const QString &language) {
    m_defaultLanguage = language;
    this->updateDefaultG2pId(language);
}

QString SingingClip::defaultLanguage() const {
    return m_defaultLanguage.get();
}

QString SingingClip::defaultG2pId() const {
    return m_singerInfo.get().g2pId(m_defaultLanguage.get());
}

SingerInfo SingingClip::singerInfo() const {
    if (useTrackSingerInfo) {
        return m_trackSingerInfo.get();
    }
    return m_singerInfo.get();
}

void SingingClip::setSingerInfo(const SingerInfo &singerInfo) {
    m_singerInfo = singerInfo;
    this->updateDefaultG2pId(m_defaultLanguage);
}

void SingingClip::setTrackSingerInfo(const SingerInfo &singerInfo) {
    m_trackSingerInfo = singerInfo;
    this->updateDefaultG2pId(m_defaultLanguage);
}

SpeakerInfo SingingClip::speakerInfo() const {
    if (useTrackSpeakerInfo) {
        return m_trackSpeakerInfo.get();
    }
    return m_speakerInfo.get();
}

QString SingingClip::speakerId() const {
    return speakerInfo().id();
}

void SingingClip::setSpeakerInfo(const SpeakerInfo &speakerInfo) {
    m_speakerInfo = speakerInfo;
}

void SingingClip::setTrackSpeakerInfo(const SpeakerInfo &speakerInfo) {
    m_trackSpeakerInfo = speakerInfo;
}

SingerIdentifier SingingClip::singerIdentifier() const {
    return singerInfo().identifier();
}

void SingingClip::init() {
    m_defaultLanguage.onChanged(qSignalCallback(defaultLanguageChanged));
    m_singerInfo.onChanged([this](const SingerInfo &) {
        if (!useTrackSingerInfo) {
            Q_EMIT singerChanged(singerInfo());
        }
    });
    m_trackSingerInfo.onChanged([this](const SingerInfo &) {
        if (useTrackSingerInfo) {
            Q_EMIT singerChanged(singerInfo());
        }
    });
    useTrackSingerInfo.onChanged([this](bool) {
        if (m_singerInfo.get() != m_trackSingerInfo.get()) {
            Q_EMIT singerChanged(singerInfo());
        }
    });
    connect(this, &SingingClip::singerChanged, this, [this](const SingerInfo &currentSingerInfo) {
        bool needsResegment = false;
        for (const auto piece : std::as_const(m_pieces)) {
            auto currentIdentifier = currentSingerInfo.identifier();
            if (piece->identifier != currentIdentifier) {
                piece->identifier = std::move(currentIdentifier);
                piece->dirty = true;
                needsResegment = true;
            }
        }
        if (needsResegment) {
            reSegment();
        }

        // TODO: use dspkg dict
        const auto s2pMgr = S2pMgr::instance();
        const auto languages = currentSingerInfo;
        for (const auto &lang : languages.languages())
            s2pMgr->addS2p(currentSingerInfo.identifier(), lang.g2p(), lang.dict());
    });
    m_speakerInfo.onChanged([this](const SpeakerInfo &) {
        if (!useTrackSpeakerInfo) {
            Q_EMIT speakerChanged(speakerInfo());
        }
    });
    m_trackSpeakerInfo.onChanged([this](const SpeakerInfo &) {
        if (useTrackSpeakerInfo) {
            Q_EMIT speakerChanged(speakerInfo());
        }
    });
    useTrackSpeakerInfo.onChanged([this](bool) {
        if (m_speakerInfo.get() != m_trackSpeakerInfo.get()) {
            Q_EMIT speakerChanged(speakerInfo());
        }
    });
    connect(this, &SingingClip::speakerChanged, this,
            [this](const SpeakerInfo &currentSpeakerInfo) {
                bool needsResegment = false;
                const auto currentSpeaker = currentSpeakerInfo.id();
                for (const auto piece : std::as_const(m_pieces)) {
                    qDebug() << "changing speaker before" << piece->speaker << "after"
                        << currentSpeaker;
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

void SingingClip::updateDefaultG2pId(const QString &language) {
    const auto languageInfos = this->singerInfo().languages();
    for (const auto &languageInfo : languageInfos) {
        if (language == languageInfo.id()) {
            m_defaultG2pId = languageInfo.g2p();
            break;
        }
    }
}