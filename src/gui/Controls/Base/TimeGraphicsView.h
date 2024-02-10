//
// Created by fluty on 2024/2/10.
//

#ifndef TIMEGRAPHICSVIEW_H
#define TIMEGRAPHICSVIEW_H

#include "CommonGraphicsView.h"
#include "TimeGraphicsScene.h"

class TimeGraphicsView : public CommonGraphicsView {
    Q_OBJECT

public:
    explicit TimeGraphicsView(TimeGraphicsScene *scene);
    TimeGraphicsScene *scene();
    void setSceneVisibility(bool on);
    double startTick() const;
    double endTick() const;
    void setPixelsPerQuarterNote(int px);
    void setAutoTurnPage(bool on);

signals:
    void timeRangeChanged(double startTick, double endTick);

public slots:
    void setPlaybackPosition(double tick);
    void setLastPlaybackPosition(double tick);
    void setStartTick(double tick);
    void pageAdd();
    // void setTimeRange(double startTick, double endTick);

private:
    TimeGraphicsScene *m_scene;
    TimeGridGraphicsItem *m_gridItem;
    TimeIndicatorGraphicsItem *m_scenePlayPosIndicator;
    TimeIndicatorGraphicsItem *m_sceneLastPlayPosIndicator;

    double sceneXToTick(double pos) const;
    double tickToSceneX(double tick) const;
    int m_pixelsPerQuarterNote = 64;
    bool m_autoTurnPage = true;
    double m_playbackPosition = 0;
    double m_lastPlaybackPosition = 0;
};



#endif // TIMEGRAPHICSVIEW_H
