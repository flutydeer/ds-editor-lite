//
// Created by fluty on 24-9-18.
//

#ifndef SINGINGCLIP_H
#define SINGINGCLIP_H

#include "Clip.h"
#include "Params.h"
#include "Global/AppGlobal.h"
#include "Utils/Property.h"

class DrawCurve;
class InferPiece;
class Note;

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
    ~SingingClip() override;

    [[nodiscard]] ClipType clipType() const override;
    [[nodiscard]] const OverlappableSerialList<Note> &notes() const;
    Property<AppGlobal::LanguageType> defaultLanguage = AppGlobal::unknown;
    ParamInfo params;
    Property<QString> configPath;

    void insertNote(Note *note);
    void removeNote(Note *note);
    [[nodiscard]] Note *findNoteById(int id) const;
    void notifyNoteChanged(NoteChangeType type, const QList<Note *> &notes);
    void notifyParamChanged(ParamInfo::Name name, Param::Type type);
    [[nodiscard]] const QList<InferPiece *> &pieces() const;
    void reSegment();
    void updateOriginalParam(ParamInfo::Name name);
    [[nodiscard]] InferPiece *findPieceById(int id) const;
    [[nodiscard]] QList<InferPiece *> findPiecesByNotes(const QList<Note *> &notes) const;

    static void copyCurves(const QList<Curve *> &source, QList<Curve *> &target);
    static void copyCurves(const QList<DrawCurve *> &source, QList<DrawCurve *> &target);

signals:
    void configPathChanged(QString path);
    void noteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes);
    void paramChanged(ParamInfo::Name name, Param::Type type);
    void defaultLanguageChanged(AppGlobal::LanguageType language);
    void piecesChanged(const QList<InferPiece *> &pieces);

private:
    OverlappableSerialList<Note> m_notes;
    QList<InferPiece *> m_pieces;
};

#endif // SINGINGCLIP_H
