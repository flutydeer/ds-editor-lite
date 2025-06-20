//
// Created by FlutyDeer on 2025/3/26.
//

#ifndef FADER_H
#define FADER_H

#include <QWidget>

class FaderPrivate;

class Fader : public QWidget {
    Q_OBJECT

    Q_DECLARE_PRIVATE(Fader)

    Q_PROPERTY(QColor trackInactiveColor READ trackInactiveColor WRITE setTrackInactiveColor)
    Q_PROPERTY(QColor trackActiveColor READ trackActiveColor WRITE setTrackActiveColor)
    Q_PROPERTY(QColor thumbFillColor READ thumbFillColor WRITE setThumbFillColor)
    Q_PROPERTY(int animationDuration READ animationDuration WRITE setAnimationDuration)

public:
    explicit Fader(QWidget *parent = nullptr);
    ~Fader() override;

    double sliderPosition() const;
    void setSliderPosition(double position);

    double value() const;

    int animationDuration() const;
    void setAnimationDuration(int dur);

public Q_SLOTS:
    void setValue(double dB);
    void resetValue();

Q_SIGNALS:
    void sliderPressed();
    void sliderReleased();
    void sliderMoved(double gain);
    void valueChanged(double gain);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    explicit Fader(QWidget *parent, FaderPrivate &d);

private:
    QScopedPointer<FaderPrivate> d_ptr;

    QColor trackInactiveColor() const;
    void setTrackInactiveColor(const QColor &color);
    QColor trackActiveColor() const;
    void setTrackActiveColor(const QColor &color);
    QColor thumbFillColor() const;
    void setThumbFillColor(const QColor &color);
};

#endif // FADER_H