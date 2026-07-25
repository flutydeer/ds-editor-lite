#ifndef LEVELMETERVIEWMODEL_H
#define LEVELMETERVIEWMODEL_H

#include <QObject>

class QTimer;
class QVariantAnimation;

class LevelMeterViewModel : public QObject {
    Q_OBJECT
    Q_PROPERTY(double levelL READ levelL NOTIFY levelChanged)
    Q_PROPERTY(double levelR READ levelR NOTIFY levelChanged)
    Q_PROPERTY(double displayedPeakL READ displayedPeakL NOTIFY peakChanged)
    Q_PROPERTY(double displayedPeakR READ displayedPeakR NOTIFY peakChanged)
    Q_PROPERTY(bool clippedL READ clippedL NOTIFY clipStateChanged)
    Q_PROPERTY(bool clippedR READ clippedR NOTIFY clipStateChanged)

public:
    explicit LevelMeterViewModel(QObject *parent = nullptr);

    void setLevels(double dBL, double dBR);
    void resetClip();
    [[nodiscard]] double peakValue() const;

    double levelL() const;
    double levelR() const;
    double displayedPeakL() const;
    double displayedPeakR() const;
    bool clippedL() const;
    bool clippedR() const;

signals:
    void levelChanged();
    void peakChanged();
    void clipStateChanged();
    void peakValueChanged(double dB);

private:
    struct ChannelData {
        double currentLevel = 0;
        double peak = 0;
        double displayedPeak = 0;
        QTimer *peakHoldTimer = nullptr;
        QVariantAnimation *decayAnimation = nullptr;
        bool isDecaying = false;
        bool clipped = false;
    };

    void startDecayAnimation(ChannelData &channel);
    void updatePeakValue(ChannelData &channel);
    void cancelDecayAnimation(ChannelData &channel);
    void handleAnimationUpdate(const QVariant &value, ChannelData &channel);

    void notifyDisplayedPeakChange();
    double getPeakValueForTextDisplaying() const;

    ChannelData m_leftChannel;
    ChannelData m_rightChannel;
    int m_peakHoldTime = 2500;
    int m_decayTime = 1000;
    double m_lastPeakValue = 0.0;
};

#endif // LEVELMETERVIEWMODEL_H
