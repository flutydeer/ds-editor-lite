//
// Created by fluty on 2026/5/4.
//

#ifndef SPEAKERMIXEDITORVIEW_H
#define SPEAKERMIXEDITORVIEW_H

#include "UI/Views/Common/TimeOverlayView.h"

#include <QColor>
#include <QString>

struct SpeakerMixSpeaker {
    QString name;
    QColor color;
    QColor fillColor;
};

struct SpeakerMixKeyframe {
    enum InterpMode { Linear, Hermite };

    int tick = 0;
    QList<double> weights;
    InterpMode interpMode = Hermite;
};

class SpeakerMixEditorView : public TimeOverlayView {

public:
    SpeakerMixEditorView();

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void updateRectAndPos() override;

    QList<double> interpolateWeights(double tick) const;
    void drawStackedArea(QPainter *painter) const;
    void drawKeyframeDots(QPainter *painter) const;

    QList<SpeakerMixSpeaker> m_speakers;
    QList<SpeakerMixKeyframe> m_keyframes;
};

#endif // SPEAKERMIXEDITORVIEW_H
