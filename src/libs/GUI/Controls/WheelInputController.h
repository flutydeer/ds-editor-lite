#ifndef DSEDITORLITE_WHEELINPUTCONTROLLER_H
#define DSEDITORLITE_WHEELINPUTCONTROLLER_H

#include <lite/GUI/Animation/IAnimatable.h>

#include <QPoint>
#include <QTimer>
#include <QVariantAnimation>

#include <functional>
#include <optional>

class QWheelEvent;

namespace WheelInput {

    [[nodiscard]] double axisValue(const QPoint &delta, Qt::Orientation axis);
    [[nodiscard]] bool usesPixelDelta(const QWheelEvent *event);
    [[nodiscard]] double zoomDelta(const QWheelEvent *event, Qt::Orientation axis);
    [[nodiscard]] Qt::Orientation dominantAxis(const QWheelEvent *event);

    class DeviceState final {
    public:
        DeviceState();
        DeviceState(const DeviceState &) = delete;
        DeviceState &operator=(const DeviceState &) = delete;

        [[nodiscard]] bool isDiscrete(const QWheelEvent *event);

    private:
#if !defined(Q_OS_MAC) || QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        QTimer m_unlockTimer;
        bool m_continuousInputLocked = false;
#endif
    };

} // namespace WheelInput

class WheelInputController final : public QObject, public IAnimatable {
    Q_OBJECT

public:
    enum class Action {
        Automatic,
        HorizontalScroll,
        VerticalScroll,
        HorizontalZoom,
        VerticalZoom,
    };

    enum class ContinuousInputMode { Handle, PassThrough };

    struct ScrollTarget {
        std::function<double()> value;
        std::function<void(double)> setValue;
        std::function<double(double)> boundedValue;
        std::function<double()> step;
        std::function<bool()> canScroll;
    };

    struct ZoomTarget {
        std::function<double()> value;
        std::function<void(double, double)> setValueAt;
        std::function<double(double)> boundedValue;
        double step = 0.0;
    };

    explicit WheelInputController(QObject *parent = nullptr);

    void setScrollTarget(Qt::Orientation orientation, ScrollTarget target);
    void setZoomTarget(Qt::Orientation orientation, ZoomTarget target);
    void setContinuousInputMode(ContinuousInputMode mode);
    void setDiscreteAnimationEnabled(std::function<bool()> enabled);

    bool handleWheel(QWheelEvent *event, Action action = Action::Automatic,
                     std::optional<Qt::Orientation> sourceAxis = std::nullopt);
    bool zoomByFactor(Qt::Orientation orientation, double factor, double anchor);
    void stop();

    [[nodiscard]] std::optional<double> logicalScrollValue(Qt::Orientation orientation) const;

protected:
    void afterSetAnimationEnabled(bool enabled) override;
    void afterSetTimeScale(double scale) override;

private:
    struct Motion {
        QVariantAnimation animation;
        std::optional<double> logicalValue;
        double remainder = 0.0;
    };

    [[nodiscard]] Action resolveAction(const QWheelEvent *event, Action action) const;
    [[nodiscard]] Qt::Orientation
        resolveSourceAxis(const QWheelEvent *event, Action action,
                          std::optional<Qt::Orientation> sourceAxis) const;
    bool handleScroll(QWheelEvent *event, Qt::Orientation targetAxis, Qt::Orientation sourceAxis,
                      bool discrete);
    bool handleZoom(QWheelEvent *event, Qt::Orientation targetAxis, Qt::Orientation sourceAxis,
                    bool discrete);
    void startMotion(Motion &motion, double currentValue, double targetValue);
    void stopMotion(Motion &motion, bool resetRemainder = true);
    void updateAnimationDuration();

    [[nodiscard]] ScrollTarget &scrollTarget(Qt::Orientation orientation);
    [[nodiscard]] const ScrollTarget &scrollTarget(Qt::Orientation orientation) const;
    [[nodiscard]] ZoomTarget &zoomTarget(Qt::Orientation orientation);
    [[nodiscard]] const ZoomTarget &zoomTarget(Qt::Orientation orientation) const;
    [[nodiscard]] Motion &scrollMotion(Qt::Orientation orientation);
    [[nodiscard]] const Motion &scrollMotion(Qt::Orientation orientation) const;
    [[nodiscard]] Motion &zoomMotion(Qt::Orientation orientation);

    static constexpr int kAnimationDuration = 250;

    ScrollTarget m_horizontalScrollTarget;
    ScrollTarget m_verticalScrollTarget;
    ZoomTarget m_horizontalZoomTarget;
    ZoomTarget m_verticalZoomTarget;
    Motion m_horizontalScroll;
    Motion m_verticalScroll;
    Motion m_horizontalZoom;
    Motion m_verticalZoom;
    double m_horizontalZoomAnchor = 0.0;
    double m_verticalZoomAnchor = 0.0;
    WheelInput::DeviceState m_inputState;
    ContinuousInputMode m_continuousInputMode = ContinuousInputMode::Handle;
    std::function<bool()> m_discreteAnimationEnabled = [] { return true; };
};

#endif // DSEDITORLITE_WHEELINPUTCONTROLLER_H
