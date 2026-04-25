#ifndef OVERLAYSPLITTER_H
#define OVERLAYSPLITTER_H

#include <QSplitter>

class QVariantAnimation;
class SplitterOverlayGrip;

// A QSplitter variant with a zero-width handle that takes no layout space.
// An invisible overlay grip widget is placed on top of the split boundary
// (parented to the splitter's parent widget so QSplitter won't manage it).
// The grip becomes visible on hover and handles drag interaction, forwarding
// size changes back to the splitter via setSizes().
class OverlaySplitter : public QSplitter {
    Q_OBJECT

public:
    explicit OverlaySplitter(Qt::Orientation orientation, QWidget *parent = nullptr);
    explicit OverlaySplitter(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;

private:
    // Lazily creates the overlay grip once a parent widget is available.
    void ensureGrip();
    // Synchronizes the grip's geometry with the current split boundary.
    void updateGripPosition();

    SplitterOverlayGrip *m_grip = nullptr;

    friend class SplitterOverlayGrip;
};

// A transparent widget overlaid on the split boundary that serves as
// the draggable hit area. It is parented to the splitter's parent widget
// (not the splitter itself) to avoid being managed by QSplitter's layout.
// On hover it draws a thin highlight line; on drag it adjusts the splitter
// sizes while respecting child widget min/max constraints.
class SplitterOverlayGrip : public QWidget {
    Q_OBJECT

public:
    explicit SplitterOverlayGrip(OverlaySplitter *splitter, QWidget *parent);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    void setHighlightVisible(bool visible);

    OverlaySplitter *m_splitter;
    QVariantAnimation *m_animation = nullptr;
    qreal m_highlightOpacity = 0.0;
    bool m_hovered = false;
    bool m_dragging = false;
    QPoint m_dragStartPos;
    QList<int> m_dragStartSizes;
    // Hysteresis state for collapse/expand to prevent jitter near the threshold.
    bool m_collapsed0 = false;
    bool m_collapsed1 = false;
};

#endif // OVERLAYSPLITTER_H
