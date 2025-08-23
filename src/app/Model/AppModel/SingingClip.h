//
// Created by fluty on 24-9-18.
//

#ifndef SINGINGCLIP_H
#define SINGINGCLIP_H

#include "Clip.h"
#include "Params.h"
#include "Global/AppGlobal.h"
#include "Utils/Property.h"
#include "Modules/Inference/Models/SingerIdentifier.h"

class DrawCurve;
class InferPiece;
class Note;

using PieceList = QList<InferPiece *>;

class SingingClip final : public Clip {
    Q_OBJECT

public:
    enum NoteChangeType {
        Insert,
        Remove,
        TimeKeyPropertyChange,
        OriginalWordPropertyChange,
        EditedWordPropertyChange,
        EditedPhonemeOffsetChange
    };

    explicit SingingClip();
    explicit SingingClip(const QList<Note *> &notes);
    ~SingingClip() override;

    [[nodiscard]] ClipType clipType() const override;
    [[nodiscard]] const OverlappableSerialList<Note> &notes() const;
    Property<QString> defaultLanguage{"unknown"};
    Property<QString> defaultG2pId{"unknown"};
    ParamInfo params;
    Property<SingerIdentifier> singerIdentifier;
    Property<SingerIdentifier> trackSingerIdentifier;
    Property<bool> useTrackSingerIdentifier{true};
    Property<QString> speaker;
    Property<QString> trackSpeaker;
    Property<bool> useTrackSpeaker{true};

    void insertNote(Note *note);
    void insertNotes(const QList<Note *> &notes);
    void removeNote(Note *note);
    [[nodiscard]] Note *findNoteById(int id) const;
    void notifyNoteChanged(NoteChangeType type, const QList<Note *> &notes);
    void notifyParamChanged(ParamInfo::Name name, Param::Type type);
    [[nodiscard]] const PieceList &pieces() const;
    void reSegment();
    void updateOriginalParam(ParamInfo::Name name);
    [[nodiscard]] InferPiece *findPieceById(int id) const;
    [[nodiscard]] PieceList findPiecesByNotes(const QList<Note *> &notes) const;
    const Property<SingerIdentifier> &getSingerIdentifier() const;
    QString getSpeaker() const;

signals:
    void identifierChanged(const SingerIdentifier &identifier);
    void speakerChanged(const QString &speaker);
    void noteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes);
    void paramChanged(ParamInfo::Name name, Param::Type type);
    void defaultLanguageChanged(QString language);
    void defaultG2pIdChanged(QString g2pId);
    void piecesChanged(const PieceList &pieces, const PieceList &newPieces,
                       const PieceList &discardedPieces);

private:
    void init();
    OverlappableSerialList<Note> m_notes;
    PieceList m_pieces;
};

#endif // SINGINGCLIP_H
