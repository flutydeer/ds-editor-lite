//
// Created by fluty on 2024/1/23.
//

#ifndef PIANOROLLGRAPHICSVIEW_H
#define PIANOROLLGRAPHICSVIEW_H

#include "Interface/IAtomicAction.h"

#include "UI/Views/ClipEditor/ClipEditorGlobal.h"
#include "UI/Views/Common/TimeGraphicsView.h"

class SingingClip;
class DrawCurve;
class Note;
class PianoRollGraphicsScene;
class CommonParamEditorView;
class NoteView;

using namespace ClipEditorGlobal;

class PianoRollGraphicsViewPrivate;

class PianoRollGraphicsView final : public TimeGraphicsView, public IAtomicAction {
    Q_OBJECT
    Q_PROPERTY(int noteFontPixelSize READ noteFontPixelSize WRITE setNoteFontPixelSize)
    Q_PROPERTY(QColor whiteKeyColor READ whiteKeyColor WRITE setWhiteKeyColor)
    Q_PROPERTY(QColor blackKeyColor READ blackKeyColor WRITE setBlackKeyColor)
    Q_PROPERTY(QColor octaveDividerColor READ octaveDividerColor WRITE setOctaveDividerColor)

public:
    explicit PianoRollGraphicsView(PianoRollGraphicsScene *scene, const QWidget *parent = nullptr);
    ~PianoRollGraphicsView() override;
    void setDataContext(SingingClip *clip);
    void setEditMode(PianoRollEditMode mode);
    void reset();
    QList<int> selectedNotesId() const;
    void clearNoteSelections(const NoteView *except = nullptr);

    void discardAction() override;
    void commitAction() override;

    double topKeyIndex() const;
    double bottomKeyIndex() const;
    void setViewportCenterAt(double tick, double keyIndex);
    void setViewportCenterAtKeyIndex(double keyIndex);

signals:
    void keyRangeChanged(double start, double end);

public slots:
    void onSceneSelectionChanged() const;

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
    int m_noteFontPixelSize = 13;

    int noteFontPixelSize() const;
    void setNoteFontPixelSize(int size);
    QColor whiteKeyColor() const;
    void setWhiteKeyColor(const QColor &color);
    QColor blackKeyColor() const;
    void setBlackKeyColor(const QColor &color);
    QColor octaveDividerColor() const;
    void setOctaveDividerColor(const QColor &color);

    Q_DECLARE_PRIVATE(PianoRollGraphicsView)
    PianoRollGraphicsViewPrivate *d_ptr;
};

#endif // PIANOROLLGRAPHICSVIEW_H
