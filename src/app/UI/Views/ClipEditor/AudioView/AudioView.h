#ifndef AUDIOVIEW_H
#define AUDIOVIEW_H

#include "AudioViewContent.h"
#include "Interface/EditorViewState.h"

#include <QWidget>

class AudioChannelAxisView;
class AudioClip;
class AudioViewGraphicsScene;
class AudioViewGraphicsView;
class AudioViewRhiWidget;
class QLabel;
class QVBoxLayout;
class QWheelEvent;
class QEvent;
class TimelineView;

// Read-only view of an audio clip: one waveform lane per channel plus a dB axis
// on the left, sharing the time ruler with the piano roll. It occupies the same
// place in the clip editor and switches in by clip type, so a stereo clip shows
// its two channels instead of an empty piano roll.
class AudioView final : public QWidget {
    Q_OBJECT

public:
    explicit AudioView(QWidget *parent = nullptr);
    ~AudioView() override;

    void setDataContext(AudioClip *clip);
    void setTrackColorIndex(int index);
    [[nodiscard]] PianoRollViewState viewState() const;
    bool centerAt(double tick) const;
    bool setTimeViewport(double centerTick, double horizontalScale) const;
    [[nodiscard]] double scaleX() const;
    [[nodiscard]] int horizontalBarValue() const;

public slots:
    void onWheelHorScale(QWheelEvent *event) const;
    void onWheelHorScroll(QWheelEvent *event) const;
    void setHorizontalBarValue(int value) const;
    void setPlaybackPosition(double tick) const;
    void setLastPlaybackPosition(double tick) const;

signals:
    void scaleChanged(double horizontal);
    void horizontalBarValueChanged(int value);

protected:
    void changeEvent(QEvent *event) override;

private:
    void createLegacyBackend();
    void connectLegacyBackend();
    void connectRhiBackend();
    void fallbackToLegacy();
    void applyContent();
    void refreshContent();
    void updateContentVisibility() const;
    [[nodiscard]] AudioViewContent buildContent() const;

    AudioViewGraphicsScene *m_scene = nullptr;
    AudioViewGraphicsView *m_graphicsView = nullptr;
    AudioViewRhiWidget *m_rhiView = nullptr;
    QWidget *m_editorWidget = nullptr;
    TimelineView *m_timelineView = nullptr;
    AudioChannelAxisView *m_axisView = nullptr;
    QLabel *m_lbTip = nullptr;
    QVBoxLayout *m_rightLayout = nullptr;
    QWidget *m_axisColumn = nullptr;
    AudioClip *m_clip = nullptr;
    int m_trackColorIndex = 0;
};

#endif // AUDIOVIEW_H
