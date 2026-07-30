#ifndef RHITRACKCANVAS_H
#define RHITRACKCANVAS_H

#include "ITrackEditorCanvas.h"

#include "UI/Views/EditorCanvas/RenderUpdateScheduler.h"

#include <QHash>
#include <QList>
#include <QMetaObject>

namespace talcs {
    class AbstractAudioFormatIO;
}

class RhiEditorCanvasWidget;
class AudioClip;
class Clip;

class RhiTrackCanvas final : public ITrackEditorCanvas {
    Q_OBJECT

public:
    explicit RhiTrackCanvas(QObject *parent = nullptr);
    ~RhiTrackCanvas() override;

    [[nodiscard]] EditorCanvasBackend backend() const override;
    [[nodiscard]] QWidget *widget() const override;
    [[nodiscard]] QScrollBar *horizontalScrollBar() const override;
    [[nodiscard]] QScrollBar *verticalScrollBar() const override;
    [[nodiscard]] EditorViewportState viewportState() const override;
    void restoreViewportState(const EditorViewportState &state) override;
    bool centerAt(double tick, double trackIndex) override;
    bool setViewportScale(double horizontalScale, double verticalScale) override;
    [[nodiscard]] QRectF visibleRect() const override;
    [[nodiscard]] double sceneXForTick(double tick) const override;
    void setSceneLength(int tick) override;
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
    enum class Interaction {
        None,
        Move,
        ResizeLeft,
        ResizeRight,
        RectSelect,
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
    void clearPastePreview();
    void addAudioClip(int trackIndex, int tick);
    void relocateAudioClip(int clipId);
    static void extractMidi(int clipId);
    void appendAudioWaveform(EditorRenderSnapshot &snapshot, const AudioClip *clip,
                             const QRectF &bounds, int materialStartTick, const QColor &color);
    [[nodiscard]] talcs::AbstractAudioFormatIO *audioReader(const QString &path);
    void clearAudioReaders();
    static double sincInterpolate(const QVector<float> &samples, qint64 sampleOffset,
                                  qint64 totalFrames, double position);
    [[nodiscard]] Clip *findClip(int id, int *trackIndex = nullptr) const;
    [[nodiscard]] int tickAt(double x) const;
    [[nodiscard]] int trackAt(double y) const;
    [[nodiscard]] int snappedTick(int tick, bool snapOff) const;
    void beginEditTransaction(int clipId);
    void finishEditTransaction(bool commit);
    void reconnectModelSignals();

    RhiEditorCanvasWidget *m_widget = nullptr;
    RenderUpdateScheduler m_scheduler;
    quint64 m_revision = 0;
    Interaction m_interaction = Interaction::None;
    QPointF m_pressPosition;
    QRectF m_selectionRect;
    int m_pressedClipId = -1;
    int m_hoveredClipId = -1;
    int m_originalStart = 0;
    int m_originalLength = 0;
    int m_originalClipStart = 0;
    int m_originalClipLen = 0;
    int m_originalTrack = -1;
    int m_previewStart = 0;
    int m_previewLength = 0;
    int m_previewClipStart = 0;
    int m_previewClipLen = 0;
    int m_previewTrack = -1;
    bool m_editTransactionActive = false;
    bool m_dragUsesRealtimeTruth = false;
    double m_dragTrimMs = 0.0;
    double m_dragPlayLengthMs = 0.0;
    double m_dragMaterialLengthMs = 0.0;
    double m_grabOffsetMs = 0.0;
    double m_materialStartMs = 0.0;
    double m_visibleEndMs = 0.0;
    QVector<EditorRenderRect> m_pastePreviewRects;
    double m_cachedStartTick = 0.0;
    double m_cachedEndTick = 0.0;
    double m_cachedTopTrack = 0.0;
    double m_cachedBottomTrack = 0.0;
    int m_sceneLength = 0;
    QList<QMetaObject::Connection> m_modelConnections;
    QHash<QString, talcs::AbstractAudioFormatIO *> m_audioReaders;
};

#endif // RHITRACKCANVAS_H
