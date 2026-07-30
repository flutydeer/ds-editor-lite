#ifndef RHIPIANOROLLCANVAS_H
#define RHIPIANOROLLCANVAS_H

#include "IPianoRollCanvas.h"

#include "UI/Views/EditorCanvas/RenderUpdateScheduler.h"

#include <QPointer>
#include <QSet>

#include <utility>

class RhiEditorCanvasWidget;
class InlineTextEditOverlay;
class Note;

class RhiPianoRollCanvas final : public IPianoRollCanvas {
    Q_OBJECT

public:
    explicit RhiPianoRollCanvas(QObject *parent = nullptr);
    ~RhiPianoRollCanvas() override;

    [[nodiscard]] EditorCanvasBackend backend() const override;
    [[nodiscard]] QWidget *widget() const override;
    [[nodiscard]] QScrollBar *horizontalScrollBar() const override;
    [[nodiscard]] QScrollBar *verticalScrollBar() const override;
    void setDataContext(SingingClip *clip) override;
    void setEditMode(ClipEditorGlobal::PianoRollEditMode mode) override;
    void setTrackColorIndex(int index) override;
    [[nodiscard]] EditorViewportState viewportState() const override;
    void restoreViewportState(const EditorViewportState &state) override;
    bool centerAt(double tick, double keyIndex) override;
    bool setViewportScale(double horizontalScale, double verticalScale) override;
    [[nodiscard]] double startTick() const override;
    [[nodiscard]] double endTick() const override;
    [[nodiscard]] double centerKeyIndex() const override;
    [[nodiscard]] double scaleX() const override;
    [[nodiscard]] double scaleY() const override;
    [[nodiscard]] int horizontalBarValue() const override;
    void setPlaybackPosition(double tick) override;
    void setLastPlaybackPosition(double tick) override;
    void refreshSnapshot(EditorDirtyDomains domains) override;
    [[nodiscard]] HistoryFocusVisibility focusVisibility(const HistoryFocus &focus) const override;
    bool revealFocus(const HistoryFocus &focus, bool animated) override;

public slots:
    void onWheelHorScale(QWheelEvent *event) override;
    void onWheelVerScale(QWheelEvent *event) override;
    void onWheelHorScroll(QWheelEvent *event) override;
    void onWheelVerScroll(QWheelEvent *event) override;

private:
    struct AnchorSelection {
        int curveIndex = -1;
        int nodeIndex = -1;
        int startTick = 0;
        int startValue = 0;
    };

    enum class Interaction {
        None,
        Move,
        ResizeLeft,
        ResizeRight,
        Draw,
        Erase,
        RectSelect,
        DrawPitch,
        ErasePitch,
        MoveAnchor,
    };

    ImmutableEditorRenderSnapshot buildSnapshot();
    void publishSnapshot(EditorDirtyDomains domains);
    void ensureVisibleSnapshot();
    void onPointerPressed(const QPointF &position, const EditorHitResult &hit,
                          Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void onPointerMoved(const QPointF &position, const EditorHitResult &hit,
                        Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers);
    void onPointerReleased(const QPointF &position, const EditorHitResult &hit,
                           Qt::MouseButton button, Qt::KeyboardModifiers modifiers);
    void onPointerDoubleClicked(const QPointF &position, const EditorHitResult &hit,
                                Qt::MouseButton button);
    void showContextMenu(const QPointF &position, const EditorHitResult &hit,
                         const QPoint &globalPosition);
    void onKeyPressed(int key, Qt::KeyboardModifiers modifiers);
    void cancelInteraction();
    void autoScrollFor(const QPointF &logicalPosition);
    void beginPitchTransaction();
    void applyPitchStroke(bool erase);
    void startLyricEditing(int noteId);
    void startPronunciationEditing(int noteId);
    void finishInlineEditing(const QString &text);
    void openPhonemeEditor(int noteId);
    void clearPastePreview();
    [[nodiscard]] std::pair<int, int> anchorAt(const QPointF &position) const;
    [[nodiscard]] bool isAnchorSelected(int curveIndex, int nodeIndex) const;
    void selectAnchor(int curveIndex, int nodeIndex);
    void selectAnchorsInRect(const QRectF &rect, bool additive);
    void updateAnchorMergeCandidate(const QPointF &position);
    void commitAnchorMove(bool removeSelected = false);
    void mergeAnchorCurves(int sourceCurveIndex, int targetCurveIndex);
    void setSelectedAnchorInterpolation(int interpolationMode);
    void insertAnchorAt(const QPointF &position);
    void beginEditTransaction(const QList<int> &noteIds);
    void finishEditTransaction(bool commit);
    void positionViewportAtClipContent();
    [[nodiscard]] Note *findNote(int id) const;
    [[nodiscard]] int localTickAt(double x) const;
    [[nodiscard]] int keyAt(double y) const;
    [[nodiscard]] int snappedLocalTick(int tick, bool nearest = false) const;
    [[nodiscard]] int clipOffset() const;

    RhiEditorCanvasWidget *m_widget = nullptr;
    RenderUpdateScheduler m_scheduler;
    QPointer<SingingClip> m_clip;
    ClipEditorGlobal::PianoRollEditMode m_editMode = ClipEditorGlobal::Select;
    int m_trackColorIndex = 0;
    quint64 m_revision = 0;
    Interaction m_interaction = Interaction::None;
    QPointF m_pressPosition;
    QPointF m_lastPosition;
    QRectF m_selectionRect;
    int m_pressedNoteId = -1;
    int m_hoveredNoteId = -1;
    int m_hoveredKey = -1;
    bool m_pointerInside = false;
    int m_originalStart = 0;
    int m_originalLength = 0;
    int m_originalKey = 60;
    int m_previewDeltaTick = 0;
    int m_previewDeltaKey = 0;
    int m_drawStart = 0;
    int m_drawLength = 0;
    int m_drawKey = 60;
    QSet<int> m_eraseNoteIds;
    bool m_editTransactionActive = false;
    InlineTextEditOverlay *m_inlineEditor = nullptr;
    int m_inlineEditingNoteId = -1;
    bool m_inlineEditingPronunciation = false;
    QVector<QPointF> m_pitchStroke;
    QVector<AnchorSelection> m_selectedAnchors;
    QVector<AnchorSelection> m_anchorSelectionBeforeRect;
    int m_anchorPreviewDeltaTick = 0;
    int m_anchorPreviewDeltaValue = 0;
    int m_anchorMergeCurveIndex = -1;
    int m_anchorMergeNodeIndex = -1;
    QVector<EditorRenderRect> m_pastePreviewRects;
    QVector<EditorRenderText> m_pastePreviewTexts;
    double m_cachedStartTick = 0.0;
    double m_cachedEndTick = 0.0;
    double m_cachedTopKey = 127.0;
    double m_cachedBottomKey = 0.0;
};

#endif // RHIPIANOROLLCANVAS_H
