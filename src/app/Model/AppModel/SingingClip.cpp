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
#include "Modules/SingingClipSlicer/SingingClipSlicer.h"
#include "Model/Utils/AppModelUtils.h"
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
        case EditedPronunciationOnly:
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
    if (m_pieces.isEmpty())
        return;

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
            newPiece->notes = segment.notes;
            newPiece->headAvailableLengthMs = segment.headAvailableLengthMs;
            newPiece->paddingStartMs = segment.paddingStartMs;
            newPiece->paddingEndMs = segment.paddingEndMs;
            const Timeline timeline{{{0, appModel->tempo()}}};
            newPiece->speakerMix = InferSpeakerMixModel::effectiveSpeakerMixFromData(
                speakerMixData(), speakerId(), newPiece->localStartTick(), newPiece->localEndTick(),
                timeline);
            newPiece->speaker = newPiece->speakerMix.fallbackSpeaker;
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
    if (m_useTrackSingerInfo) {
        return m_trackSingerInfo.get();
    }
    return m_singerInfo.get();
}

SingerInfo SingingClip::ownSingerInfo() const {
    return m_singerInfo.get();
}

SpeakerInfo SingingClip::speakerInfo() const {
    if (m_useTrackSingerInfo) {
        return m_trackSpeakerInfo.get();
    }
    return m_speakerInfo.get();
}

SpeakerInfo SingingClip::ownSpeakerInfo() const {
    return m_speakerInfo.get();
}

SpeakerMixData SingingClip::speakerMixData() const {
    return m_useTrackSingerInfo ? m_trackSpeakerMixData : m_ownSpeakerMixData;
}

SpeakerMixData SingingClip::ownSpeakerMixData() const {
    return m_ownSpeakerMixData;
}

SpeakerMixData SingingClip::trackSpeakerMixData() const {
    return m_trackSpeakerMixData;
}

EffectiveVoiceContext SingingClip::effectiveVoiceContext() const {
    return {singerInfo(), speakerInfo(), speakerMixData(), m_useTrackSingerInfo.get()};
}

bool SingingClip::usesTrackVoiceContext() const {
    return m_useTrackSingerInfo.get();
}

void SingingClip::setSpeakerMixData(const SpeakerMixData &data) {
    if (m_useTrackSingerInfo) {
        const auto context = effectiveVoiceContext();
        setOwnVoiceContext(context.singer, context.speaker, data);
        return;
    }

    setOwnSpeakerMixData(data);
}

void SingingClip::setOwnSpeakerMixData(const SpeakerMixData &data) {
    const auto normalized = normalizeSpeakerMixData(data);
    if (m_ownSpeakerMixData == normalized)
        return;

    const auto oldContext = effectiveVoiceContext();
    m_ownSpeakerMixData = normalized;
    notifyEffectiveVoiceContextChanged(oldContext);
}

void SingingClip::setTrackSpeakerMixData(const SpeakerMixData &data) {
    setTrackVoiceContext(m_trackSingerInfo, m_trackSpeakerInfo, data);
}

void SingingClip::resetSpeakerMixToSingle() {
    setSpeakerMixData({});
}

QString SingingClip::speakerId() const {
    return speakerInfo().id();
}

void SingingClip::setTrackSingerAndSpeakerInfo(const SingerInfo &singerInfo,
                                               const SpeakerInfo &speakerInfo) {
    setTrackVoiceContext(singerInfo, speakerInfo, m_trackSpeakerMixData);
}

void SingingClip::setTrackVoiceContext(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                       const SpeakerMixData &speakerMixData) {
    const auto oldContext = effectiveVoiceContext();
    const auto normalizedSpeakerMixData = normalizeSpeakerMixData(speakerMixData);
    if (m_trackSingerInfo.get() == singerInfo && m_trackSpeakerInfo.get() == speakerInfo &&
        m_trackSpeakerMixData == normalizedSpeakerMixData)
        return;

    m_trackSingerInfo = singerInfo;
    m_trackSpeakerInfo = speakerInfo;
    m_trackSpeakerMixData = normalizedSpeakerMixData;
    updateDefaultG2pId(m_defaultLanguage);
    notifyEffectiveVoiceContextChanged(oldContext);
}

void SingingClip::setOwnVoiceContext(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                     const SpeakerMixData &speakerMixData) {
    const auto oldContext = effectiveVoiceContext();
    m_speakerInfo = speakerInfo;
    m_singerInfo = singerInfo;
    m_useTrackSingerInfo = false;
    m_ownSpeakerMixData = normalizeSpeakerMixData(speakerMixData);
    updateDefaultG2pId(m_defaultLanguage);
    notifyEffectiveVoiceContextChanged(oldContext);
}

void SingingClip::selectOwnSingleSpeaker(const SingerInfo &singerInfo,
                                         const SpeakerInfo &speakerInfo) {
    setOwnVoiceContext(singerInfo, speakerInfo, {});
}

void SingingClip::setOwnSingerAndSpeaker(const SingerInfo &singerInfo,
                                         const SpeakerInfo &speakerInfo) {
    selectOwnSingleSpeaker(singerInfo, speakerInfo);
}

void SingingClip::useTrackVoiceContext() {
    const auto oldContext = effectiveVoiceContext();
    m_useTrackSingerInfo = true;
    updateDefaultG2pId(m_defaultLanguage);
    notifyEffectiveVoiceContextChanged(oldContext);
}

void SingingClip::useTrackSingerAndSpeaker() {
    useTrackVoiceContext();
}

SingerIdentifier SingingClip::singerIdentifier() const {
    return singerInfo().identifier();
}

void SingingClip::init() {
    m_defaultLanguage.onChanged(qSignalCallback(defaultLanguageChanged));
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

void SingingClip::notifyEffectiveVoiceContextChanged(const EffectiveVoiceContext &oldContext) {
    const auto newContext = effectiveVoiceContext();
    if (oldContext == newContext)
        return;

    if (!oldContext.hasSameInferenceInput(newContext))
        bumpInferenceRevision();

    Q_EMIT voiceContextChanged({oldContext, newContext});
}
