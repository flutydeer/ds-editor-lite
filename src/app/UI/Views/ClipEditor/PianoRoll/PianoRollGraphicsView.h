//
// Created by fluty on 2024/1/23.
//

#ifndef PIANOROLLGRAPHICSVIEW_H
#define PIANOROLLGRAPHICSVIEW_H

#include "NoteLayer.h"
#include "Interface/IAtomicAction.h"
#include "Model/AppModel/Clip.h"
#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/GraphicsLayerManager.h"
#include "UI/Views/Common/TimeGraphicsView.h"

class CMenu;
class Note;
class PianoRollGraphicsScene;
class CommonParamEditorView;
class NoteView;

using namespace ClipEditorGlobal;

class PianoRollGraphicsViewPrivate;

class PianoRollGraphicsView final : public TimeGraphicsView, public IAtomicAction {
    Q_OBJECT

public:
    explicit PianoRollGraphicsView(PianoRollGraphicsScene *scene, QWidget *parent = nullptr);
    ~PianoRollGraphicsView() override;
    void setDataContext(SingingClip *clip);
    void setEditMode(PianoRollEditMode mode);
    void reset();
    [[nodiscard]] QList<int> selectedNotesId() const;
    void clearNoteSelections(NoteView *except = nullptr);

    void cancelAction() override;
    void commitAction() override;

    [[nodiscard]] double topKeyIndex() const;
    [[nodiscard]] double bottomKeyIndex() const;
    void setViewportCenterAt(double tick, double keyIndex);
    void setViewportCenterAtKeyIndex(double keyIndex);

signals:
    void keyRangeChanged(double start, double end);

public slots:
    void onSceneSelectionChanged() const;
    void onPitchEditorEditCompleted(const QList<DrawCurve *> &curves);

private slots:
    void notifyKeyRangeChanged();

protected:
    bool event(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    Q_DECLARE_PRIVATE(PianoRollGraphicsView)
    PianoRollGraphicsViewPrivate *d_ptr;
};

#endif // PIANOROLLGRAPHICSVIEW_H
