#ifndef EMBEDDEDMODALHOST_H
#define EMBEDDEDMODALHOST_H

#include <QWidget>

class QColor;
class QShortcut;

// Embedded modal host: a layout-less overlay child of the main window
// (following the floating-grip pattern of OverlaySplitter — not managed by
// any parent layout; the owner keeps its geometry in sync in resizeEvent and
// raises it when opening). It fills the whole window and paints a translucent
// backdrop directly in paintEvent. QGraphicsEffect is deliberately avoided:
// stacked effects break the painting of QSS-styled widgets on Windows
// ("QPainter::begin: a paint device can only be painted by one painter at a
// time"). The centered content panel stays visible over the live editor
// behind it, mimicking a web-style modal. Open/close are instantaneous (no
// animation) — animated repaints of the translucent backdrop stuttered even
// in Release builds.
class EmbeddedModalHost : public QWidget {
    Q_OBJECT

public:
    explicit EmbeddedModalHost(QWidget *parent = nullptr);

    // Shows content as the panel body; panelSize falls back to the content
    // sizeHint when invalid.
    void open(QWidget *content, const QSize &panelSize = QSize());
    // Hides the panel and emits closed() immediately.
    void closePanel();
    [[nodiscard]] bool isOpen() const;

signals:
    void opened();
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    // Clicks on the backdrop close the modal (Esc does too); presses that
    // bubble up from blank areas inside the panel are swallowed without
    // closing, and nothing passes through to the editor below.
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
    void applyTheme();
    void anchorPanelToCenter();
    void resetPanelGeometry();
    [[nodiscard]] QPoint centerPosition(const QSize &size) const;

    QWidget *m_panel = nullptr;
    QWidget *m_content = nullptr;
    QShortcut *m_escShortcut = nullptr;

    QSize m_panelSize;
    QPoint m_panelRestPos;
    QColor m_backdropColor;
    bool m_open = false;
};

#endif // EMBEDDEDMODALHOST_H
