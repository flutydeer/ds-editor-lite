//
// Created by FlutyDeer on 2025/6/11.
//

#ifndef PANSLIDER_H
#define PANSLIDER_H

#include <QWidget>

class PanSliderPrivate;

class PanSlider : public QWidget {
    Q_OBJECT

    Q_DECLARE_PRIVATE(PanSlider)

    Q_PROPERTY(QColor centerGraduateColor READ centerGraduateColor WRITE setCenterGraduateColor)
    Q_PROPERTY(QColor trackActiveColor READ trackActiveColor WRITE setTrackActiveColor)
    Q_PROPERTY(QColor thumbFillColor READ thumbFillColor WRITE setThumbFillColor)
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration)

public:
    explicit PanSlider(QWidget *parent = nullptr);
    ~PanSlider() override;

    double sliderPosition() const;
    void setSliderPosition(double position);

    double value() const;

    int animationDuration() const;
    void setAnimationDuration(int dur);

public Q_SLOTS:
    void setValue(double pan);
    void resetValue();

Q_SIGNALS:
    void sliderPressed();
    void sliderReleased();
    void sliderMoved(double gain);
    void valueChanged(double gain);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    explicit PanSlider(QWidget *parent, PanSliderPrivate &d);

private:
    QScopedPointer<PanSliderPrivate> d_ptr;

    QColor centerGraduateColor() const;
    void setCenterGraduateColor(const QColor &color);
    QColor trackActiveColor() const;
    void setTrackActiveColor(const QColor &color);
    QColor thumbFillColor() const;
    void setThumbFillColor(const QColor &color);
};

#endif //PANSLIDER_H