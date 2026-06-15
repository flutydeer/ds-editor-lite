//
// Created by FlutyDeer on 2026/6/15.
//

#ifndef COLORDOT_H
#define COLORDOT_H

#include <QColor>
#include <QWidget>

class ColorDot final : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double radius READ radius WRITE setRadius NOTIFY radiusChanged)
    Q_PROPERTY(QColor color READ color WRITE setColor NOTIFY colorChanged)

public:
    explicit ColorDot(QWidget *parent = nullptr);
    explicit ColorDot(const QColor &color, QWidget *parent = nullptr);

    [[nodiscard]] double radius() const;
    void setRadius(double radius);

    [[nodiscard]] QColor color() const;
    void setColor(const QColor &color);

signals:
    void radiusChanged(double radius);
    void colorChanged(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    static constexpr int defaultSize = 24;
    double m_radius = 6.0;
    QColor m_color = Qt::white;
};

#endif // COLORDOT_H
