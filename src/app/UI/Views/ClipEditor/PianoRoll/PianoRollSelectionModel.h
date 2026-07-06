#ifndef PIANOROLLSELECTIONMODEL_H
#define PIANOROLLSELECTIONMODEL_H

#include <QList>
#include <QObject>
#include <QHash>
#include <functional>

class NoteView;
class Note;
class PianoRollGraphicsView;

class PianoRollSelectionModel : public QObject {
    Q_OBJECT

public:
    explicit PianoRollSelectionModel(PianoRollGraphicsView *view,
                                     QList<NoteView *> &noteViews,
                                     QHash<int, NoteView *> &noteViewIndex,
                                     QList<Note *> &notes,
                                     QObject *parent = nullptr);

    [[nodiscard]] QList<NoteView *> selectedNoteItems() const;
    [[nodiscard]] bool isSelecting() const { return m_selecting; }
    void setSelecting(bool on) { m_selecting = on; }

    [[nodiscard]] bool selectionChangeBarrier() const { return m_selectionChangeBarrier; }
    void setSelectionChangeBarrier(bool on) { m_selectionChangeBarrier = on; }

    void updateSceneSelectionState();
    void updateOverlappedState();

    // Paste preview
    [[nodiscard]] QList<NoteView *> pastePreviewViews() const { return m_pastePreviewViews; }
    void setPastePreviewViews(const QList<NoteView *> &views) { m_pastePreviewViews = views; }
    void clearPastePreviewViews();

    [[nodiscard]] QList<NoteView *> noteViewsToErase() const { return m_noteViewsToErase; }
    void setNoteViewsToErase(const QList<NoteView *> &views) { m_noteViewsToErase = views; }
    void clearNoteViewsToErase() { m_noteViewsToErase.clear(); }
    void appendNoteViewToErase(NoteView *view) { m_noteViewsToErase.append(view); }

signals:
    void selectionChanged();

private:
    bool m_selecting = false;
    bool m_selectionChangeBarrier = false;
    QList<NoteView *> m_pastePreviewViews;
    QList<NoteView *> m_noteViewsToErase;

    // References held from PianoRollGraphicsViewPrivate
    QList<NoteView *> &m_noteViews;
    QHash<int, NoteView *> &m_noteViewIndex;
    QList<Note *> &m_notes;
    PianoRollGraphicsView *m_view = nullptr;
};

#endif // PIANOROLLSELECTIONMODEL_H