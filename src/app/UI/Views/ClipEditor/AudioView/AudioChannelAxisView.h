#ifndef AUDIOCHANNELAXISVIEW_H
#define AUDIOCHANNELAXISVIEW_H

#include <QColor>
#include <QWidget>

// Left column of the audio view. Occupies the place the piano keyboard has in
// the piano roll and carries one dB axis per channel lane plus the channel tag.
// It is a plain widget rather than part of the canvas, so both render backends
// (QGraphicsView and RHI) share exactly one axis implementation.
class AudioChannelAxisView final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor tickColor READ tickColor WRITE setTickColor)
    Q_PROPERTY(QColor labelColor READ labelColor WRITE setLabelColor)
    Q_PROPERTY(QColor infiniteLabelColor READ infiniteLabelColor WRITE setInfiniteLabelColor)
    Q_PROPERTY(QColor separatorColor READ separatorColor WRITE setSeparatorColor)
    Q_PROPERTY(QColor channelTagColor READ channelTagColor WRITE setChannelTagColor)
    Q_PROPERTY(QColor laneBackgroundColor READ laneBackgroundColor WRITE setLaneBackgroundColor)
    Q_PROPERTY(QColor laneAlternateBackgroundColor READ laneAlternateBackgroundColor WRITE
                   setLaneAlternateBackgroundColor)

public:
    explicit AudioChannelAxisView(QWidget *parent = nullptr);

    void setChannelCount(int count);
    [[nodiscard]] int channelCount() const;

    QColor tickColor() const;
    void setTickColor(const QColor &color);
    QColor labelColor() const;
    void setLabelColor(const QColor &color);
    QColor infiniteLabelColor() const;
    void setInfiniteLabelColor(const QColor &color);
    QColor separatorColor() const;
    void setSeparatorColor(const QColor &color);
    QColor channelTagColor() const;
    void setChannelTagColor(const QColor &color);
    QColor laneBackgroundColor() const;
    void setLaneBackgroundColor(const QColor &color);
    QColor laneAlternateBackgroundColor() const;
    void setLaneAlternateBackgroundColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    int m_channelCount = 1;
    QColor m_tickColor = {90, 96, 110};
    QColor m_labelColor = {140, 146, 160};
    QColor m_infiniteLabelColor = {110, 116, 130};
    QColor m_separatorColor = {60, 65, 78};
    QColor m_channelTagColor = {96, 102, 116};
    QColor m_laneBackgroundColor = {32, 35, 43};
    QColor m_laneAlternateBackgroundColor = {28, 31, 38};
};

#endif // AUDIOCHANNELAXISVIEW_H
