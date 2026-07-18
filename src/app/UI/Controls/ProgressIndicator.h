//
// Created by fluty on 2023/8/14.
//

#ifndef DATASET_TOOLS_PROGRESSINDICATOR_H
#define DATASET_TOOLS_PROGRESSINDICATOR_H

#include <QElapsedTimer>
#include <QPropertyAnimation>
#include <QWidget>
#include <QTimer>

#include "UI/Utils/IAnimatable.h"
#include "Global/TaskGlobal.h"

class ProgressIndicator : public QWidget, public IAnimatable {
    Q_OBJECT
    Q_PROPERTY(double minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(double maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(double value READ value WRITE setValue NOTIFY valueChanged)
    Q_PROPERTY(double secondaryValue READ secondaryValue WRITE setSecondaryValue NOTIFY
                   secondaryValueChanged)
    Q_PROPERTY(
        double currentTaskValue READ currentTaskValue WRITE setValue NOTIFY currentTaskValueChanged)
    Q_PROPERTY(int thumbProgress READ thumbProgress WRITE setThumbProgress)
    Q_PROPERTY(double apparentValue READ apparentValue WRITE setApparentValue)
    Q_PROPERTY(
        double apparentSecondaryValue READ apparentSecondaryValue WRITE setApparentSecondaryValue)
    Q_PROPERTY(double apparentCurrentTaskValue READ apparentCurrentTaskValue WRITE
                   setApparentCurrentTaskValue)
    Q_PROPERTY(bool indeterminate READ indeterminate WRITE setIndeterminate)
    Q_PROPERTY(QColor inactiveColor READ inactiveColor WRITE setInactiveColor)
    Q_PROPERTY(QColor normalTotalColor READ normalTotalColor WRITE setNormalTotalColor)
    Q_PROPERTY(QColor normalSecondaryColor READ normalSecondaryColor WRITE setNormalSecondaryColor)
    Q_PROPERTY(QColor normalCurrentTaskColor READ normalCurrentTaskColor WRITE
                   setNormalCurrentTaskColor)
    Q_PROPERTY(QColor warningTotalColor READ warningTotalColor WRITE setWarningTotalColor)
    Q_PROPERTY(
        QColor warningSecondaryColor READ warningSecondaryColor WRITE setWarningSecondaryColor)
    Q_PROPERTY(QColor warningCurrentTaskColor READ warningCurrentTaskColor WRITE
                   setWarningCurrentTaskColor)
    Q_PROPERTY(QColor errorTotalColor READ errorTotalColor WRITE setErrorTotalColor)
    Q_PROPERTY(QColor errorSecondaryColor READ errorSecondaryColor WRITE setErrorSecondaryColor)
    Q_PROPERTY(
        QColor errorCurrentTaskColor READ errorCurrentTaskColor WRITE setErrorCurrentTaskColor)

public:
    enum IndicatorStyle {
        HorizontalBar,
        //        VerticalBar,
        Ring
    };

    explicit ProgressIndicator(QWidget *parent = nullptr);
    explicit ProgressIndicator(IndicatorStyle indicatorStyle, QWidget *parent = nullptr);

    [[nodiscard]] double minimum() const;
    [[nodiscard]] double maximum() const;
    [[nodiscard]] double value() const;
    [[nodiscard]] double secondaryValue() const;
    [[nodiscard]] double currentTaskValue() const;
    [[nodiscard]] bool indeterminate() const;
    [[nodiscard]] TaskGlobal::Status taskStatus() const;
    //
    //    QSize sizeHint() const override;
    //    QSize minimumSizeHint() const override;
    //
    //    Qt::Orientation orientation() const;
    //
    //

public slots:
    void reset();
    void setRange(double minimum, double maximum);
    void setMinimum(double minimum);
    void setMaximum(double maximum);
    void setValue(double value);
    void setSecondaryValue(double value);
    void setCurrentTaskValue(double value);
    void setIndeterminate(bool on);
    void setTaskStatus(TaskGlobal::Status status);

signals:
    void valueChanged(double value);
    void secondaryValueChanged(double value);
    void currentTaskValueChanged(double value);

protected:
    struct ColorPalette {
        QColor inactive;
        QColor total;
        QColor secondary;
        QColor currentTask;
    };

    ColorPalette colorPaletteNormal = {QColor(255, 255, 255, 72), QColor(155, 186, 255),
                                       QColor(159, 189, 255), QColor(113, 218, 255)};

    ColorPalette colorPaletteWarning = {QColor(255, 255, 255, 72), QColor(255, 205, 155),
                                        QColor(255, 204, 153), QColor(255, 218, 113)};

    ColorPalette colorPaletteError = {QColor(255, 255, 255, 72), QColor(255, 155, 157),
                                      QColor(255, 171, 173), QColor(255, 171, 221)};

    ColorPalette m_colorPalette;

    // Theme color accessors (QSS-overridable via qproperty-*); the inactive
    // color is shared across the three palettes
    [[nodiscard]] QColor inactiveColor() const;
    void setInactiveColor(const QColor &color);
    [[nodiscard]] QColor normalTotalColor() const;
    void setNormalTotalColor(const QColor &color);
    [[nodiscard]] QColor normalSecondaryColor() const;
    void setNormalSecondaryColor(const QColor &color);
    [[nodiscard]] QColor normalCurrentTaskColor() const;
    void setNormalCurrentTaskColor(const QColor &color);
    [[nodiscard]] QColor warningTotalColor() const;
    void setWarningTotalColor(const QColor &color);
    [[nodiscard]] QColor warningSecondaryColor() const;
    void setWarningSecondaryColor(const QColor &color);
    [[nodiscard]] QColor warningCurrentTaskColor() const;
    void setWarningCurrentTaskColor(const QColor &color);
    [[nodiscard]] QColor errorTotalColor() const;
    void setErrorTotalColor(const QColor &color);
    [[nodiscard]] QColor errorSecondaryColor() const;
    void setErrorSecondaryColor(const QColor &color);
    [[nodiscard]] QColor errorCurrentTaskColor() const;
    void setErrorCurrentTaskColor(const QColor &color);

    // Re-resolve m_colorPalette from the palette matching m_taskStatus
    void applyColorPalette();

    IndicatorStyle m_indicatorStyle = HorizontalBar;
    TaskGlobal::Status m_taskStatus = TaskGlobal::Normal;
    double m_max = 100;
    double m_min = 0;
    double m_value = 0;
    double m_secondaryValue = 0;
    double m_currentTaskValue = 0;
    bool m_indeterminate = false;
    int m_thumbProgress = 0;
    QElapsedTimer m_elapsedTimer;
    double m_indeterminateSpeed = 270.0;
    double m_indeterminateMinLength = 30.0;
    double m_indeterminateMaxLength = 90.0;
    double m_indeterminateFrequency = 0.2;
    int m_penWidth = 0;
    int m_padding = 0;
    int m_halfRectHeight = 0;
    QPoint m_trackStart;
    QPoint m_trackEnd;
    int m_actualLength = 0;
    QRect m_ringRect;
    QTimer m_timer;
    QPropertyAnimation m_valueAnimation;
    QPropertyAnimation m_SecondaryValueAnimation;
    QPropertyAnimation m_currentTaskValueAnimation;

    void initUi();
    void calculateBarParams();
    void calculateRingParams();
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

    void afterSetAnimationLevel(AnimationGlobal::AnimationLevels level) override {
    }

    void afterSetTimeScale(double scale) override {
    }

private:
    double m_apparentValue = 0;
    double m_apparentSecondaryValue = 0;
    double m_apparentCurrentTaskValue = 0;

    [[nodiscard]] int thumbProgress() const;
    void setThumbProgress(int x);
    [[nodiscard]] double apparentValue() const;
    void setApparentValue(double x);
    [[nodiscard]] double apparentSecondaryValue() const;
    void setApparentSecondaryValue(double x);
    [[nodiscard]] double apparentCurrentTaskValue() const;
    void setApparentCurrentTaskValue(double x);
};



#endif // DATASET_TOOLS_PROGRESSINDICATOR_H
