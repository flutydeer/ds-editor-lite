// ReSharper disable CppUseRangeAlgorithm

#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/DrawCurve.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/InferenceData/InferPiece.h>
#include <lite/MusicBase/Timeline.h>
#include <lite/ProjectModel/AppModel/Track.h>
#include <lite/ProjectModel/SingingClipSlicer/SingingClipSlicer.h>
#include <lite/ProjectModel/Utils/AppModelUtils.h>
#include <lite/ProjectModel/Voice/LanguageCompatibility.h>
#include <lite/Support/MathUtils.h>

#include <QSet>
#include <QTimer>

using namespace SpeakerMixModel;

SingingClip::SingingClip() : Clip(), params(this) {
}

SingingClip::SingingClip(const QList<Note *> &notes) : Clip(), params(this) {
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
            clearNoteInferenceErrors(notes);
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

ReSegmentResult SingingClip::reSegment(const Timeline &timeline, const bool bumpRevision) {
    ReSegmentResult result;
    const auto sliceResult = SingingClipSlicer::slice(timeline, m_notes.toList());
    const auto &segments = sliceResult.segments;

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
            newPiece->speakerMix = InferSpeakerMixModel::effectiveSpeakerMixFromData(
                speakerMixData(), speakerId(), start() + newPiece->localStartTick(timeline),
                start() + newPiece->localEndTick(timeline), start(), timeline);
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
    if (bumpRevision)
        bumpInferenceRevision();
    emit piecesChanged(m_pieces, newPieces, temp);
    qInfo() << "piecesChanged";
    for (const auto piece : temp)
        delete piece;
    rebuildSliceErrors(sliceResult);
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

QString SingingClip::fallbackLanguage() const {
    return LanguageCompatibility::fallbackLanguage(singerInfo());
}

SingerInfo SingingClip::singerInfo() const {
    if (m_useTrackSingerInfo) {
        return m_trackSingerInfo.get();
    }
    return m_singerInfo.get();
}

SingerInfo SingingClip::trackSingerInfo() const {
    return m_trackSingerInfo.get();
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
        notifyEffectiveVoiceContextChanged(oldContext);
}

void SingingClip::setOwnVoiceContext(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                                     const SpeakerMixData &speakerMixData) {
    const auto oldContext = effectiveVoiceContext();
    m_speakerInfo = speakerInfo;
    m_singerInfo = singerInfo;
    m_useTrackSingerInfo = false;
    m_ownSpeakerMixData = normalizeSpeakerMixData(speakerMixData);
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
        notifyEffectiveVoiceContextChanged(oldContext);
}

void SingingClip::useTrackSingerAndSpeaker() {
    useTrackVoiceContext();
}

SingerIdentifier SingingClip::singerIdentifier() const {
    return singerInfo().identifier();
}

void SingingClip::notifyEffectiveVoiceContextChanged(const EffectiveVoiceContext &oldContext) {
    const auto newContext = effectiveVoiceContext();
    if (oldContext == newContext)
        return;

    if (!oldContext.hasSameInferenceInput(newContext)) {
        bumpInferenceRevision();
        // Slice results and task failures describe the previous inference input,
        // so they must not outlive the voice change.
        clearAllNoteInferenceErrors();
    }

    Q_EMIT voiceContextChanged({oldContext, newContext});
}

const QHash<int, NoteInferenceErrorInfo> &SingingClip::noteInferenceErrors() const {
    return m_noteInferenceErrors;
}

const QList<QPair<int, int>> &SingingClip::skippedPhraseRanges() const {
    return m_skippedPhraseRanges;
}

void SingingClip::setPendingNoteTaskErrors(const QHash<int, QString> &taskErrors) {
    m_pendingNoteTaskErrors = taskErrors;
    refreshNoteDiagnostics();
}

void SingingClip::rebuildSliceErrors(const SliceResult &sliceResult) {
    m_sliceErrors.clear();
    for (const auto &excluded : sliceResult.excludedNotes) {
        NoteInferenceErrorInfo info;
        info.reason = excluded.reason;
        info.detail = m_pendingNoteTaskErrors.value(excluded.noteId);
        m_sliceErrors.insert(excluded.noteId, info);
    }
    const bool rangesChanged = sliceResult.skippedPhraseRanges != m_skippedPhraseRanges;
    m_skippedPhraseRanges = sliceResult.skippedPhraseRanges;
    refreshNoteDiagnostics(rangesChanged);
}

namespace {
    bool canCheckPhonemeCompatibility(const SingerCapabilitySummary &capability) {
        // Every non-vocoder stage must have contributed a table and those tables
        // must agree, otherwise the intersection cannot say whether a phoneme is
        // rejected. Degraded and inconsistent models stay unreported instead of
        // being presented as either compatible or incompatible.
        return !capability.phonemeDegraded && capability.phonemeConsistency == 0 &&
               !capability.effectivePhonemes.isEmpty();
    }

    bool canCheckPhonemeLanguage(const SingerCapabilitySummary &capability) {
        return capability.languageConsistency == 0 && !capability.effectiveLanguages.isEmpty();
    }
}

void SingingClip::refreshNoteDiagnostics(const bool rangesChanged) {
    m_languageErrors.clear();
    m_phonemeErrors.clear();

    const auto singer = singerInfo();
    QList<Note *> notes;
    notes.reserve(m_notes.count());
    for (const auto note : m_notes)
        notes.append(note);

    for (const auto &conflict : LanguageCompatibility::conflicts(notes, singer)) {
        const auto available = LanguageCompatibility::supportedLanguages(singer);
        for (const auto noteId : conflict.noteIds) {
            NoteInferenceErrorInfo info;
            info.reason = NoteInferenceIssueReason::UnsupportedLanguage;
            info.language = conflict.language;
            info.availableLanguages = available;
            info.usesEditedPhonemes = conflict.notesWithEditedPhonemes.contains(noteId);
            info.detail = m_pendingNoteTaskErrors.value(noteId);
            m_languageErrors.insert(noteId, info);
        }
    }

    const auto support = LanguageCompatibility::conversionSupport(singer);
    if (support.isKnown() && support.languageIds.isEmpty()) {
        // A singer without any conversion route cannot convert an unspecified
        // note either. Report the missing capability on those notes instead of
        // inventing a declaration conflict for them.
        for (const auto note : notes) {
            if (!note->language().isEmpty() || note->isSlur())
                continue;
            NoteInferenceErrorInfo info;
            info.reason = NoteInferenceIssueReason::UnsupportedLanguage;
            info.detail = m_pendingNoteTaskErrors.value(note->id());
            info.usesEditedPhonemes = note->phonemeNameSeq().isEdited();
            m_languageErrors.insert(note->id(), info);
        }
    }

    const auto &capability = singer.capability();
    if (capability) {
        QSet<QString> phonemes;
        QSet<QString> phonemeLanguages;
        if (canCheckPhonemeCompatibility(*capability))
            phonemes = QSet<QString>(capability->effectivePhonemes.begin(),
                                     capability->effectivePhonemes.end());
        if (canCheckPhonemeLanguage(*capability))
            phonemeLanguages = QSet<QString>(capability->effectiveLanguages.begin(),
                                             capability->effectiveLanguages.end());

        for (const auto note : notes) {
            const auto &sequence = note->phonemeNameSeq().result();
            if (sequence.isEmpty())
                continue;
            for (const auto &phoneme : sequence) {
                // An empty symbol carries no claim about the model and is not a
                // compatibility statement either way.
                if (phoneme.name.isEmpty())
                    continue;
                // Model tables may list phonemes either bare or qualified with
                // their dictionary language, so both forms are accepted.
                const bool unknownName =
                    !phonemes.isEmpty() && !phonemes.contains(phoneme.name) &&
                    (phoneme.language.isEmpty() ||
                     !phonemes.contains(phoneme.language + QLatin1Char('/') + phoneme.name));
                const bool unknownLanguage = !phonemeLanguages.isEmpty() &&
                                             !phoneme.language.isEmpty() &&
                                             !phonemeLanguages.contains(phoneme.language);
                if (!unknownName && !unknownLanguage)
                    continue;
                NoteInferenceErrorInfo info;
                info.reason = NoteInferenceIssueReason::IncompatiblePhoneme;
                info.phoneme = phoneme.name;
                info.phonemeLanguage = phoneme.language;
                info.usesEditedPhonemes = note->phonemeNameSeq().isEdited();
                m_phonemeErrors.insert(note->id(), info);
                break;
            }
        }
    }

    QHash<int, NoteInferenceErrorInfo> merged = m_sliceErrors;
    for (auto it = m_languageErrors.constBegin(); it != m_languageErrors.constEnd(); ++it) {
        auto info = it.value();
        if (merged.contains(it.key()))
            info.otherReasons.append(merged.value(it.key()).reason);
        if (m_phonemeErrors.contains(it.key()))
            info.otherReasons.append(NoteInferenceIssueReason::IncompatiblePhoneme);
        merged.insert(it.key(), info);
    }
    for (auto it = m_phonemeErrors.constBegin(); it != m_phonemeErrors.constEnd(); ++it) {
        if (merged.contains(it.key()))
            continue;
        merged.insert(it.key(), it.value());
    }

    const bool changed = rangesChanged || merged != m_noteInferenceErrors;
    m_noteInferenceErrors = std::move(merged);
    if (changed)
        emit noteInferenceErrorsChanged();
}

void SingingClip::clearNoteInferenceErrors(const QList<Note *> &notes) {
    // Hide the stale slice and task diagnostics of the edited notes right away.
    // The language and phoneme ones are recomputed from the new state below.
    for (const auto note : notes) {
        m_sliceErrors.remove(note->id());
        m_pendingNoteTaskErrors.remove(note->id());
    }
    refreshNoteDiagnostics();
}

void SingingClip::clearAllNoteInferenceErrors() {
    const bool rangesChanged = !m_skippedPhraseRanges.isEmpty();
    m_pendingNoteTaskErrors.clear();
    m_sliceErrors.clear();
    m_skippedPhraseRanges.clear();
    refreshNoteDiagnostics(rangesChanged);
}
