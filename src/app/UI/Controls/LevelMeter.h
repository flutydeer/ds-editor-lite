//
// Created by fluty on 2023/8/6.
//

#ifndef CHORUSKIT_LEVELMETER_H
#define CHORUSKIT_LEVELMETER_H

#include <QWidget>
#include <QTimer>

class LevelMeter : public QWidget {
    Q_OBJECT
public:
    enum class MeterStyle { Segmented, Gradient };
    explicit LevelMeter(QWidget *parent = nullptr);
    ~LevelMeter() override;

    void setClipped(bool onL, bool onR);
    [[nodiscard]] int bufferSize() const;
    void setBufferSize(int size);
    void initBuffer(int bufferSize);
    [[nodiscard]] bool freeze() const;
    void setFreeze(bool on);

    // public slots:
    void readSample(double sampleL, double sampleR);
    void setValue(double valueL, double valueR);

private:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resetBuffer();
    [[nodiscard]] bool mouseOnClipIndicator(const QPointF &pos) const;

    void drawSegmentedBar(QPainter &painter, const QRectF &rect, const double &level);
    void drawGradientBar(QPainter &painter, const QRectF &rect, const double &level);

    // QColor m_colorBackground = QColor(0, 0, 0, 60);
    MeterStyle m_style = LevelMeter::MeterStyle::Gradient;
    QColor m_colorSafe = QColor(155, 186, 255);
    QColor m_colorWarn = QColor(255, 205, 155);
    QColor m_colorCritical = QColor(255, 155, 157);
    const double m_safeThreshold = 0.707946;    //-3 dB
    const double m_warnThreshold = 0.891251;    //-1 dB
    const double m_safeThresholdAlt = 0.501187; //-6 dB
    double m_smoothedLevelL = 0;
    double m_smoothedLevelR = 0;
    bool m_clippedL = false;
    bool m_clippedR = false;
    //    int sampleRate = 44100;
    double *m_bufferL = nullptr;
    double *m_bufferR = nullptr;
    int m_bufferPos = 0;
    int m_bufferSize = 8;
    QTimer *m_timer;
    bool m_freezed = false;
    double m_clipIndicatorLength = 6; // px
    double m_spacing = 1.0;
};



#endif // CHORUSKIT_LEVELMETER_H
