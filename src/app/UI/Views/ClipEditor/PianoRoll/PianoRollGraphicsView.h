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
class QShowEvent;

class PianoRollGraphicsView final : public TimeGraphicsView, public IAtomicAction {
    Q_OBJECT
    Q_PROPERTY(int noteFontPixelSize READ noteFontPixelSize WRITE setNoteFontPixelSize)
    Q_PROPERTY(QColor whiteKeyColor READ whiteKeyColor WRITE setWhiteKeyColor)
    Q_PROPERTY(QColor blackKeyColor READ blackKeyColor WRITE setBlackKeyColor)
    Q_PROPERTY(QColor octaveDividerColor READ octaveDividerColor WRITE setOctaveDividerColor)
    Q_PROPERTY(QColor noteSelectedBorderColor READ noteSelectedBorderColor WRITE
                   setNoteSelectedBorderColor)
    Q_PROPERTY(QColor pronunciationTextColor READ pronunciationTextColor WRITE
                   setPronunciationTextColor)
    Q_PROPERTY(QColor anchorColor READ anchorColor WRITE setAnchorColor)
    Q_PROPERTY(QColor anchorSelectedColor READ anchorSelectedColor WRITE setAnchorSelectedColor)
    Q_PROPERTY(QColor anchorCurveColor READ anchorCurveColor WRITE setAnchorCurveColor)
    Q_PROPERTY(QColor anchorPreviewColor READ anchorPreviewColor WRITE setAnchorPreviewColor)
    Q_PROPERTY(
        QColor clipRangeOverlayColor READ clipRangeOverlayColor WRITE setClipRangeOverlayColor)
    Q_PROPERTY(QColor splitLineColor READ splitLineColor WRITE setSplitLineColor)
    Q_PROPERTY(QColor paramGraduateColor READ paramGraduateColor WRITE setParamGraduateColor)
    Q_PROPERTY(QColor paramOriginalCurveColor READ paramOriginalCurveColor WRITE
                   setParamOriginalCurveColor)
    Q_PROPERTY(
        QColor paramEditedCurveColor READ paramEditedCurveColor WRITE setParamEditedCurveColor)
    Q_PROPERTY(QColor paramBackgroundLayerColor READ paramBackgroundLayerColor WRITE
                   setParamBackgroundLayerColor)

public:
    explicit PianoRollGraphicsView(PianoRollGraphicsScene *scene, const QWidget *parent = nullptr);
    ~PianoRollGraphicsView() override;
    void setDataContext(SingingClip *clip);
    void setEditMode(PianoRollEditMode mode);
    void reset();
    [[nodiscard]] QList<int> selectedNotesId() const;
    void clearNoteSelections(const NoteView *except = nullptr);

    void discardAction() override;
    void commitAction() override;

    [[nodiscard]] double topKeyIndex() const;
    [[nodiscard]] double bottomKeyIndex() const;
    void setViewportCenterAt(double tick, double keyIndex);
    void setViewportCenterAtKeyIndex(double keyIndex);

    [[nodiscard]] double sceneXToTick(double pos) const {
        return TimeGraphicsView::sceneXToTick(pos);
    }

    [[nodiscard]] double tickToSceneX(double tick) const {
        return TimeGraphicsView::tickToSceneX(tick);
    }

signals:
    void keyRangeChanged(double start, double end);
    void keyHovered(int keyIndex);
    void keyHoverCleared();

public slots:
    void onSceneSelectionChanged() const;

private slots:
    void notifyKeyRangeChanged();

protected:
    void showEvent(QShowEvent *event) override;
    bool event(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void onEdgeAutoScrollFrame(const QPoint &clampedViewportPos,
                               Qt::KeyboardModifiers modifiers) override;

private:
    int m_noteFontPixelSize = 13;

    void updateNoteDragAt(const QPoint &viewportPos, Qt::KeyboardModifiers modifiers);

    [[nodiscard]] int noteFontPixelSize() const;
    void setNoteFontPixelSize(int size);
    QColor whiteKeyColor() const;
    void setWhiteKeyColor(const QColor &color);
    QColor blackKeyColor() const;
    void setBlackKeyColor(const QColor &color);
    QColor octaveDividerColor() const;
    void setOctaveDividerColor(const QColor &color);
    QColor noteSelectedBorderColor() const;
    void setNoteSelectedBorderColor(const QColor &color);
    QColor pronunciationTextColor() const;
    void setPronunciationTextColor(const QColor &color);
    QColor anchorColor() const;
    void setAnchorColor(const QColor &color);
    QColor anchorSelectedColor() const;
    void setAnchorSelectedColor(const QColor &color);
    QColor anchorCurveColor() const;
    void setAnchorCurveColor(const QColor &color);
    QColor anchorPreviewColor() const;
    void setAnchorPreviewColor(const QColor &color);
    QColor clipRangeOverlayColor() const;
    void setClipRangeOverlayColor(const QColor &color);
    QColor splitLineColor() const;
    void setSplitLineColor(const QColor &color);
    QColor paramGraduateColor() const;
    void setParamGraduateColor(const QColor &color);
    QColor paramOriginalCurveColor() const;
    void setParamOriginalCurveColor(const QColor &color);
    QColor paramEditedCurveColor() const;
    void setParamEditedCurveColor(const QColor &color);
    QColor paramBackgroundLayerColor() const;
    void setParamBackgroundLayerColor(const QColor &color);

    Q_DECLARE_PRIVATE(PianoRollGraphicsView)
    PianoRollGraphicsViewPrivate *d_ptr;
};

#endif // PIANOROLLGRAPHICSVIEW_H
