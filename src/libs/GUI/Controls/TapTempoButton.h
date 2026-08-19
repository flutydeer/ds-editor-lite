#ifndef TAPTEMPOBUTTON_H
#define TAPTEMPOBUTTON_H

#include <lite/GUI/Animation/IAnimatable.h>
#include <lite/GUI/Controls/Button.h>

#include <QColor>
#include <QPropertyAnimation>

class TapTempoButton : public Button, public IAnimatable {
    Q_OBJECT
    Q_PROPERTY(double apparentProgress READ apparentProgress WRITE setApparentProgress)
    Q_PROPERTY(QColor progressColor READ progressColor WRITE setProgressColor)
    Q_PROPERTY(bool stable READ isStable WRITE setStable)

public:
    explicit TapTempoButton(QWidget *parent = nullptr);

    [[nodiscard]] double progress() const;
    void setProgress(double progress);
    void setProgressImmediately(double progress);

    [[nodiscard]] QColor progressColor() const;
    void setProgressColor(const QColor &color);

    [[nodiscard]] bool isStable() const;
    void setStable(bool stable);

protected:
    void paintEvent(QPaintEvent *event) override;
    void afterSetAnimationEnabled(bool enabled) override;
    void afterSetTimeScale(double scale) override;

private:
    [[nodiscard]] double apparentProgress() const;
    void setApparentProgress(double progress);
    void updateAnimationSettings();

    double m_progress = 0.0;
    QColor m_progressColor = QColor(155, 186, 255, 80);
    bool m_stable = false;
    QPropertyAnimation m_progressAnimation;
};

#endif // TAPTEMPOBUTTON_H
