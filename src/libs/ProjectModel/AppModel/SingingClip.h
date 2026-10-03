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

    void setDefaultLanguage(const QString &language);
    QString defaultLanguage() const;
    // effective language usable at inference: explicit clip language > singer package default >
    // defaultLanguage() fallback
    QString effectiveDefaultLanguage() const;
    QString defaultG2pId() const;

    // Transient diagnostics of the last segmentation/inference cycle, never
    // serialized nor undoable
    const QHash<int, NoteInferenceErrorInfo> &noteInferenceErrors() const;
    const QList<QPair<int, int>> &skippedPhraseRanges() const;
    // Task-level failure messages to merge into the diagnostics on the next
    // reSegment; set by the inference automation bridge right before it
    void setPendingNoteTaskErrors(const QHash<int, QString> &taskErrors);

    SingerInfo singerInfo() const;
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
    void defaultLanguageChanged(QString language);
    void defaultG2pIdChanged(QString g2pId);
    void piecesChanged(const PieceList &pieces, const PieceList &newPieces,
                       const PieceList &discardedPieces);
    void noteInferenceErrorsChanged();

private:
    void init();
    void updateDefaultG2pId(const QString &language);
    void notifyEffectiveVoiceContextChanged(const EffectiveVoiceContext &oldContext);
    void rebuildNoteInferenceErrors(const SliceResult &sliceResult);
    void clearNoteInferenceErrors(const QList<Note *> &notes);
    void clearAllNoteInferenceErrors();

    OverlappableSerialList<Note> m_notes;
    PieceList m_pieces;
    quint64 m_inferenceRevision = 0;

    QHash<int, NoteInferenceErrorInfo> m_noteInferenceErrors;
    QList<QPair<int, int>> m_skippedPhraseRanges;
    QHash<int, QString> m_pendingNoteTaskErrors;

    Property<QString> m_defaultLanguage{"unknown"};
    Property<QString> m_defaultG2pId{"unknown"};

    Property<SingerInfo> m_singerInfo;
    Property<SingerInfo> m_trackSingerInfo;

    Property<SpeakerInfo> m_speakerInfo;
    Property<SpeakerInfo> m_trackSpeakerInfo;
    SpeakerMixData m_ownSpeakerMixData;
    SpeakerMixData m_trackSpeakerMixData;
    Property<bool> m_useTrackSingerInfo{true};
};

#endif // SINGINGCLIP_H
