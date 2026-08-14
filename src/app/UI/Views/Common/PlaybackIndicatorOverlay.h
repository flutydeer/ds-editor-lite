#ifndef PLAYBACKINDICATOROVERLAY_H
#define PLAYBACKINDICATOROVERLAY_H

#include <QColor>
#include <QWidget>

class PlaybackIndicatorOverlay final : public QWidget {
    Q_OBJECT

public:
    enum class Shape { Line, Triangle };

    explicit PlaybackIndicatorOverlay(Shape shape, QWidget *parent = nullptr);
    ~PlaybackIndicatorOverlay() override;

    static void setWindowIndicatorsSuppressed(QWidget *window, bool suppressed);

    void setPosition(qreal x);
    void setColor(const QColor &color);
    void setIndicatorVisible(bool visible);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    [[nodiscard]] QRect indicatorRect(qreal position) const;
    [[nodiscard]] bool isPositionVisible(qreal position) const;
    void setSuppressed(bool suppressed);
    void scheduleRefresh();
    void updateGeometry();

    Shape m_shape;
    qreal m_position = 0.0;
    QColor m_color = {200, 200, 200};
    bool m_indicatorVisible = true;
    bool m_suppressed = false;
    bool m_refreshPending = false;
};

#endif // PLAYBACKINDICATOROVERLAY_H
