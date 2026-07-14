//
// Created by fluty on 24-9-18.
//

#ifndef SINGINGCLIP_H
#define SINGINGCLIP_H

#include "Clip.h"
#include "EffectiveVoiceContext.h"
#include "Params.h"
#include "SpeakerMixData.h"
#include "Global/AppGlobal.h"
#include "Utils/Property.h"
#include "Modules/Inference/Models/SingerIdentifier.h"
#include "Modules/PackageManager/Models/SingerInfo.h"
#include "Modules/PackageManager/Models/SpeakerInfo.h"

class DrawCurve;
class InferPiece;
class Note;

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
    ReSegmentResult reSegment();
    void updateOriginalParam(ParamInfo::Name name);
    InferPiece *findPieceById(int id) const;
    PieceList findPiecesByNotes(const QList<Note *> &notes) const;
    quint64 inferenceRevision() const;
    quint64 bumpInferenceRevision();

    void setDefaultLanguage(const QString &language);
    QString defaultLanguage() const;
    QString defaultG2pId() const;

    SingerInfo singerInfo() const;
    SingerInfo ownSingerInfo() const;
    SpeakerInfo speakerInfo() const;
    SpeakerInfo ownSpeakerInfo() const;
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

    Property<bool> useTrackSingerInfo{true};

signals:
    void singerOrSpeakerChanged();
    void speakerMixChanged(const SpeakerMixData &data);
    void noteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes);
    void paramChanged(ParamInfo::Name name, Param::Type type);
    void defaultLanguageChanged(QString language);
    void defaultG2pIdChanged(QString g2pId);
    void piecesChanged(const PieceList &pieces, const PieceList &newPieces,
                       const PieceList &discardedPieces);

private:
    void init();
    void updateDefaultG2pId(const QString &language);
    void notifyEffectiveVoiceContextChanged(const EffectiveVoiceContext &oldContext);

    OverlappableSerialList<Note> m_notes;
    PieceList m_pieces;
    quint64 m_inferenceRevision = 0;

    Property<QString> m_defaultLanguage{"unknown"};
    Property<QString> m_defaultG2pId{"unknown"};

    Property<SingerInfo> m_singerInfo;
    Property<SingerInfo> m_trackSingerInfo;

    Property<SpeakerInfo> m_speakerInfo;
    Property<SpeakerInfo> m_trackSpeakerInfo;
    SpeakerMixData m_ownSpeakerMixData;
    SpeakerMixData m_trackSpeakerMixData;
    bool m_singerSpeakerBatching = false;
};

#endif // SINGINGCLIP_H
