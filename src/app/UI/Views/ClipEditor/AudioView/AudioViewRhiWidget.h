#ifndef AUDIOVIEWRHIWIDGET_H
#define AUDIOVIEWRHIWIDGET_H

#include "AudioViewContent.h"
#include "UI/Views/Common/EditorRhiWidget.h"
#include "UI/Views/Common/EditorTouchTarget.h"

#include <QColor>

#include <memory>

class QHideEvent;
class QResizeEvent;
class QShowEvent;
class QWheelEvent;

// RHI backend of the audio view. Shares the lane geometry and the dB axis with
// the QGraphicsView backend, but draws the waveforms as triangles so a long
// clip keeps its frame time when the view is zoomed out.
class AudioViewRhiWidget final : public EditorRhiWidget, public EditorTouchTarget {
    Q_OBJECT
    Q_PROPERTY(QColor laneBackgroundColor READ laneBackgroundColor WRITE setLaneBackgroundColor)
    Q_PROPERTY(QColor laneSeparatorColor READ laneSeparatorColor WRITE setLaneSeparatorColor)
    Q_PROPERTY(
        QColor waveformCenterLineColor READ waveformCenterLineColor WRITE setWaveformCenterLineColor)
    Q_PROPERTY(
        QColor clipRangeOverlayColor READ clipRangeOverlayColor WRITE setClipRangeOverlayColor)
    Q_PROPERTY(QColor barLineColor READ barLineColor WRITE setBarLineColor)
    Q_PROPERTY(QColor beatLineColor READ beatLineColor WRITE setBeatLineColor)
    Q_PROPERTY(QColor commonLineColor READ commonLineColor WRITE setCommonLineColor)
    Q_PROPERTY(
        QColor playPosIndicatorColor READ playPosIndicatorColor WRITE setPlayPosIndicatorColor)
    Q_PROPERTY(QColor lastPlayPosIndicatorColor READ lastPlayPosIndicatorColor WRITE
                   setLastPlayPosIndicatorColor)
    Q_PROPERTY(QColor emptyViewBackgroundColor READ emptyViewBackgroundColor WRITE
                   setEmptyViewBackgroundColor)

public:
    explicit AudioViewRhiWidget(QWidget *parent = nullptr);
    ~AudioViewRhiWidget() override;

    void setContent(const AudioViewContent &content);
    [[nodiscard]] const AudioViewContent &content() const;

    [[nodiscard]] double startTick() const;
    [[nodiscard]] double endTick() const;
    [[nodiscard]] double viewCenterTick() const;
    [[nodiscard]] double scaleX() const;
    [[nodiscard]] int horizontalBarValue() const;
    bool centerAt(double center);
    bool setTimeViewport(double center, double horizontalScale);
    void positionViewportAtContent();

public slots:
    void setHorizontalBarValue(int value);
    void setPlaybackPosition(double tick);
    void setLastPlaybackPosition(double tick);
    void onWheelHorScale(QWheelEvent *event);
    void onWheelHorScroll(QWheelEvent *event);

signals:
    void timeRangeChanged(double startTick, double endTick);
    void scaleChanged(double horizontal);
    void horizontalBarValueChanged(int value);
    void backendUnavailable();

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void onRhiReady() override;
    void onDevicePixelRatioChanged() override;

    // --- EditorTouchTarget ---
    // Read-only canvas: nothing here can be grabbed, so a finger only pans and
    // pinches, and a tap is consumed
    void stopTouchViewportAnimation() override;
    void panTouchViewportBy(const QPointF &deltaPixels) override;
    void zoomTouchViewportBy(double horizontalFactor, double verticalFactor,
                             const QPointF &anchor) override;
    [[nodiscard]] ContentHit touchContentAt(const QPointF &viewportPosition) const override;
    [[nodiscard]] BlankDragAction touchBlankDragAction() const override;
    [[nodiscard]] bool touchFingerEdits() const override;

public:
    QColor laneBackgroundColor() const;
    void setLaneBackgroundColor(const QColor &color);
    QColor laneSeparatorColor() const;
    void setLaneSeparatorColor(const QColor &color);
    QColor waveformCenterLineColor() const;
    void setWaveformCenterLineColor(const QColor &color);
    QColor clipRangeOverlayColor() const;
    void setClipRangeOverlayColor(const QColor &color);
    QColor barLineColor() const;
    void setBarLineColor(const QColor &color);
    QColor beatLineColor() const;
    void setBeatLineColor(const QColor &color);
    QColor commonLineColor() const;
    void setCommonLineColor(const QColor &color);
    QColor playPosIndicatorColor() const;
    void setPlayPosIndicatorColor(const QColor &color);
    QColor lastPlayPosIndicatorColor() const;
    void setLastPlayPosIndicatorColor(const QColor &color);
    QColor emptyViewBackgroundColor() const;
    void setEmptyViewBackgroundColor(const QColor &color);

private:
    void notifyViewportChanged();

    class Private;
    std::unique_ptr<Private> d;
};

#endif // AUDIOVIEWRHIWIDGET_H
