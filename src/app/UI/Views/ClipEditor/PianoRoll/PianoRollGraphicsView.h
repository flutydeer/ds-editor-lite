//
// Created by fluty on 2024/1/23.
//

#ifndef PIANOROLLGRAPHICSVIEW_H
#define PIANOROLLGRAPHICSVIEW_H

#include "Global/ClipEditorGlobal.h"
#include "NoteLayer.h"
#include "Model/AppModel/Clip.h"
#include "Model/AppModel/Params.h"
#include "UI/Views/Common/GraphicsLayerManager.h"
#include "UI/Views/Common/TimeGraphicsView.h"

class CMenu;
class Note;
class PianoRollGraphicsScene;
class CommonParamEditorView;
class NoteView;

using namespace ClipEditorGlobal;

class PianoRollGraphicsViewPrivate;

class PianoRollGraphicsView final : public TimeGraphicsView {
    Q_OBJECT

public:
    explicit PianoRollGraphicsView(PianoRollGraphicsScene *scene, QWidget *parent = nullptr);
    ~PianoRollGraphicsView() override;
    void setDataContext(SingingClip *clip);
    void setEditMode(PianoRollEditMode mode);
    void reset();
    [[nodiscard]] QList<int> selectedNotesId() const;
    void clearNoteSelections(NoteView *except = nullptr);

    [[nodiscard]] double topKeyIndex() const;
    [[nodiscard]] double bottomKeyIndex() const;
    void setViewportCenterAt(double tick, double keyIndex);
    void setViewportCenterAtKeyIndex(double keyIndex);

signals:
    void keyRangeChanged(double start, double end);

public slots:
    void onEditModeChanged(ClipEditorGlobal::PianoRollEditMode mode);
    void onSceneSelectionChanged() const;
    void onPitchEditorEditCompleted(const QList<DrawCurve *> &curves);

private slots:
    void notifyKeyRangeChanged();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    Q_DECLARE_PRIVATE(PianoRollGraphicsView)
    PianoRollGraphicsViewPrivate *d_ptr;
};

#endif // PIANOROLLGRAPHICSVIEW_H
