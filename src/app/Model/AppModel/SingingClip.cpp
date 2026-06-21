//
// Created by fluty on 24-9-18.
//

// ReSharper disable CppUseRangeAlgorithm

#include "SingingClip.h"

#include "AppModel.h"
#include "Clip.h"
#include "DrawCurve.h"
#include "Note.h"
#include "InferPiece.h"
#include "Timeline.h"
#include "Track.h"
#include "Modules/Language/OnsetMarker/OnsetMarkerMgr.h"
#include "Modules/Language/S2pMgr.h"
#include "Modules/SingingClipSlicer/SingingClipSlicer.h"
#include "Utils/AppModelUtils.h"
#include "Utils/MathUtils.h"

#include <QTimer>

using namespace SpeakerMixModel;

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
    switch (type) {
        case Insert:
        case Remove:
        case TimeKeyPropertyChange:
        case EditedWordPropertyChange:
        case EditedPhonemeOffsetChange:
            bumpInferenceRevision();
            break;
        case OriginalWordPropertyChange:
            break;
    }
    emit noteChanged(type, notes);
}

void SingingClip::notifyParamChanged(const ParamInfo::Name name, const Param::Type type) {
    if (type == Param::Edited)
        bumpInferenceRevision();
    emit paramChanged(name, type);
}

const PieceList &SingingClip::pieces() const {
    return m_pieces;
}

void SingingClip::removeAllPieces() {
    PieceList temp = m_pieces;
    m_pieces.clear();
    bumpInferenceRevision();
    emit piecesChanged(m_pieces, {}, temp);
    for (const auto piece : temp)
        delete piece;
}

ReSegmentResult SingingClip::reSegment() {
    ReSegmentResult result;
    // TODO: Refactor AppModel to support multiple tempos
    Timeline timeline;
    timeline.tempos = {
        {0, appModel->tempo()}
    };

    auto [segments] = SingingClipSlicer::slice(timeline, m_notes.toList());

    // Check if existing piece and segment are the same
    // 1. Head padding length is the same
    // 2. Note sequence is the same
    // 3. Tail padding length is the same
    auto isSamePiece = [](const InferPiece &left, const Segment &right) {
        if (left.notes.count() != right.notes.count())
            return false;

        if (!qFuzzyCompare(left.paddingStartMs, right.paddingStartMs))
            return false;

        if (!qFuzzyCompare(left.paddingEndMs, right.paddingEndMs))
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
            // Ignore dirty segments, keep segments that are not marked as dirty and are the same as
            // before
            if (!piece->dirty && isSamePiece(*piece, segment)) {
                exists = true;
                // Although it's still the same segment, the head available space may have changed
                // and needs to be updated
                piece->headAvailableLengthMs = segment.headAvailableLengthMs;
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
            result.addedPieces.append(newPiece);
        }
    }
    PieceList temp = m_pieces;
    m_pieces.clear();
    m_pieces = newPieces;
    for (const auto piece : temp)
        result.removedPieceIds.append(piece->id());
    bumpInferenceRevision();
    emit piecesChanged(m_pieces, newPieces, temp);
    qInfo() << "piecesChanged";
    for (const auto piece : temp)
        delete piece;
    return result;
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

quint64 SingingClip::inferenceRevision() const {
    return m_inferenceRevision;
}

quint64 SingingClip::bumpInferenceRevision() {
    return ++m_inferenceRevision;
}

void SingingClip::setDefaultLanguage(const QString &language) {
    const auto oldLanguage = defaultLanguage();
    const auto oldG2pId = defaultG2pId();
    m_defaultLanguage = language;
    this->updateDefaultG2pId(language);
    if (oldLanguage != defaultLanguage() || oldG2pId != defaultG2pId())
        bumpInferenceRevision();
}

QString SingingClip::defaultLanguage() const {
    // 空值表示跟随父级
    if (m_defaultLanguage.get().isEmpty()) {
        if (const auto track = qobject_cast<const Track *>(parent())) {
            return track->defaultLanguage();
        }
        return "unknown";
    }
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

SingerInfo SingingClip::ownSingerInfo() const {
    return m_singerInfo.get();
}

SpeakerInfo SingingClip::speakerInfo() const {
    if (useTrackSingerInfo) {
        return m_trackSpeakerInfo.get();
    }
    return m_speakerInfo.get();
}

SpeakerInfo SingingClip::ownSpeakerInfo() const {
    return m_speakerInfo.get();
}

SpeakerMixData SingingClip::speakerMixData() const {
    return useTrackSingerInfo ? m_trackSpeakerMixData : m_ownSpeakerMixData;
}

SpeakerMixData SingingClip::ownSpeakerMixData() const {
    return m_ownSpeakerMixData;
}

SpeakerMixData SingingClip::trackSpeakerMixData() const {
    return m_trackSpeakerMixData;
}

void SingingClip::setSpeakerMixData(const SpeakerMixData &data) {
    if (useTrackSingerInfo)
        return;

    setOwnSpeakerMixData(data);
}

void SingingClip::setOwnSpeakerMixData(const SpeakerMixData &data) {
    const auto normalized = normalizeSpeakerMixData(data);
    if (m_ownSpeakerMixData == normalized)
        return;

    const auto oldEffective = speakerMixData();
    m_ownSpeakerMixData = normalized;
    bumpInferenceRevision();
    if (oldEffective != speakerMixData())
        Q_EMIT speakerMixChanged(speakerMixData());
}

void SingingClip::setTrackSpeakerMixData(const SpeakerMixData &data) {
    const auto normalized = normalizeSpeakerMixData(data);
    if (m_trackSpeakerMixData == normalized)
        return;

    const auto oldEffective = speakerMixData();
    m_trackSpeakerMixData = normalized;
    if (oldEffective != speakerMixData()) {
        bumpInferenceRevision();
        Q_EMIT speakerMixChanged(speakerMixData());
    }
}

void SingingClip::resetSpeakerMixToSingle() {
    setSpeakerMixData({});
}

QString SingingClip::speakerId() const {
    return speakerInfo().id();
}

void SingingClip::setTrackSingerAndSpeakerInfo(const SingerInfo &singerInfo,
                                               const SpeakerInfo &speakerInfo) {
    const auto oldSingerInfo = this->singerInfo();
    const auto oldSpeakerInfo = this->speakerInfo();
    m_singerSpeakerBatching = true;
    m_trackSingerInfo = singerInfo;
    m_trackSpeakerInfo = speakerInfo;
    m_singerSpeakerBatching = false;
    updateDefaultG2pId(m_defaultLanguage);
    if (oldSingerInfo != this->singerInfo() || oldSpeakerInfo != this->speakerInfo()) {
        Q_EMIT singerOrSpeakerChanged();
    }
}

void SingingClip::setOwnSingerAndSpeaker(const SingerInfo &singerInfo,
                                         const SpeakerInfo &speakerInfo) {
    const auto oldSingerInfo = this->singerInfo();
    const auto oldSpeakerInfo = this->speakerInfo();
    m_singerSpeakerBatching = true;
    m_speakerInfo = speakerInfo;
    m_singerInfo = singerInfo;
    useTrackSpeakerInfo = false;
    useTrackSingerInfo = false;
    m_singerSpeakerBatching = false;
    updateDefaultG2pId(m_defaultLanguage);
    if (oldSingerInfo != this->singerInfo() || oldSpeakerInfo != this->speakerInfo()) {
        resetSpeakerMixIfSingerChanged(oldSingerInfo);
        Q_EMIT singerOrSpeakerChanged();
    }
}

void SingingClip::useTrackSingerAndSpeaker() {
    const auto oldSingerInfo = this->singerInfo();
    const auto oldSpeakerInfo = this->speakerInfo();
    const auto oldSpeakerMix = this->speakerMixData();
    m_singerSpeakerBatching = true;
    useTrackSpeakerInfo = true;
    useTrackSingerInfo = true;
    m_singerSpeakerBatching = false;
    updateDefaultG2pId(m_defaultLanguage);
    if (oldSingerInfo != this->singerInfo() || oldSpeakerInfo != this->speakerInfo()) {
        resetSpeakerMixIfSingerChanged(oldSingerInfo);
        Q_EMIT singerOrSpeakerChanged();
    }
    if (oldSpeakerMix != this->speakerMixData()) {
        bumpInferenceRevision();
        Q_EMIT speakerMixChanged(this->speakerMixData());
    }
}

SingerIdentifier SingingClip::singerIdentifier() const {
    return singerInfo().identifier();
}

void SingingClip::init() {
    m_defaultLanguage.onChanged(qSignalCallback(defaultLanguageChanged));
    m_singerInfo.onChanged([this](const SingerInfo &) {
        if (m_singerSpeakerBatching)
            return;
        if (!useTrackSingerInfo)
            Q_EMIT singerOrSpeakerChanged();
    });
    m_trackSingerInfo.onChanged([this](const SingerInfo &) {
        if (m_singerSpeakerBatching)
            return;
        if (useTrackSingerInfo)
            Q_EMIT singerOrSpeakerChanged();
    });
    useTrackSingerInfo.onChanged([this](bool) {
        if (m_singerSpeakerBatching)
            return;
        useTrackSpeakerInfo = useTrackSingerInfo.get();
        Q_EMIT singerOrSpeakerChanged();
    });
    connect(this, &SingingClip::singerOrSpeakerChanged, this, [this] {
        bumpInferenceRevision();
        const auto currentSingerInfo = singerInfo();
        // bool needsResegment = false;
        // for (const auto piece : std::as_const(m_pieces)) {
        //     auto currentIdentifier = currentSingerInfo.identifier();
        //     if (piece->identifier != currentIdentifier) {
        //         piece->identifier = std::move(currentIdentifier);
        //         piece->dirty = true;
        //         needsResegment = true;
        //     }
        // }
        // TODO
        // 现在是在音素信息未获取时分段，会导致segment.paddingStartMs计算错误，应该等待音素信息获取完成后再分段
        // if (needsResegment) {
        //     reSegment();
        // }

        const auto s2pMgr = S2pMgr::instance();
        const auto onsetMarkerMgr = OnsetMarkerMgr::instance();
        for (const auto &lang : currentSingerInfo.languages()) {
            s2pMgr->addS2p(currentSingerInfo.identifier(), lang.g2p(), lang.s2pMode(),
                           lang.s2pFile());
            onsetMarkerMgr->addOnsetMarker(currentSingerInfo.identifier(), lang.id(),
                                           lang.onsetMode(), lang.onsetFile());
        }
    });
    m_speakerInfo.onChanged([this](const SpeakerInfo &) {
        if (m_singerSpeakerBatching)
            return;
        if (!useTrackSingerInfo)
            Q_EMIT singerOrSpeakerChanged();
    });
    m_trackSpeakerInfo.onChanged([this](const SpeakerInfo &) {
        if (m_singerSpeakerBatching)
            return;
        if (useTrackSingerInfo)
            Q_EMIT singerOrSpeakerChanged();
    });
    useTrackSpeakerInfo.onChanged([this](bool) {
        if (m_singerSpeakerBatching)
            return;
        useTrackSpeakerInfo = useTrackSingerInfo.get();
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

void SingingClip::resetSpeakerMixIfSingerChanged(const SingerInfo &oldSingerInfo) {
    if (oldSingerInfo == singerInfo() || m_ownSpeakerMixData.mode == SingerSourceMode::Single)
        return;

    resetSpeakerMixToSingle();
}
