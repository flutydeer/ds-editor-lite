//
// Created by fluty on 24-8-4.
//

#ifndef PIANOROLLGRAPHICSVIEW_P_H
#define PIANOROLLGRAPHICSVIEW_P_H

#include "Model/AppModel/Params.h"
#include "Model/AppModel/SingingClip.h"

#include <QHash>
#include <QObject>
#include <QPointF>
#include <QPointer>
#include <QColor>

class ClipRangeOverlay;
class PitchAnchorEditorView;
class PitchEditorView;
class QMouseEvent;
class QHoverEvent;
class QAction;
class QPaintEvent;
class Menu;
class NoteView;
class Note;
class PianoRollGraphicsView;
class PronunciationView;
class PianoRollBackground;
class PianoRollEditHandler;
class PianoRollSelectionModel;
class NoteInteractionController;
class InlineTextEditOverlay;
enum class EditSessionEndReason;

using namespace ClipEditorGlobal;

class PianoRollGraphicsViewPrivate : public QObject {
    Q_OBJECT
    Q_DECLARE_PUBLIC(PianoRollGraphicsView)

public:
    enum class InlineEditField { None, Lyric, Pronunciation };

    explicit PianoRollGraphicsViewPrivate(PianoRollGraphicsView *p) : q_ptr(p) {};
    bool eventFilter(QObject *watched, QEvent *event) override;

    SingingClip *m_clip = nullptr;
    int m_offset = 0;
    QList<Note *> m_notes;

    PianoRollBackground *m_gridItem = nullptr;
    QList<NoteView *> noteViews;
    QHash<int, NoteView *> noteViewIndex;

    PitchEditorView *m_pitchEditor = nullptr;
    bool m_pitchEditSessionActive = false;
    quint64 m_pitchEditSessionId = 0;
    PitchAnchorEditorView *m_anchorEditor = nullptr;
    ClipRangeOverlay *m_clipRangeOverlay = nullptr;
    // Applied to the lazily-created SplitLineIndicator on each tool activation
    QColor m_splitLineColor = {255, 100, 100};

    PianoRollEditHandler *m_currentHandler = nullptr;
    QHash<PianoRollEditMode, PianoRollEditHandler *> m_handlers;
    PianoRollSelectionModel *m_selectionModel = nullptr;
    NoteInteractionController *m_interactionController = nullptr;
    InlineTextEditOverlay *m_inlineEditor = nullptr;
    InlineEditField m_inlineEditField = InlineEditField::None;
    int m_inlineEditingNoteId = -1;
    QAction *m_pasteAction = nullptr;
    void restoreHandler();

    PianoRollEditMode m_editMode = Select;

    bool m_mouseDown = false;
    Qt::MouseButton m_mouseDownButton = Qt::NoButton;
    bool m_isEditPitchMode = false;
    bool m_initialViewportPositionPending = false;

    void moveToNullClipState();
    void moveToSingingClipState(SingingClip *clip);
    void positionViewportAtClipContent();
    void endPitchEditSession(EditSessionEndReason reason);

    void updateNoteTimeAndKey(const Note *note) const;
    void updateNoteWord(const Note *note) const;

    void updatePitch(Param::Type paramType, const Param &param) const;

    void setPitchEditMode(bool on, bool isErase);
    [[nodiscard]] NoteView *noteViewAt(const QPoint &pos);
    [[nodiscard]] PronunciationView *pronViewAt(const QPoint &pos);
    [[nodiscard]] NoteView *findNoteViewById(int id) const;

    void handleNoteInserted(Note *note);
    void handleNoteRemoved(Note *note);
    void addNoteViewToScene(NoteView *view);
    void removeNoteViewFromScene(NoteView *view);

    void onHoverEnter(QHoverEvent *event);
    void onHoverLeave(QHoverEvent *event);
    void onHoverMove(const QHoverEvent *event);

public slots:
    void onClipPropertyChanged();
    void onNoteChanged(SingingClip::NoteChangeType type, const QList<Note *> &notes);
    void onNoteSelectionChanged();
    void onParamChanged(ParamInfo::Name name, Param::Type type) const;

    void onDeleteSelectedNotes() const;
    void onOpenPhonemeEditor(int noteId);
    void onStartEditingNoteLyric(NoteView *noteView);
    void finishInlineEditing();
    void onInlineTextSubmitted(const QString &text);
    void onInlineNavigationRequested(const QString &text, bool backwards);
    void onInlineEditCancelled();
    void applyLyricEdit(int noteId, const QString &text);
    void applyPronunciationEdit(int noteId, const QString &text);
    NoteView *findAdjacentNoteView(NoteView *currentNoteView, bool backwards) const;

    void onStartEditingPronunciation(PronunciationView *pronView);

private:
    PianoRollGraphicsView *q_ptr;
};

#endif // PIANOROLLGRAPHICSVIEW_P_H
