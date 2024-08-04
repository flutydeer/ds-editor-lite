//
// Created by fluty on 2024/2/10.
//

#ifndef TIMEGRAPHICSVIEW_H
#define TIMEGRAPHICSVIEW_H

#include "CommonGraphicsView.h"

class TimeGraphicsScene;
class TimeGridGraphicsItem;
class TimeIndicatorGraphicsItem;

class TimeGraphicsView : public CommonGraphicsView {
    Q_OBJECT

public:
    explicit TimeGraphicsView(TimeGraphicsScene *scene, QWidget *parent = nullptr);
    TimeGraphicsScene *scene();
    void setGridItem(TimeGridGraphicsItem *item);
    void setSceneVisibility(bool on);
    [[nodiscard]] double startTick() const;
    [[nodiscard]] double endTick() const;
    void setOffset(int tick);
    void setPixelsPerQuarterNote(int px);
    void setAutoTurnPage(bool on);
    void setViewportStartTick(double tick);
    void setViewportCenterAtTick(double tick);

signals:
    void timeRangeChanged(double startTick, double endTick);

public slots:
    void setPlaybackPosition(double tick);
    void setLastPlaybackPosition(double tick);
    void pageAdd();
    // void setTimeRange(double startTick, double endTick);

protected:
    [[nodiscard]] double sceneXToTick(double pos) const;
    [[nodiscard]] double tickToSceneX(double tick) const;

private:
    TimeGraphicsScene *m_scene;
    TimeGridGraphicsItem *m_gridItem = nullptr;
    TimeIndicatorGraphicsItem *m_scenePlayPosIndicator = nullptr;
    TimeIndicatorGraphicsItem *m_sceneLastPlayPosIndicator = nullptr;

    int m_offset = 0;
    int m_pixelsPerQuarterNote = 64;
    bool m_autoTurnPage = true;
    double m_playbackPosition = 0;
    double m_lastPlaybackPosition = 0;
};



#endif // TIMEGRAPHICSVIEW_H
