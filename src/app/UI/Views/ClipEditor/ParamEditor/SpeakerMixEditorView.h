//
// Created by fluty on 2026/5/4.
//

#ifndef SPEAKERMIXEDITORVIEW_H
#define SPEAKERMIXEDITORVIEW_H

#include "UI/Views/Common/TimeOverlayView.h"

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QList>

struct SpeakerMixSpeaker {
    QString name;
    QColor color;
    QColor fillColor;
    QColor dotFillColor;
};

struct SpeakerMixKeyframe {
    int tick = 0;
    QList<double> weights;
};

struct SpeakerMixHitResult {
    int keyframeIndex = -1;
    int splitIndex = -1;
};

class SpeakerMixEditorView : public TimeOverlayView {

public:
    SpeakerMixEditorView();

    const QList<SpeakerMixSpeaker> &speakers() const;
    const QList<SpeakerMixKeyframe> &keyframes() const;
    int keyframeCount() const;
    int selectedKeyframeIndex() const;

    double previousKeyframeTick(double currentTick) const;
    double nextKeyframeTick(double currentTick) const;

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void updateRectAndPos() override;

    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;

    QList<double> interpolateWeights(double tick) const;
    void drawStackedArea(QPainter *painter) const;
    void drawKeyframeDots(QPainter *painter) const;
    void drawSelectionRect(QPainter *painter) const;

    SpeakerMixHitResult hitTest(const QPointF &itemPos) const;

    double cumWeightAtSplit(const SpeakerMixKeyframe &kf, int splitIndex) const;
    double cumWeightFromItemY(double itemY) const;
    double cumWeightToItemY(double cumWeight) const;

    void updateHover(const QPointF &itemPos);
    void startIntervalSelection(const QPointF &itemPos);
    void updateIntervalSelection(const QPointF &itemPos);
    void endIntervalSelection();

    void startDrag(const QPointF &scenePos);
    void updateDrag(const QPointF &scenePos);
    void endDrag();

    void addKeyframeAt(int tick);
    void deleteSelectedKeyframe();

    QList<SpeakerMixSpeaker> m_speakers;
    QList<SpeakerMixKeyframe> m_keyframes;

    struct InteractionState {
        int hoveredKeyframeIndex = -1;
        int hoveredSplitIndex = -1;
        int selectedKeyframeIndex = -1;
        int selectedSplitIndex = -1;

        bool dragging = false;
        QPointF dragStartScenePos;
        SpeakerMixKeyframe dragStartWeights;
        int dragStartTick = 0;
        int dragSplitIndex = -1;
        bool altDrag = false;

        bool selecting = false;
        QPointF selectionStartPos;
        QRectF selectionRect;
        QList<int> selectedKeyframeIndices;
    } m_state;

    static constexpr double kDotRadius = 6.0;
    static constexpr double kHitRadius = 6.0;
    static constexpr double kDragThreshold = 3.0;
    static constexpr double kPadding = 2.0;
};

#endif // SPEAKERMIXEDITORVIEW_H
