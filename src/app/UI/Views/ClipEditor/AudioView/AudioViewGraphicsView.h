#ifndef AUDIOVIEWGRAPHICSVIEW_H
#define AUDIOVIEWGRAPHICSVIEW_H

#include "AudioViewContent.h"
#include "UI/Views/Common/TimeGraphicsView.h"

#include <QColor>
#include <QList>

class AudioViewBackground;
class AudioViewGraphicsScene;
class AudioWaveformLaneView;
class ClipRangeOverlay;

// QGraphicsView backend of the audio view. One lane item per channel, no
// selection and no editing: the view is a read-only rendering of the audio
// material, so every pointer interaction that would mutate something is left to
// the piano roll.
class AudioViewGraphicsView final : public TimeGraphicsView {
    Q_OBJECT
    Q_PROPERTY(QColor laneBackgroundColor READ laneBackgroundColor WRITE setLaneBackgroundColor)
    Q_PROPERTY(QColor laneSeparatorColor READ laneSeparatorColor WRITE setLaneSeparatorColor)
    Q_PROPERTY(
        QColor waveformCenterLineColor READ waveformCenterLineColor WRITE setWaveformCenterLineColor)
    Q_PROPERTY(QColor clipRangeOverlayColor READ clipRangeOverlayColor WRITE setClipRangeOverlayColor)

public:
    explicit AudioViewGraphicsView(AudioViewGraphicsScene *scene, QWidget *parent = nullptr);

    void setContent(const AudioViewContent &content);
    [[nodiscard]] const AudioViewContent &content() const;

    [[nodiscard]] double viewCenterTick() const;
    bool centerAt(double center);
    bool setTimeViewport(double center, double horizontalScale);
    // Centres the viewport on the audible part of the clip the first time the
    // view is shown, and keeps the current time window afterwards
    void positionViewportAtContent();

    QColor laneBackgroundColor() const;
    void setLaneBackgroundColor(const QColor &color);
    QColor laneSeparatorColor() const;
    void setLaneSeparatorColor(const QColor &color);
    QColor waveformCenterLineColor() const;
    void setWaveformCenterLineColor(const QColor &color);
    QColor clipRangeOverlayColor() const;
    void setClipRangeOverlayColor(const QColor &color);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void afterSetScale() override;

private:
    void rebuildLanes();
    void applyContentToLanes();
    void applyLaneColors();

    AudioViewGraphicsScene *m_audioScene = nullptr;
    AudioViewBackground *m_background = nullptr;
    ClipRangeOverlay *m_clipRangeOverlay = nullptr;
    QList<AudioWaveformLaneView *> m_laneViews;
    AudioViewContent m_content;
    QColor m_laneBackgroundColor;
    QColor m_laneSeparatorColor;
    QColor m_waveformCenterLineColor;
    QColor m_clipRangeOverlayColor;
    bool m_viewportPositioned = false;
};

#endif // AUDIOVIEWGRAPHICSVIEW_H
