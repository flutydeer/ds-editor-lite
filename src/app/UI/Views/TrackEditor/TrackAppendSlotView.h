#ifndef TRACKAPPENDSLOTVIEW_H
#define TRACKAPPENDSLOTVIEW_H

#include <lite/GUI/Controls/WheelEventPolicy.h>

#include <QColor>
#include <QWidget>

// Placeholder row widget for the virtual append slot at the bottom of the
// track list. It only mirrors the canvas append slot visually: disabled,
// non-selectable and non-draggable, never part of the track model.
class TrackAppendSlotView final : public QWidget, private WheelEventPolicySupport {
    Q_OBJECT
    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor)

public:
    explicit TrackAppendSlotView(QWidget *parent = nullptr);

    [[nodiscard]] QColor lineColor() const;
    void setLineColor(const QColor &color);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

private:
    QColor m_lineColor{0x2F, 0x33, 0x38};
};

#endif // TRACKAPPENDSLOTVIEW_H
