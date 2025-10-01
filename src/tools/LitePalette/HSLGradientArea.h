//
// Created by FlutyDeer on 2025/6/14.
//

#ifndef HSLGRADIENTAREA_H
#define HSLGRADIENTAREA_H

#include <QWidget>

class HSLGradientArea : public QWidget {
    Q_OBJECT

public:
    explicit HSLGradientArea(QWidget *parent = nullptr);

private:
    void resizeEvent(QResizeEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

    static float normalizeHue(float hue);
    static float hueDifference(float h1, float h2);
    QColor getInterpolatedColor(const QColor &c1, const QColor &c2, double factor);

    QPixmap pixmap_;
    QColor colorStart_ = {205,188,105};
    QColor colorEnd_ = {152,190,247};
};


#endif //HSLGRADIENTAREA_H