#ifndef DSEDITORLITE_SMOOTHSCROLLER_H
#define DSEDITORLITE_SMOOTHSCROLLER_H

#include "WheelInputController.h"

#include <QObject>

class QAbstractScrollArea;
class QScrollBar;
class QWheelEvent;

/// Smoothly scrolls a managed scroll area: intercepts wheel events on its viewport
/// and drives the scrollbar value via an OutCubic animation. Real mouse wheels
/// (angleDelta multiples of 120) animate; touchpads (fractional/pixel deltas) pass through.
class SmoothScroller : public QObject {
    Q_OBJECT

public:
    explicit SmoothScroller(QObject *parent = nullptr);
    /// Binds a scroll area (installs this object as an event filter on its viewport).
    void attachTo(QAbstractScrollArea *area);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    [[nodiscard]] QScrollBar *scrollBar(Qt::Orientation orientation) const;
    [[nodiscard]] bool canAnimateScroll(Qt::Orientation orientation) const;
    [[nodiscard]] double scrollStep(Qt::Orientation orientation) const;

    QAbstractScrollArea *m_area = nullptr;
    WheelInputController m_wheelInput;
};

#endif // DSEDITORLITE_SMOOTHSCROLLER_H
