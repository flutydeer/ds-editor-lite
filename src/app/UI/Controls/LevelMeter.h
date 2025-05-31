//
// Created by fluty on 2023/8/6.
//

#ifndef CHORUSKIT_LEVELMETER_H
#define CHORUSKIT_LEVELMETER_H

#include <QWidget>

class QTimer;
class QVariantAnimation;
class ToolTipFilter;

class LevelMeter : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double padding READ padding WRITE setPadding)
    Q_PROPERTY(double spacing READ spacing WRITE setSpacing)
    Q_PROPERTY(double clipIndicatorLength READ clipIndicatorLength WRITE setClipIndicatorLength)
    Q_PROPERTY(bool showValueWhenHover READ showValueWhenHover WRITE setShowValueWhenHover)
    Q_PROPERTY(QColor dimmedColor READ dimmedColor WRITE setDimmedColor)
    Q_PROPERTY(QColor clippedColor READ clippedColor WRITE setClippedColor)
    Q_PROPERTY(QColor safeColor READ safeColor WRITE setSafeColor)
    Q_PROPERTY(QColor warnColor READ warnColor WRITE setWarnColor)
    Q_PROPERTY(QColor criticalColor READ criticalColor WRITE setCriticalColor)
    Q_PROPERTY(QColor currentValueColor READ currentValueColor WRITE setCurrentValueColor)

public:
    enum class MeterStyle { Segmented, Gradient };

    explicit LevelMeter(QWidget *parent = nullptr);

    void setClipped(bool onL, bool onR);
    void clearClipped();
    void setValue(double valueL, double valueR);

signals:
    void peakValueChanged(double value);

private:
    struct ChannelData {
        double currentValue = 0.0;
        // double peakValue = 0.0;
        double displayedPeak = 0.0;
        QTimer *peakHoldTimer = nullptr;
        QVariantAnimation *decayAnimation = nullptr;
        bool isDecaying = false;
        bool clipped = false;
    };

    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    [[nodiscard]] bool mouseOnClipIndicator(const QPointF &pos) const;
    bool event(QEvent *event) override;

    void onHover(QHoverEvent *event);
    void handleHoverOnBar();
    void handleHoverOnClipIndicator();

    void drawSegmentedBar(QPainter &painter, const QRectF &rect, const double &level);
    void drawGradientBar(QPainter &painter, const QRectF &rect, const double &level);
    void drawPeakHold(QPainter &painter, const QRectF &rect, const double &level);

    void startDecayAnimation(ChannelData &channel);
    void updatePeakValue(ChannelData &channel, double newValue);
    void cancelDecayAnimation(ChannelData &channel);
    void handleAnimationUpdate(const QVariant &value, ChannelData &channel);

    void notifyPeakValueChange();

    QString gainValueToString(double gain); // TODO: refactor

    // Properties getters and setters
    double padding() const;
    void setPadding(double value);
    double spacing() const;
    void setSpacing(double value);
    double clipIndicatorLength() const;
    void setClipIndicatorLength(double value);
    bool showValueWhenHover() const;
    void setShowValueWhenHover(bool on);
    QColor dimmedColor() const;
    void setDimmedColor(const QColor &color);
    QColor clippedColor() const;
    void setClippedColor(const QColor &color);
    QColor safeColor() const;
    void setSafeColor(const QColor &color);
    QColor warnColor() const;
    void setWarnColor(const QColor &color);
    QColor criticalColor() const;
    void setCriticalColor(const QColor &color);
    QColor currentValueColor() const;
    void setCurrentValueColor(const QColor &color);

    // Properties private fields
    double m_padding = 8;
    double m_spacing = 1;
    double m_clipIndicatorLength = 6; // px
    bool m_showValueWhenHover = true;
    QColor m_colorDimmed = {41, 44, 54};
    QColor m_colorClipped = {255, 155, 157};
    QColor m_colorSafe = {155, 224, 255};
    QColor m_colorWarn = {155, 255, 174};
    QColor m_colorCritical = {255, 224, 155};
    QColor m_colorCurrentValue = {211, 214, 224};
    QColor m_colorPeakHold = {211, 214, 224};
    double m_peakHoldWidth = 1.2;
    double m_lastPeakValue = 0.0;

    // For drawing
    MeterStyle m_style = MeterStyle::Gradient;
    const double m_safeThreshold = 0.707946;    //-3 dB
    const double m_warnThreshold = 0.891251;    //-1 dB
    const double m_safeThresholdAlt = 0.501187; //-6 dB
    ChannelData m_leftChannel;
    ChannelData m_rightChannel;
    int m_peakHoldTime = 2000;
    int m_decayTime = 1000;

    QRectF paddedRect;
    double channelWidth = 0;
    double channelTop = 0;
    double channelLength = 0;
    double mouseY = 0;
    bool m_mouseOnBar = false;
    QString m_currentValueText;

};


#endif // CHORUSKIT_LEVELMETER_H