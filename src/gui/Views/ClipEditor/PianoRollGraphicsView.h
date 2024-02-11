//
// Created by fluty on 2024/1/23.
//

#ifndef PIANOROLLGRAPHICSVIEW_H
#define PIANOROLLGRAPHICSVIEW_H

#include "PianoRollGlobal.h"
#include "GraphicsItem/NoteGraphicsItem.h"
#include "Model/Note.h"
#include "Views/Common/TimeGraphicsView.h"

using namespace PianoRollGlobal;

class PianoRollGraphicsView final : public TimeGraphicsView {
    Q_OBJECT

public:
    explicit PianoRollGraphicsView(PianoRollGraphicsScene *scene);
    void setIsSingingClip(bool isSingingClip);
    void setEditMode(PianoRollEditMode mode);
    void insertNote(Note *dsNote);
    void removeNote(int noteId);
    void updateNote(Note *note);
    void reset();
    QList<int> selectedNotesId() const;

    double topKeyIndex()const;
    double bottomKeyIndex() const;
    void setViewportTopKey(double key);
    void setViewportCenterAt(double tick, double keyIndex);
    void setViewportCenterAtKeyIndex(double keyIndex);

signals:
    void noteShapeEdited(NoteEditMode mode, int deltaTick, int deltaKey);
    void removeNoteTriggered();
    void editNoteLyricTriggered();

private:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

    bool m_isSingingClipSelected = false;
    PianoRollEditMode m_mode = Select;
    QList<NoteGraphicsItem *> m_noteItems;

    NoteGraphicsItem *findNoteById(int id);
    double keyIndexToSceneY(double index) const;
    double sceneYToKeyIndex(double y) const;
};

#endif // PIANOROLLGRAPHICSVIEW_H
