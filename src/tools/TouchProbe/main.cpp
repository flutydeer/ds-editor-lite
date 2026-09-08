// TouchProbe — 输入探针
//
// A standalone diagnostic window that visualizes every pointer event the
// platform delivers, so the touch/pen behaviour of a real machine can be
// checked before trusting it in the editor. It answers three questions the
// desktop application cannot answer on its own:
//
//   1. Does touch reach Qt as QTouchEvent, or only as synthesized mouse input?
//   2. Does a stylus produce QTabletEvent, a mouse event, or both?
//   3. What exactly does Direct Manipulation swallow once it is registered?
//
// Accepting a QTouchEvent stops Qt's own touch to mouse synthesis but not the
// operating system's: Windows promotes the primary touch point to legacy mouse
// messages regardless. The "swallow synthesized mouse" switch applies the same
// rule the editor uses (drop everything whose source is not
// Qt::MouseEventNotSynthesized) so the effect can be seen side by side.
//
// Trails are colored per device: mouse blue, touch green (one hue per point
// id), pen red, wheel and native gestures yellow. Synthesized mouse events are
// drawn as a grey dashed trail, which makes an unexpected synthesis obvious at
// a glance.
//
// Keys: C clear, D toggle Direct Manipulation, S toggle swallowing, F
// fullscreen, Esc quit.

#include <QApplication>
#include <QCheckBox>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QHash>
#include <QPair>
#include <QStringList>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStandardPaths>
#include <QTabletEvent>
#include <QTextStream>
#include <QTouchEvent>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>
#include <QWindow>

#include <algorithm>

#if defined(WITH_DIRECT_MANIPULATION)
#  include <QWDMHCore/DirectManipulationSystem.h>
#endif

namespace {

    constexpr int maximumTrailPoints = 400;
    constexpr int maximumHudLines = 22;
    // A trail with no explicit end (hover, wheel, native gestures) is cut here
    // so that two separate movements never get joined by a straight line.
    constexpr qint64 strokeBreakMs = 300;

    enum class DeviceKind { Mouse, SynthesizedMouse, Touch, Pen, Wheel, Gesture };

    QString deviceKindName(const DeviceKind kind) {
        switch (kind) {
            case DeviceKind::Mouse:
                return QStringLiteral("mouse");
            case DeviceKind::SynthesizedMouse:
                return QStringLiteral("mouse(synth)");
            case DeviceKind::Touch:
                return QStringLiteral("touch");
            case DeviceKind::Pen:
                return QStringLiteral("pen");
            case DeviceKind::Wheel:
                return QStringLiteral("wheel");
            case DeviceKind::Gesture:
                return QStringLiteral("gesture");
        }
        return QStringLiteral("?");
    }

    QColor deviceKindColor(const DeviceKind kind, const int pointId) {
        // One shade per touch point id so two fingers never share a trail
        // color, all kept inside the green family so that a finger is never
        // mistaken for the blue mouse, the red pen or the yellow wheel.
        static const QColor touchColors[] = {
            {60, 220, 120}, {0, 205, 190}, {150, 230, 60}, {0, 225, 255}, {120, 235, 165},
        };
        switch (kind) {
            case DeviceKind::Mouse:
                return {90, 150, 255};
            case DeviceKind::SynthesizedMouse:
                return {150, 150, 150};
            case DeviceKind::Touch:
                return touchColors[std::abs(pointId) % std::size(touchColors)];
            case DeviceKind::Pen:
                return {255, 90, 90};
            case DeviceKind::Wheel:
            case DeviceKind::Gesture:
                return {235, 210, 80};
        }
        return Qt::white;
    }

    QString deviceTypeName(const QInputDevice *device) {
        if (!device)
            return QStringLiteral("null");
        switch (device->type()) {
            case QInputDevice::DeviceType::Mouse:
                return QStringLiteral("Mouse");
            case QInputDevice::DeviceType::TouchScreen:
                return QStringLiteral("TouchScreen");
            case QInputDevice::DeviceType::TouchPad:
                return QStringLiteral("TouchPad");
            case QInputDevice::DeviceType::Stylus:
                return QStringLiteral("Stylus");
            case QInputDevice::DeviceType::Airbrush:
                return QStringLiteral("Airbrush");
            case QInputDevice::DeviceType::Puck:
                return QStringLiteral("Puck");
            case QInputDevice::DeviceType::Keyboard:
                return QStringLiteral("Keyboard");
            default:
                break;
        }
        return QStringLiteral("Unknown");
    }

    QString mouseSourceName(const Qt::MouseEventSource source) {
        switch (source) {
            case Qt::MouseEventNotSynthesized:
                return QStringLiteral("NotSynthesized");
            case Qt::MouseEventSynthesizedBySystem:
                return QStringLiteral("BySystem");
            case Qt::MouseEventSynthesizedByQt:
                return QStringLiteral("ByQt");
            case Qt::MouseEventSynthesizedByApplication:
                return QStringLiteral("ByApplication");
        }
        return QStringLiteral("?");
    }

    struct Trail {
        DeviceKind kind = DeviceKind::Mouse;
        int pointId = 0;
        // One entry per stroke. A finger lifting and landing somewhere else
        // must not be drawn as one continuous line.
        QList<QList<QPointF>> strokes;
        bool open = false;
        qint64 lastAppendMs = 0;

        [[nodiscard]] qsizetype pointCount() const {
            qsizetype count = 0;
            for (const auto &stroke : strokes)
                count += stroke.size();
            return count;
        }
    };

    class ProbeCanvas final : public QWidget {
    public:
        explicit ProbeCanvas(QWidget *parent = nullptr) : QWidget(parent) {
            setAttribute(Qt::WA_AcceptTouchEvents);
            setMouseTracking(true);
            setTabletTracking(true);
            setFocusPolicy(Qt::StrongFocus);
            setAutoFillBackground(false);
            m_clock.start();

            const auto directory =
                QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
            QDir().mkpath(directory);
            m_logFile.setFileName(directory + QStringLiteral("/touch-probe.log"));
            if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append))
                qWarning("TouchProbe: cannot open the log file, HUD only");
            log(QStringLiteral("--- session started, log at %1").arg(m_logFile.fileName()));
        }

        void clear() {
            m_trails.clear();
            m_hud.clear();
            m_activePoints = 0;
            update();
        }

        void setSwallowSynthesizedMouse(const bool on) {
            m_swallowSynthesizedMouse = on;
            log(QStringLiteral("--- swallow synthesized mouse: %1")
                    .arg(on ? QStringLiteral("on") : QStringLiteral("off")));
            update();
        }

        void logExternal(const QString &line) {
            log(line);
            update();
        }

        [[nodiscard]] QString logPath() const {
            return m_logFile.fileName();
        }

    protected:
        bool event(QEvent *event) override {
            switch (event->type()) {
                case QEvent::TouchBegin:
                case QEvent::TouchUpdate:
                case QEvent::TouchEnd:
                case QEvent::TouchCancel:
                    recordTouch(static_cast<QTouchEvent *>(event));
                    // Accepting TouchBegin suppresses Qt's own mouse synthesis,
                    // which is the whole point: an extra grey trail after this
                    // means the synthesis came from the platform, not from Qt.
                    event->accept();
                    return true;
                case QEvent::NativeGesture:
                    recordGesture(static_cast<QNativeGestureEvent *>(event));
                    return true;
                case QEvent::ContextMenu: {
                    // Windows turns a touch press and hold into a right click,
                    // which reaches the application as a context menu on top of
                    // whatever the long press already did.
                    const auto *menuEvent = static_cast<QContextMenuEvent *>(event);
                    log(QStringLiteral("menu     reason=%1 pos=(%2,%3)")
                            .arg(contextMenuReasonName(menuEvent->reason()))
                            .arg(menuEvent->pos().x())
                            .arg(menuEvent->pos().y()));
                    update();
                    return true;
                }
                default:
                    break;
            }
            return QWidget::event(event);
        }

        void mousePressEvent(QMouseEvent *event) override {
            recordMouse(event, QStringLiteral("press"));
        }

        void mouseMoveEvent(QMouseEvent *event) override {
            recordMouse(event, QStringLiteral("move"));
        }

        void mouseReleaseEvent(QMouseEvent *event) override {
            recordMouse(event, QStringLiteral("release"));
            endStroke(DeviceKind::Mouse, -1);
            endStroke(DeviceKind::SynthesizedMouse, -4);
        }

        void tabletEvent(QTabletEvent *event) override {
            const auto kind = DeviceKind::Pen;
            appendPoint(kind, -2, event->position());
            log(QStringLiteral("tablet   %1 dev=%2 pressure=%3 tilt=(%4,%5) pos=(%6,%7) buttons=%8")
                    .arg(tabletActionName(event->type()), deviceTypeName(event->device()))
                    .arg(event->pressure(), 0, 'f', 2)
                    .arg(event->xTilt())
                    .arg(event->yTilt())
                    .arg(event->position().x(), 0, 'f', 1)
                    .arg(event->position().y(), 0, 'f', 1)
                    .arg(static_cast<int>(event->buttons())));
            if (event->type() == QEvent::TabletRelease ||
                event->type() == QEvent::TabletLeaveProximity)
                endStroke(kind, -2);
            // Deliberately left unaccepted: that is how the editor gets the
            // stylus as ordinary mouse input.
            event->ignore();
            update();
        }

        void wheelEvent(QWheelEvent *event) override {
            appendPoint(DeviceKind::Wheel, -3, event->position());
            log(QStringLiteral("wheel    dev=%1 angle=(%2,%3) pixel=(%4,%5) phase=%6 inverted=%7")
                    .arg(deviceTypeName(event->device()))
                    .arg(event->angleDelta().x())
                    .arg(event->angleDelta().y())
                    .arg(event->pixelDelta().x())
                    .arg(event->pixelDelta().y())
                    .arg(static_cast<int>(event->phase()))
                    .arg(event->inverted()));
            event->accept();
            update();
        }

        void paintEvent(QPaintEvent *) override {
            QPainter painter(this);
            painter.fillRect(rect(), QColor(24, 26, 30));
            painter.setRenderHint(QPainter::Antialiasing);

            for (const auto &trail : m_trails) {
                if (trail.strokes.isEmpty())
                    continue;
                QPen pen(deviceKindColor(trail.kind, trail.pointId));
                pen.setWidthF(trail.kind == DeviceKind::Pen ? 3.0 : 2.0);
                if (trail.kind == DeviceKind::SynthesizedMouse) {
                    pen.setStyle(Qt::DashLine);
                    pen.setWidthF(2.0);
                }
                painter.setPen(pen);
                for (const auto &stroke : trail.strokes) {
                    if (stroke.size() < 2) {
                        if (stroke.size() == 1)
                            painter.drawPoint(stroke.first());
                        continue;
                    }
                    QPainterPath path(stroke.first());
                    for (qsizetype i = 1; i < stroke.size(); ++i)
                        path.lineTo(stroke.at(i));
                    painter.drawPath(path);
                }
                painter.setBrush(pen.color());
                painter.drawEllipse(trail.strokes.last().last(), 5.0, 5.0);
                painter.setBrush(Qt::NoBrush);
            }

            paintHud(painter);
        }

        void keyPressEvent(QKeyEvent *event) override {
            if (event->key() == Qt::Key_C) {
                clear();
                return;
            }
            QWidget::keyPressEvent(event);
        }

    private:
        static QString tabletActionName(const QEvent::Type type) {
            switch (type) {
                case QEvent::TabletPress:
                    return QStringLiteral("press");
                case QEvent::TabletMove:
                    return QStringLiteral("move");
                case QEvent::TabletRelease:
                    return QStringLiteral("release");
                case QEvent::TabletEnterProximity:
                    return QStringLiteral("enter");
                case QEvent::TabletLeaveProximity:
                    return QStringLiteral("leave");
                default:
                    break;
            }
            return QStringLiteral("?");
        }

        void recordMouse(QMouseEvent *event, const QString &action) {
            const auto synthesized = event->source() != Qt::MouseEventNotSynthesized;
            const auto swallowed = synthesized && m_swallowSynthesizedMouse;
            if (!swallowed) {
                const auto kind = synthesized ? DeviceKind::SynthesizedMouse : DeviceKind::Mouse;
                appendPoint(kind, synthesized ? -4 : -1, event->position());
            }
            log(QStringLiteral("mouse    %1 dev=%2 source=%3 pos=(%4,%5) buttons=%6%7")
                    .arg(action, deviceTypeName(event->pointingDevice()),
                         mouseSourceName(event->source()))
                    .arg(event->position().x(), 0, 'f', 1)
                    .arg(event->position().y(), 0, 'f', 1)
                    .arg(static_cast<int>(event->buttons()))
                    .arg(swallowed ? QStringLiteral(" SWALLOWED") : QString()));
            event->accept();
            update();
        }

        static QString contextMenuReasonName(const QContextMenuEvent::Reason reason) {
            switch (reason) {
                case QContextMenuEvent::Mouse:
                    return QStringLiteral("Mouse");
                case QContextMenuEvent::Keyboard:
                    return QStringLiteral("Keyboard");
                case QContextMenuEvent::Other:
                    return QStringLiteral("Other");
            }
            return QStringLiteral("?");
        }

        void recordTouch(QTouchEvent *event) {
            m_activePoints = 0;
            for (const auto &point : event->points()) {
                if (point.state() == QEventPoint::State::Released) {
                    endStroke(DeviceKind::Touch, point.id());
                    continue;
                }
                ++m_activePoints;
                appendPoint(DeviceKind::Touch, point.id(), point.position());
            }
            log(QStringLiteral("touch    %1 dev=%2 points=%3 [%4]")
                    .arg(touchActionName(event->type()), deviceTypeName(event->pointingDevice()))
                    .arg(event->points().size())
                    .arg(describePoints(event)));
            update();
        }

        void recordGesture(const QNativeGestureEvent *event) {
            appendPoint(DeviceKind::Gesture, -5, event->position());
            log(QStringLiteral("gesture  type=%1 value=%2 dev=%3 pos=(%4,%5)")
                    .arg(static_cast<int>(event->gestureType()))
                    .arg(event->value(), 0, 'f', 4)
                    .arg(deviceTypeName(event->device()))
                    .arg(event->position().x(), 0, 'f', 1)
                    .arg(event->position().y(), 0, 'f', 1));
            update();
        }

        static QString touchActionName(const QEvent::Type type) {
            switch (type) {
                case QEvent::TouchBegin:
                    return QStringLiteral("begin  ");
                case QEvent::TouchUpdate:
                    return QStringLiteral("update ");
                case QEvent::TouchEnd:
                    return QStringLiteral("end    ");
                case QEvent::TouchCancel:
                    return QStringLiteral("cancel ");
                default:
                    break;
            }
            return QStringLiteral("?");
        }

        static QString describePoints(const QTouchEvent *event) {
            QStringList parts;
            for (const auto &point : event->points()) {
                parts.append(QStringLiteral("#%1:%2 (%3,%4)")
                                 .arg(point.id())
                                 .arg(static_cast<int>(point.state()))
                                 .arg(point.position().x(), 0, 'f', 0)
                                 .arg(point.position().y(), 0, 'f', 0));
            }
            return parts.join(QStringLiteral(", "));
        }

        void appendPoint(const DeviceKind kind, const int pointId, const QPointF &position) {
            const auto key = QPair<int, int>(static_cast<int>(kind), pointId);
            auto &trail = m_trails[key];
            trail.kind = kind;
            trail.pointId = pointId;
            const auto elapsed = m_clock.elapsed();
            if (!trail.open || trail.strokes.isEmpty() ||
                elapsed - trail.lastAppendMs > strokeBreakMs) {
                trail.strokes.append(QList<QPointF>());
                trail.open = true;
            }
            trail.strokes.last().append(position);
            trail.lastAppendMs = elapsed;
            while (trail.pointCount() > maximumTrailPoints && !trail.strokes.isEmpty()) {
                trail.strokes.first().removeFirst();
                if (trail.strokes.first().isEmpty())
                    trail.strokes.removeFirst();
            }
        }

        // The pointer left the surface: whatever comes next starts a new stroke.
        void endStroke(const DeviceKind kind, const int pointId) {
            const auto key = QPair<int, int>(static_cast<int>(kind), pointId);
            const auto iterator = m_trails.find(key);
            if (iterator != m_trails.end())
                iterator->open = false;
        }

        void log(const QString &line) {
            const auto stamped = QStringLiteral("%1 %2").arg(
                QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")), line);
            m_hud.append(stamped);
            while (m_hud.size() > maximumHudLines)
                m_hud.removeFirst();
            if (m_logFile.isOpen()) {
                QTextStream stream(&m_logFile);
                stream << stamped << '\n';
            }
        }

        void paintHud(QPainter &painter) const {
            QFont font(QStringLiteral("Consolas"));
            font.setStyleHint(QFont::Monospace);
            font.setPixelSize(12);
            painter.setFont(font);

            const QRectF panel(8, 8, std::min(760.0, width() - 16.0),
                               24.0 + maximumHudLines * 14.0);
            painter.fillRect(panel, QColor(0, 0, 0, 170));
            painter.setPen(QColor(220, 220, 220));

            const auto header =
                QStringLiteral("touch points: %1    dpr: %2    swallow synth mouse: %3    "
                               "C clear / D toggle DM / S toggle swallow")
                    .arg(m_activePoints)
                    .arg(devicePixelRatioF(), 0, 'f', 2)
                    .arg(m_swallowSynthesizedMouse ? QStringLiteral("on") : QStringLiteral("off"));
            painter.drawText(QPointF(panel.left() + 8, panel.top() + 16), header);

            double y = panel.top() + 34;
            for (const auto &line : m_hud) {
                painter.drawText(QPointF(panel.left() + 8, y), line);
                y += 14.0;
            }
        }

        QHash<QPair<int, int>, Trail> m_trails;
        QStringList m_hud;
        int m_activePoints = 0;
        bool m_swallowSynthesizedMouse = false;
        QElapsedTimer m_clock;
        QFile m_logFile;
    };

    class ProbeWindow final : public QWidget {
    public:
        ProbeWindow() {
            setWindowTitle(QStringLiteral("TouchProbe — pointer input probe"));
            resize(1200, 800);

            m_canvas = new ProbeCanvas(this);

            auto *clearButton = new QPushButton(QStringLiteral("Clear (C)"), this);
            connect(clearButton, &QPushButton::clicked, m_canvas, &ProbeCanvas::clear);

            m_directManipulationButton = new QPushButton(this);
            m_directManipulationButton->setCheckable(true);
#if defined(WITH_DIRECT_MANIPULATION)
            connect(m_directManipulationButton, &QPushButton::toggled, this,
                    &ProbeWindow::setDirectManipulationEnabled);
#else
            m_directManipulationButton->setEnabled(false);
#endif
            updateDirectManipulationButton();

            m_swallowButton = new QPushButton(this);
            m_swallowButton->setCheckable(true);
            connect(m_swallowButton, &QPushButton::toggled, this, [this](const bool on) {
                m_canvas->setSwallowSynthesizedMouse(on);
                updateSwallowButton();
            });
            updateSwallowButton();

            m_statusLabel = new QLabel(this);
            m_statusLabel->setText(m_canvas->logPath());
            m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

            auto *controls = new QHBoxLayout;
            controls->addWidget(clearButton);
            controls->addWidget(m_directManipulationButton);
            controls->addWidget(m_swallowButton);
            controls->addWidget(m_statusLabel, 1);

            auto *layout = new QVBoxLayout(this);
            layout->addLayout(controls);
            layout->addWidget(m_canvas, 1);
        }

    protected:
        void keyPressEvent(QKeyEvent *event) override {
            switch (event->key()) {
                case Qt::Key_Escape:
                    close();
                    return;
                case Qt::Key_C:
                    m_canvas->clear();
                    return;
                case Qt::Key_D:
                    m_directManipulationButton->toggle();
                    return;
                case Qt::Key_S:
                    m_swallowButton->toggle();
                    return;
                case Qt::Key_F:
                    isFullScreen() ? showNormal() : showFullScreen();
                    return;
                default:
                    break;
            }
            QWidget::keyPressEvent(event);
        }

    private:
#if defined(WITH_DIRECT_MANIPULATION)
        void setDirectManipulationEnabled(const bool enabled) {
            using System = QWDMH::DirectManipulationSystem;
            auto *handle = windowHandle();
            if (!handle)
                return;
            if (enabled) {
                // The exact configuration the application registers: touchpad
                // and wheel only, so touch and pen stay visible to Qt.
                System::registerWindow(handle,
                                       System::TranslationX | System::TranslationY |
                                           System::Scaling | System::TranslationInertia |
                                           System::ScalingInertia,
                                       System::Touchpad | System::Wheel);
            } else {
                System::unregisterWindow(handle);
            }
            m_canvas->logExternal(QStringLiteral("--- direct manipulation: %1")
                                      .arg(enabled ? QStringLiteral("registered Touchpad|Wheel")
                                                   : QStringLiteral("unregistered")));
            updateDirectManipulationButton();
        }
#endif

        void updateSwallowButton() {
            m_swallowButton->setText(m_swallowButton->isChecked()
                                         ? QStringLiteral("Swallow synth mouse: on (S)")
                                         : QStringLiteral("Swallow synth mouse: off (S)"));
        }

        void updateDirectManipulationButton() {
#if defined(WITH_DIRECT_MANIPULATION)
            m_directManipulationButton->setText(
                m_directManipulationButton->isChecked()
                    ? QStringLiteral("Direct Manipulation: on, Touchpad|Wheel (D)")
                    : QStringLiteral("Direct Manipulation: off (D)"));
#else
            m_directManipulationButton->setText(
                QStringLiteral("Direct Manipulation: unavailable in this build"));
#endif
        }

        ProbeCanvas *m_canvas = nullptr;
        QPushButton *m_directManipulationButton = nullptr;
        QPushButton *m_swallowButton = nullptr;
        QLabel *m_statusLabel = nullptr;
    };

}

int main(int argc, char *argv[]) {
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("TouchProbe"));

#if defined(WITH_DIRECT_MANIPULATION)
    // The system object must outlive every registered window.
    QWDMH::DirectManipulationSystem system;
#endif

    ProbeWindow window;
    window.show();
    return QApplication::exec();
}
