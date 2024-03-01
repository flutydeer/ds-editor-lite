//
// Created by fluty on 2024/2/10.
//

#include "TimeGraphicsView.h"

#include <QScrollBar>

#include "TimeGraphicsScene.h"
#include "TimeIndicatorGraphicsItem.h"

TimeGraphicsView::TimeGraphicsView(TimeGraphicsScene *scene) : m_scene(scene) {
    connect(this, &TimeGraphicsView::visibleRectChanged,
            [=](const QRectF &rect) { m_scene->setVisibleRect(rect); });
    connect(this, &TimeGraphicsView::scaleChanged,
            [=](double sx, double sy) { m_scene->setScaleXY(sx, sy); });

    m_scenePlayPosIndicator = new TimeIndicatorGraphicsItem;
    m_scenePlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
    QPen curPlayPosPen;
    curPlayPosPen.setWidth(1);
    curPlayPosPen.setColor(QColor(255, 204, 153));
    m_scenePlayPosIndicator->setPen(curPlayPosPen);
    m_scenePlayPosIndicator->setPosition(m_playbackPosition);
    m_scene->addTimeIndicator(m_scenePlayPosIndicator);

    m_sceneLastPlayPosIndicator = new TimeIndicatorGraphicsItem;
    m_sceneLastPlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
    QPen lastPlayPosPen;
    lastPlayPosPen.setWidth(1);
    lastPlayPosPen.setColor(QColor(160, 160, 160));
    lastPlayPosPen.setStyle(Qt::DashLine);
    m_sceneLastPlayPosIndicator->setPen(lastPlayPosPen);
    m_sceneLastPlayPosIndicator->setPosition(m_lastPlaybackPosition);
    m_scene->addTimeIndicator(m_sceneLastPlayPosIndicator);

    setScene(m_scene);

    connect(this, &CommonGraphicsView::scaleChanged, this,
            [=] { emit timeRangeChanged(startTick(), endTick()); });
    connect(this, &CommonGraphicsView::visibleRectChanged, this,
            [=] { emit timeRangeChanged(startTick(), endTick()); });
}
TimeGraphicsScene *TimeGraphicsView::scene() {
    return m_scene;
}
void TimeGraphicsView::setSceneVisibility(bool on) {
    setScene(on ? m_scene : nullptr);
}
double TimeGraphicsView::startTick() const {
    return sceneXToTick(visibleRect().left());
}
double TimeGraphicsView::endTick() const {
    return sceneXToTick(visibleRect().right());
}
void TimeGraphicsView::setPixelsPerQuarterNote(int px) {
    m_pixelsPerQuarterNote = px;
    m_scenePlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
    m_sceneLastPlayPosIndicator->setPixelsPerQuarterNote(m_pixelsPerQuarterNote);
}
void TimeGraphicsView::setAutoTurnPage(bool on) {
    m_autoTurnPage = on;
    if (m_playbackPosition > endTick())
        pageAdd();
}
void TimeGraphicsView::setPlaybackPosition(double tick) {
    m_playbackPosition = tick;
    if (m_scenePlayPosIndicator != nullptr)
        m_scenePlayPosIndicator->setPosition(tick);

    if (m_autoTurnPage) {
        if (m_playbackPosition > endTick())
            pageAdd();
        else if (m_playbackPosition < startTick())
            setViewportStartTick(m_playbackPosition);
    }
}
void TimeGraphicsView::setLastPlaybackPosition(double tick) {
    m_lastPlaybackPosition = tick;
    if (m_sceneLastPlayPosIndicator != nullptr)
        m_sceneLastPlayPosIndicator->setPosition(tick);
}
void TimeGraphicsView::setViewportStartTick(double tick) {
    auto sceneX = qRound(tickToSceneX(tick));
    horizontalScrollBar()->setValue(sceneX);
}
void TimeGraphicsView::setViewportCenterAtTick(double tick) {
    auto tickRange = endTick() - startTick();
    auto targetStart = tick - tickRange / 2;
    qDebug() << "tickRange" << tickRange << "tick" << tick << "targetStart" << targetStart;
    setViewportStartTick(targetStart);
}
void TimeGraphicsView::pageAdd() {
    horizontalScrollBar()->triggerAction(QAbstractSlider::SliderPageStepAdd);
}
double TimeGraphicsView::sceneXToTick(double pos) const {
    return 480 * pos / scaleX() / m_pixelsPerQuarterNote;
}
double TimeGraphicsView::tickToSceneX(double tick) const {
    return tick * scaleX() * m_pixelsPerQuarterNote / 480;
}