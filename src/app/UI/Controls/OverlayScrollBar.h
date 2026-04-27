#ifndef OVERLAYSCROLLBAR_H
#define OVERLAYSCROLLBAR_H

#include <QScrollBar>

class QAbstractScrollArea;
class QVariantAnimation;

class OverlayScrollBar : public QScrollBar {
    Q_OBJECT

public:
    explicit OverlayScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);

    void attachTo(QAbstractScrollArea *scrollArea);
    void updatePosition();

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    void setHighlightVisible(bool visible);

    QAbstractScrollArea *m_scrollArea = nullptr;
    QVariantAnimation *m_animation = nullptr;
    qreal m_opacity = 0.0;
    bool m_hovered = false;
};

#endif // OVERLAYSCROLLBAR_H
