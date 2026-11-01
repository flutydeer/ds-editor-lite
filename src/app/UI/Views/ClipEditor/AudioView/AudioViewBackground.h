#ifndef AUDIOVIEWBACKGROUND_H
#define AUDIOVIEWBACKGROUND_H

#include "UI/Views/Common/TimeGridView.h"

// Canvas background of the audio view: one band per channel lane, the separators
// between them and the shared time grid on top. Every lane shares one tone, so the
// separators are what tells the channels apart.
class AudioViewBackground final : public TimeGridView {
    Q_OBJECT

public:
    explicit AudioViewBackground(QGraphicsItem *parent = nullptr);

    void setChannelCount(int count);
    void setLaneBackgroundColor(const QColor &color);
    void setLaneSeparatorColor(const QColor &color);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;
    void updateRectAndPos() override;

private:
    int m_channelCount = 1;
    QColor m_laneBackgroundColor = {32, 35, 43};
    QColor m_laneSeparatorColor = {60, 65, 78};
};

#endif // AUDIOVIEWBACKGROUND_H
