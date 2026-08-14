#ifndef PLAYBACKINDICATOROVERLAY_H
#define PLAYBACKINDICATOROVERLAY_H

#include <QColor>
#include <QWidget>

#include <memory>

#ifdef Q_OS_WIN
class NativePlaybackIndicatorWindow;
#endif

class PlaybackIndicatorOverlay final : public QWidget {
    Q_OBJECT

public:
    enum class Shape { Line, Triangle };
    enum class Surface { SharedWidget, NativeCompositor };

    explicit PlaybackIndicatorOverlay(Shape shape, QWidget *parent = nullptr,
                                      Surface surface = Surface::SharedWidget);
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
    [[nodiscard]] bool usesNativeCompositor() const;
    void setSuppressed(bool suppressed);
    void scheduleRefresh();
    void updateGeometry();
#ifdef Q_OS_WIN
    void ensureNativeWindow();
    void hideNativeWindow() const;
#endif

    Shape m_shape;
    Surface m_surface;
    qreal m_position = 0.0;
    QColor m_color = {200, 200, 200};
    bool m_indicatorVisible = true;
    bool m_suppressed = false;
    bool m_refreshPending = false;
#ifdef Q_OS_WIN
    std::unique_ptr<NativePlaybackIndicatorWindow> m_nativeWindow;
#endif
};

#endif // PLAYBACKINDICATOROVERLAY_H
