//
// Created by fluty on 2023/8/14.
//

#ifndef DATASET_TOOLS_PROGRESSINDICATOR_H
#define DATASET_TOOLS_PROGRESSINDICATOR_H

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
    //    Q_PROPERTY(bool invertedAppearance READ invertedAppearance WRITE setInvertedAppearance)
    Q_PROPERTY(int thumbProgress READ thumbProgress WRITE setThumbProgress)
    Q_PROPERTY(double apparentValue READ apparentValue WRITE setApparentValue)
    Q_PROPERTY(
        double apparentSecondaryValue READ apparentSecondaryValue WRITE setApparentSecondaryValue)
    Q_PROPERTY(double apparentCurrentTaskValue READ apparentCurrentTaskValue WRITE
                   setApparentCurrentTaskValue)
    Q_PROPERTY(bool indeterminate READ indeterminate WRITE setIndeterminate)

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
    //    void setInvertedAppearance(bool invert);
    //    bool invertedAppearance() const;
    //
    //    void setFormat(const QString &format);
    //    void resetFormat();
    //    QString format() const;

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

    IndicatorStyle m_indicatorStyle = HorizontalBar;
    TaskGlobal::Status m_taskStatus = TaskGlobal::Normal;
    double m_max = 100;
    double m_min = 0;
    double m_value = 0;
    double m_secondaryValue = 0;
    double m_currentTaskValue = 0;
    bool m_indeterminate = false;
    int m_thumbProgress = 0;
    int m_penWidth = 0;
    int m_padding = 0;
    int m_halfRectHeight = 0;
    QPoint m_trackStart;
    QPoint m_trackEnd;
    int m_actualLength = 0;
    QRect m_ringRect;
    //    bool m_invertedAppearance = false;
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
