#ifndef SINGINGCLIP_H
#define SINGINGCLIP_H

#include <lite/ProjectModel/AppModel/Clip.h>
#include <lite/ProjectModel/AppModel/EffectiveVoiceContext.h>
#include <lite/ProjectModel/AppModel/Params.h>
#include <lite/ProjectModel/AppModel/SpeakerMixData.h>
#include <lite/ADT/Property.h>
#include <lite/ProjectModel/AppModel/SingerIdentifier.h>
#include <lite/ProjectModel/SingingClipSlicer/Models/SliceResult.h>
#include <lite/ProjectModel/Voice/SingerInfo.h>
#include <lite/ProjectModel/Voice/SpeakerInfo.h>

#include <QHash>

class DrawCurve;
class InferPiece;
class Note;
class Timeline;

using PieceList = QList<InferPiece *>;
using SpeakerMixModel::SpeakerMixData;

struct ReSegmentResult {
    PieceList addedPieces;
    QList<int> removedPieceIds;
};

class SingingClip final : public Clip {
    Q_OBJECT
public:
    enum NoteChangeType {
        Insert,
        Remove,
        TimeKeyPropertyChange,
        OriginalWordPropertyChange,
        EditedWordPropertyChange,
        EditedPronunciationOnly,
        EditedPhonemeOffsetChange
    };

    explicit SingingClip();
    explicit SingingClip(const QList<Note *> &notes);
    ~SingingClip() override;

    ClipType clipType() const override;
    const OverlappableSerialList<Note> &notes() const;

    void insertNote(Note *note);
    void insertNotes(const QList<Note *> &notes);
    void removeNote(Note *note);
    Note *findNoteById(int id) const;
    void notifyNoteChanged(NoteChangeType type, const QList<Note *> &notes);
    void notifyParamChanged(ParamInfo::Name name, Param::Type type);
    const PieceList &pieces() const;
    void removeAllPieces();
    ReSegmentResult reSegment(const Timeline &timeline, bool bumpRevision = true);
    void updateOriginalParam(ParamInfo::Name name);
    InferPiece *findPieceById(int id) const;
    PieceList findPiecesByNotes(const QList<Note *> &notes) const;
    quint64 inferenceRevision() const;
    quint64 bumpInferenceRevision();

    // Route language an unspecified note resolves to under the current singer.
    QString fallbackLanguage() const;

    // Transient diagnostics of the last segmentation/inference cycle, never
    // serialized nor undoable
    const QHash<int, NoteInferenceErrorInfo> &noteInferenceErrors() const;
    const QList<QPair<int, int>> &skippedPhraseRanges() const;
    // Task-level failure messages to merge into the diagnostics on the next
    // reSegment; set by the inference automation bridge right before it
    void setPendingNoteTaskErrors(const QHash<int, QString> &taskErrors);

    SingerInfo singerInfo() const;
    /// The voice source inherited from the track, independent of the current
    /// inheritance state.
    SingerInfo trackSingerInfo() const;
    SingerInfo ownSingerInfo() const;
    SpeakerInfo speakerInfo() const;
    SpeakerInfo ownSpeakerInfo() const;
    bool usesTrackVoiceContext() const;
    SpeakerMixData speakerMixData() const;
    SpeakerMixData ownSpeakerMixData() const;
    SpeakerMixData trackSpeakerMixData() const;
    EffectiveVoiceContext effectiveVoiceContext() const;
    void setSpeakerMixData(const SpeakerMixData &data);
    void setOwnSpeakerMixData(const SpeakerMixData &data);
    void setTrackSpeakerMixData(const SpeakerMixData &data);
    void resetSpeakerMixToSingle();
    void setTrackVoiceContext(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                              const SpeakerMixData &speakerMixData);
    void setOwnVoiceContext(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo,
                            const SpeakerMixData &speakerMixData);
    void selectOwnSingleSpeaker(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo);
    void useTrackVoiceContext();
    void setTrackSingerAndSpeakerInfo(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo);
    void setOwnSingerAndSpeaker(const SingerInfo &singerInfo, const SpeakerInfo &speakerInfo);
    void useTrackSingerAndSpeaker();

    QString speakerId() const;
    SingerIdentifier singerIdentifier() const;

    ParamInfo params;

signals:
    void voiceContextChanged(const VoiceContextChange &change);
    void noteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes);
    void paramChanged(ParamInfo::Name name, Param::Type type);
    void piecesChanged(const PieceList &pieces, const PieceList &newPieces,
                       const PieceList &discardedPieces);
    void noteInferenceErrorsChanged();

private:
    void notifyEffectiveVoiceContextChanged(const EffectiveVoiceContext &oldContext);
    void rebuildSliceErrors(const SliceResult &sliceResult);
    // Recomputes the language and phoneme diagnostics from the current notes and
    // singer, then merges every source into noteInferenceErrors(). Skipped phrase
    // ranges are reported through the same signal, so a range-only change has to
    // ask for the notification explicitly.
    void refreshNoteDiagnostics(bool rangesChanged = false);
    void clearNoteInferenceErrors(const QList<Note *> &notes);
    void clearAllNoteInferenceErrors();

    OverlappableSerialList<Note> m_notes;
    PieceList m_pieces;
    quint64 m_inferenceRevision = 0;

    QHash<int, NoteInferenceErrorInfo> m_noteInferenceErrors;
    QHash<int, NoteInferenceErrorInfo> m_sliceErrors;
    QHash<int, NoteInferenceErrorInfo> m_languageErrors;
    QHash<int, NoteInferenceErrorInfo> m_phonemeErrors;
    QList<QPair<int, int>> m_skippedPhraseRanges;
    QHash<int, QString> m_pendingNoteTaskErrors;

    Property<SingerInfo> m_singerInfo;
    Property<SingerInfo> m_trackSingerInfo;

    Property<SpeakerInfo> m_speakerInfo;
    Property<SpeakerInfo> m_trackSpeakerInfo;
    SpeakerMixData m_ownSpeakerMixData;
    SpeakerMixData m_trackSpeakerMixData;
    Property<bool> m_useTrackSingerInfo{true};
};

#endif // SINGINGCLIP_H
