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
// Keys: C clear, D toggle Direct Manipulation, S toggle swallowing, A toggle
// accepting tablet events, G toggle SetGestureConfig, Q toggle the
// press-and-hold query answer, F fullscreen, Esc quit.
//
// The accept-tablet switch exists for the pen work: leaving tablet events
// unaccepted is how the editor gets the stylus as ordinary mouse input, and
// accepting them is how a future pen layer takes that mouse stream over. The
// two modes side by side on real hardware are what tell us which one the
// platform actually honours.
//
// A fourth question needs the raw Windows message rather than a Qt event:
// whether the pen side button can be seen while the pen hovers. Qt reads
// PEN_FLAG_BARREL only when the pen is in contact, so the hovering state never
// reaches QTabletEvent, QMouseEvent or QInputDevice. PenRawStateFilter reads
// the same WM_POINTER message the platform gives Qt and reports the state it
// finds there, which is what decides whether a hover gesture such as OneNote's
// barrel-button lasso is reachable from this application at all.
//
// A fifth question belongs to the touch long press: Windows promotes a held
// finger into its own right click, drawing a translucent square while the
// finger is down and opening the menu on release, and nothing in Qt can see or
// veto that. Two documented ways to switch it off exist, and they sit at
// different layers, so the probe exposes them as two independent switches:
//
//   G  SetGestureConfig(hwnd, 0, 1, {0, 0, GC_ALLGESTURES}, sizeof)  legacy gestures
//   Q  answer WM_TABLET_QUERYSYSTEMGESTURESTATUS with TABLET_DISABLE_PRESSANDHOLD
//
// Every raw message line is stamped with the arm it was recorded under, so a
// session with the switches flipped halfway through is still readable. A hold
// that keeps producing WM_RBUTTONDOWN/UP and WM_CONTEXTMENU means the mechanism
// is not consulted at all, and a hold that produces none means the editor is
// free to own the long press again.

#include <QAbstractNativeEventFilter>
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
#include <QPointingDevice>
#include <QPushButton>
#include <QSet>
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

#if defined(Q_OS_WIN)
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
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
            {60,  220, 120},
            {0,   205, 190},
            {150, 230, 60 },
            {0,   225, 255},
            {120, 235, 165},
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

    // Which end of the pen is in use. This is the only field that tells a
    // flipped pen (Eraser) from a normal one, and on Windows it is what
    // distinguishes the two ends of a double-ended stylus.
    QString pointerTypeName(const QPointingDevice::PointerType type) {
        switch (type) {
            case QPointingDevice::PointerType::Unknown:
                return QStringLiteral("Unknown");
            case QPointingDevice::PointerType::Generic:
                return QStringLiteral("Generic");
            case QPointingDevice::PointerType::Finger:
                return QStringLiteral("Finger");
            case QPointingDevice::PointerType::Pen:
                return QStringLiteral("Pen");
            case QPointingDevice::PointerType::Eraser:
                return QStringLiteral("Eraser");
            case QPointingDevice::PointerType::Cursor:
                return QStringLiteral("Cursor");
            default:
                break;
        }
        return QStringLiteral("?");
    }

    // The raw integer (1 = left, 2 = right) had to be decoded by hand every
    // time. The pen work turns on exactly which button a stroke carries, so
    // print the names.
    QString buttonNames(const Qt::MouseButtons buttons) {
        if (buttons == Qt::NoButton)
            return QStringLiteral("None");
        QStringList parts;
        if (buttons & Qt::LeftButton)
            parts.append(QStringLiteral("Left"));
        if (buttons & Qt::RightButton)
            parts.append(QStringLiteral("Right"));
        if (buttons & Qt::MiddleButton)
            parts.append(QStringLiteral("Middle"));
        if (buttons & Qt::BackButton)
            parts.append(QStringLiteral("Back"));
        if (buttons & Qt::ForwardButton)
            parts.append(QStringLiteral("Forward"));
        return parts.isEmpty() ? QStringLiteral("0x%1").arg(static_cast<int>(buttons), 0, 16)
                               : parts.join(u'|');
    }

    // "wmpointer" is Qt's WM_POINTER tablet device on Windows. Any other name
    // means the legacy WinTab path, which promotes its own mouse events and
    // therefore needs a different handling strategy.
    QString deviceLabel(const QInputDevice *device) {
        if (!device)
            return QStringLiteral("null");
        const auto *pointing = dynamic_cast<const QPointingDevice *>(device);
        if (!pointing)
            return QStringLiteral("%1(%2)").arg(device->name(), deviceTypeName(device));
        return QStringLiteral("%1(%2, %3)")
            .arg(device->name(), deviceTypeName(device), pointerTypeName(pointing->pointerType()));
    }

    QString deviceDetail(const QInputDevice *device) {
        if (!device)
            return QStringLiteral("null");
        QStringList capabilities;

        const struct {
            QInputDevice::Capability flag;
            const char *name;
        } entries[] = {
            {QInputDevice::Capability::Position,           "Position"          },
            {QInputDevice::Capability::Pressure,           "Pressure"          },
            {QInputDevice::Capability::Hover,              "Hover"             },
            {QInputDevice::Capability::Rotation,           "Rotation"          },
            {QInputDevice::Capability::XTilt,              "XTilt"             },
            {QInputDevice::Capability::YTilt,              "YTilt"             },
            {QInputDevice::Capability::TangentialPressure, "TangentialPressure"},
        };

        for (const auto &entry : entries) {
            if (device->capabilities() & entry.flag)
                capabilities.append(QLatin1String(entry.name));
        }
        const auto *pointing = dynamic_cast<const QPointingDevice *>(device);
        return QStringLiteral("%1 caps=%2 maxPoints=%3 systemId=%4")
            .arg(deviceLabel(device),
                 capabilities.isEmpty() ? QStringLiteral("none") : capabilities.join(u'|'))
            .arg(pointing ? pointing->maximumPoints() : -1)
            .arg(device->systemId());
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
            logInputDevices();
            // Both mechanisms start untouched, which is the state every other
            // Windows application runs in, so the first arm in the log is the
            // baseline the other arms are compared against.
            logArm();
        }

        // The device list says up front which pen implementation the platform
        // handed us, and whether it claims pressure or tilt at all.
        void logInputDevices() {
            const auto devices = QInputDevice::devices();
            log(QStringLiteral("--- %1 input device(s) at startup").arg(devices.size()));
            for (const auto *device : devices)
                noteDevice(device);
        }

        // Pointing devices are registered lazily: on Windows the pen device
        // does not exist until the first pen event arrives, so the startup dump
        // cannot show it. Log every device the first time an event carries it,
        // which is also how a second device for the eraser end would show up.
        void noteDevice(const QInputDevice *device) {
            if (!device || m_seenDevices.contains(device))
                return;
            m_seenDevices.insert(device);
            log(QStringLiteral("device   %1").arg(deviceDetail(device)));
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

        // Off (the default) mirrors the editor today: the tablet event is
        // ignored, so Qt synthesizes mouse events from it. On stops that
        // synthesis, which is how the pen work takes the mouse stream over.
        // Flipping this switch on real hardware shows whether accepting the
        // tablet event really does silence the mouse stream.
        void setAcceptTabletEvents(const bool on) {
            m_acceptTabletEvents = on;
            log(QStringLiteral("--- accept tablet events: %1 (mouse synthesis should %2)")
                    .arg(on ? QStringLiteral("on") : QStringLiteral("off"),
                         on ? QStringLiteral("stop") : QStringLiteral("resume")));
            update();
        }

        void logExternal(const QString &line) {
            log(line);
            update();
        }

        // --- system press-and-hold switches ---------------------------------
        // Kept here rather than in the window so that every raw message line can
        // be stamped with the arm it belongs to, and the HUD can show the state
        // without reaching into another object.
        void setGestureConfigBlocked(const bool on) {
            m_gestureConfigBlocked = on;
            logArm();
        }

        void setPressAndHoldBlocked(const bool on) {
            m_pressAndHoldBlocked = on;
            logArm();
        }

        [[nodiscard]] bool pressAndHoldBlocked() const {
            return m_pressAndHoldBlocked;
        }

        // Prefixed to every raw message, so the log says which mechanisms were
        // disabled at the moment the message arrived.
        [[nodiscard]] QString armState() const {
            return QStringLiteral("G=%1 Q=%2")
                .arg(m_gestureConfigBlocked ? QStringLiteral("on") : QStringLiteral("off"),
                     m_pressAndHoldBlocked ? QStringLiteral("on") : QStringLiteral("off"));
        }

        void logRaw(const QString &line) {
            log(QStringLiteral("[%1] %2").arg(armState(), line));
            update();
        }

        void logArm() {
            log(QStringLiteral("=== arm: SetGestureConfig=%1  press-and-hold query=%2")
                    .arg(m_gestureConfigBlocked ? QStringLiteral("GC_ALLGESTURES blocked")
                                                : QStringLiteral("untouched"),
                         m_pressAndHoldBlocked ? QStringLiteral("TABLET_DISABLE_PRESSANDHOLD")
                                               : QStringLiteral("untouched")));
            update();
        }

        // Kept out of the scrolling log so the raw side-button state stays
        // readable while the pen hovers and floods the log with moves.
        void setPenRawStatus(const QString &text) {
            if (m_penRawStatus == text)
                return;
            m_penRawStatus = text;
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
                    // whatever the long press already did. The time since the
                    // last press is what attributes the menu to a hold: a short
                    // gap means a plain right click, a long one means the
                    // platform's own press-and-hold promotion.
                    const auto *menuEvent = static_cast<QContextMenuEvent *>(event);
                    log(QStringLiteral("menu     reason=%1 pos=(%2,%3) sincePress=%4ms")
                            .arg(contextMenuReasonName(menuEvent->reason()))
                            .arg(menuEvent->pos().x())
                            .arg(menuEvent->pos().y())
                            .arg(m_lastPointerPressMs < 0
                                     ? -1
                                     : m_clock.elapsed() - m_lastPointerPressMs));
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
            noteDevice(event->pointingDevice());
            appendPoint(kind, -2, event->position());
            if (event->type() == QEvent::TabletPress)
                m_lastPointerPressMs = m_clock.elapsed();
            log(QStringLiteral("tablet   %1 ptr=%2 dev=%3 button=%4 buttons=%5 pressure=%6 "
                               "tilt=(%7,%8) pos=(%9,%10)")
                    .arg(tabletActionName(event->type()), pointerTypeName(event->pointerType()),
                         deviceLabel(event->pointingDevice()), buttonNames(event->button()),
                         buttonNames(event->buttons()))
                    .arg(event->pressure(), 0, 'f', 2)
                    .arg(event->xTilt())
                    .arg(event->yTilt())
                    .arg(event->position().x(), 0, 'f', 1)
                    .arg(event->position().y(), 0, 'f', 1));
            if (event->type() == QEvent::TabletRelease ||
                event->type() == QEvent::TabletLeaveProximity)
                endStroke(kind, -2);
            if (m_acceptTabletEvents) {
                // Accepting is what lets the editor own the whole stroke: Qt
                // then sends no mouse events at all for it, and whatever we
                // synthesize is the only input the interaction layer sees.
                event->accept();
            } else {
                // Left unaccepted on purpose, which is how the editor gets the
                // stylus as ordinary mouse input today.
                event->ignore();
            }
            update();
        }

        void wheelEvent(QWheelEvent *event) override {
            noteDevice(event->device());
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
            noteDevice(event->pointingDevice());
            const auto synthesized = event->source() != Qt::MouseEventNotSynthesized;
            const auto swallowed = synthesized && m_swallowSynthesizedMouse;
            if (!swallowed) {
                const auto kind = synthesized ? DeviceKind::SynthesizedMouse : DeviceKind::Mouse;
                appendPoint(kind, synthesized ? -4 : -1, event->position());
            }
            if (action == QLatin1String("press"))
                m_lastPointerPressMs = m_clock.elapsed();
            // The device label matters as much as the button here: a mouse
            // event whose device is a Stylus is a pen stroke wearing mouse
            // clothes, and that is exactly what has to be recognized.
            log(QStringLiteral("mouse    %1 dev=%2 source=%3 pos=(%4,%5) button=%6 buttons=%7%8")
                    .arg(action, deviceLabel(event->pointingDevice()),
                         mouseSourceName(event->source()))
                    .arg(event->position().x(), 0, 'f', 1)
                    .arg(event->position().y(), 0, 'f', 1)
                    .arg(buttonNames(event->button()), buttonNames(event->buttons()))
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
            noteDevice(event->pointingDevice());
            m_activePoints = 0;
            if (event->type() == QEvent::TouchBegin) {
                m_lastPointerPressMs = m_clock.elapsed();
                m_touchDownMs = m_lastPointerPressMs;
            }
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
            if (event->type() == QEvent::TouchEnd && m_touchDownMs >= 0) {
                // The bracket around each human hold. Windows promotes a hold to
                // its own right click at roughly one second, so the duration
                // says whether the contact above was long enough for that.
                log(QStringLiteral("hold     contact lasted %1 ms")
                        .arg(m_clock.elapsed() - m_touchDownMs));
                m_touchDownMs = -1;
            }
            update();
        }

        void recordGesture(const QNativeGestureEvent *event) {
            noteDevice(event->device());
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
                               24.0 + maximumHudLines * 14.0 +
                                   (m_penRawStatus.isEmpty() ? 0.0 : 16.0));
            painter.fillRect(panel, QColor(0, 0, 0, 170));
            painter.setPen(QColor(220, 220, 220));

            const auto header =
                QStringLiteral("touch points: %1    dpr: %2    accept tablet: %3    "
                               "swallow synth mouse: %4    gestures: %5    hold query: %6    "
                               "C clear / D DM / S swallow / A accept tablet / G gestures / "
                               "Q hold query")
                    .arg(m_activePoints)
                    .arg(devicePixelRatioF(), 0, 'f', 2)
                    .arg(m_acceptTabletEvents ? QStringLiteral("on") : QStringLiteral("off"),
                         m_swallowSynthesizedMouse ? QStringLiteral("on") : QStringLiteral("off"),
                         m_gestureConfigBlocked ? QStringLiteral("GC_ALLGESTURES blocked")
                                                : QStringLiteral("system default"),
                         m_pressAndHoldBlocked ? QStringLiteral("disabled")
                                               : QStringLiteral("DefWindowProc"));
            painter.drawText(QPointF(panel.left() + 8, panel.top() + 16), header);

            double y = panel.top() + 34;
            if (!m_penRawStatus.isEmpty()) {
                painter.setPen(QColor(255, 180, 120));
                painter.drawText(QPointF(panel.left() + 8, y), m_penRawStatus);
                painter.setPen(QColor(220, 220, 220));
                y += 16.0;
            }

            for (const auto &line : m_hud) {
                painter.drawText(QPointF(panel.left() + 8, y), line);
                y += 14.0;
            }
        }

        QHash<QPair<int, int>, Trail> m_trails;
        // Devices already described in the log, so each is printed once even
        // though it is registered only when its first event arrives.
        QSet<const QInputDevice *> m_seenDevices;
        QStringList m_hud;
        QString m_penRawStatus;
        int m_activePoints = 0;
        bool m_swallowSynthesizedMouse = false;
        // Whether tablet events are accepted, which is what decides if Qt
        // synthesizes mouse events for the pen.
        bool m_acceptTabletEvents = false;
        // Whether the two system press-and-hold mechanisms are disabled. Both
        // start off, which is the state every other Windows application is in.
        bool m_gestureConfigBlocked = false;
        bool m_pressAndHoldBlocked = false;
        // When the current touch contact began, so the log can bracket a human
        // hold with its measured duration.
        qint64 m_touchDownMs = -1;
        // When the last pointer went down, so a context menu can be attributed
        // to a long hold rather than a plain right click.
        qint64 m_lastPointerPressMs = -1;
        QElapsedTimer m_clock;
        QFile m_logFile;
    };

#if defined(Q_OS_WIN)
    QString pointerMessageName(const DWORD message) {
        switch (message) {
            case WM_POINTERUPDATE:
                return QStringLiteral("WM_POINTERUPDATE");
            case WM_POINTERDOWN:
                return QStringLiteral("WM_POINTERDOWN");
            case WM_POINTERUP:
                return QStringLiteral("WM_POINTERUP");
            default:
                return QStringLiteral("0x%1").arg(message, 0, 16);
        }
    }

    // Qt reads the pen side button from PEN_FLAG_BARREL, but only while the pen
    // touches the screen:
    //
    //     if (pointerInContact && penInfo->penFlags & PEN_FLAG_BARREL)
    //         mouseButtons = Qt::RightButton;
    //
    // (qwindowspointerhandler.cpp, translatePenEvent). During hover that flag
    // is dropped before any Qt event exists, so no QTabletEvent, QMouseEvent or
    // QInputDevice can report the button. This filter reads the very message
    // Qt derives its events from and prints the state it finds there, which is
    // what decides whether a hover gesture like OneNote's barrel-button lasso
    // is reachable from an application built on Qt.
    class PenRawStateFilter final : public QAbstractNativeEventFilter {
    public:
        explicit PenRawStateFilter(ProbeCanvas *canvas) : m_canvas(canvas) {
        }

        bool nativeEventFilter(const QByteArray &eventType, void *message,
                               qintptr *result) override {
            Q_UNUSED(result)
            if (eventType != QByteArrayLiteral("windows_generic_MSG"))
                return false;

            const auto *msg = static_cast<const MSG *>(message);
            if (msg->message != WM_POINTERUPDATE && msg->message != WM_POINTERDOWN &&
                msg->message != WM_POINTERUP)
                return false;

            const quint32 pointerId = GET_POINTERID_WPARAM(msg->wParam);
            POINTER_INPUT_TYPE pointerType = PT_POINTER;
            if (!GetPointerType(pointerId, &pointerType) || pointerType != PT_PEN)
                return false;

            POINTER_PEN_INFO penInfo = {};
            if (!GetPointerPenInfo(pointerId, &penInfo))
                return false;

            const bool barrel = (penInfo.penFlags & PEN_FLAG_BARREL) != 0;
            const bool inverted = (penInfo.penFlags & (PEN_FLAG_INVERTED | PEN_FLAG_ERASER)) != 0;
            const bool inContact = (penInfo.pointerInfo.pointerFlags & POINTER_FLAG_INCONTACT) != 0;

            // Only changes are reported, because the hover stream is dense and
            // one line per message would drown the log. The first observation
            // is always reported, so a plain hover proves the filter is alive.
            const bool changed =
                barrel != m_barrel || inverted != m_inverted || inContact != m_inContact;
            if (!changed && m_reported)
                return false;
            m_reported = true;
            m_barrel = barrel;
            m_inverted = inverted;
            m_inContact = inContact;

            const auto line =
                QStringLiteral("penraw   %1 barrel=%2 inverted=%3 inContact=%4 pos=(%5,%6)")
                    .arg(pointerMessageName(msg->message))
                    .arg(barrel ? QStringLiteral("on") : QStringLiteral("off"))
                    .arg(inverted ? QStringLiteral("on") : QStringLiteral("off"))
                    .arg(inContact ? QStringLiteral("on") : QStringLiteral("off"))
                    .arg(penInfo.pointerInfo.ptPixelLocation.x)
                    .arg(penInfo.pointerInfo.ptPixelLocation.y);
            m_canvas->logExternal(line);
            m_canvas->setPenRawStatus(line);
            // Never consumed: Qt and DefWindowProc both still need the message.
            return false;
        }

    private:
        ProbeCanvas *m_canvas = nullptr;
        bool m_barrel = false;
        bool m_inverted = false;
        bool m_inContact = false;
        // Whether any pen message was seen at all, so the first one is always
        // reported even though it matches the initial state.
        bool m_reported = false;
    };
#endif

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

            m_acceptTabletButton = new QPushButton(this);
            m_acceptTabletButton->setCheckable(true);
            connect(m_acceptTabletButton, &QPushButton::toggled, this, [this](const bool on) {
                m_canvas->setAcceptTabletEvents(on);
                updateAcceptTabletButton();
            });
            updateAcceptTabletButton();

            m_gestureConfigButton = new QPushButton(this);
            m_gestureConfigButton->setCheckable(true);
            connect(m_gestureConfigButton, &QPushButton::toggled, this, [this](const bool on) {
                // The API call is the point of this switch, so it happens here
                // and its result is logged: a refusal is as informative as a
                // success, and the GetGestureConfig that follows says whether the
                // window really took the configuration.
                applyGestureConfig(on);
                m_canvas->setGestureConfigBlocked(on);
                updateGestureConfigButton();
            });
            updateGestureConfigButton();

            m_pressAndHoldButton = new QPushButton(this);
            m_pressAndHoldButton->setCheckable(true);
            connect(m_pressAndHoldButton, &QPushButton::toggled, this, [this](const bool on) {
                // Nothing to call: the answer travels back when the platform asks
                // WM_TABLET_QUERYSYSTEMGESTURESTATUS, so only the switch flips.
                // That is also what makes this arm testable at all, since the
                // query arrives at an arbitrary later moment.
                m_canvas->setPressAndHoldBlocked(on);
                updatePressAndHoldButton();
            });
            updatePressAndHoldButton();

            m_statusLabel = new QLabel(this);
            m_statusLabel->setText(m_canvas->logPath());
            m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

            auto *controls = new QHBoxLayout;
            controls->addWidget(clearButton);
            controls->addWidget(m_directManipulationButton);
            controls->addWidget(m_swallowButton);
            controls->addWidget(m_acceptTabletButton);
            controls->addWidget(m_gestureConfigButton);
            controls->addWidget(m_pressAndHoldButton);
            controls->addWidget(m_statusLabel, 1);

            auto *layout = new QVBoxLayout(this);
            layout->addLayout(controls);
            layout->addWidget(m_canvas, 1);
        }

        [[nodiscard]] ProbeCanvas *canvas() const {
            return m_canvas;
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
                case Qt::Key_A:
                    m_acceptTabletButton->toggle();
                    return;
                case Qt::Key_G:
                    m_gestureConfigButton->toggle();
                    return;
                case Qt::Key_Q:
                    m_pressAndHoldButton->toggle();
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

        void updateAcceptTabletButton() {
            m_acceptTabletButton->setText(m_acceptTabletButton->isChecked()
                                              ? QStringLiteral("Accept tablet events: on (A)")
                                              : QStringLiteral("Accept tablet events: off (A)"));
        }

        void updateGestureConfigButton() {
            m_gestureConfigButton->setText(
                m_gestureConfigButton->isChecked()
                    ? QStringLiteral("Gestures: GC_ALLGESTURES blocked (G)")
                    : QStringLiteral("Gestures: system default (G)"));
        }

        void updatePressAndHoldButton() {
            m_pressAndHoldButton->setText(
                m_pressAndHoldButton->isChecked()
                    ? QStringLiteral("Hold query: TABLET_DISABLE_PRESSANDHOLD (Q)")
                    : QStringLiteral("Hold query: DefWindowProc answers (Q)"));
        }

#if defined(Q_OS_WIN)
        // Raymond Chen's answer to "how do I disable the press-and-hold gesture
        // for my window": block every gesture on the window's HWND. The window
        // has to be registered for touch for the call to be meaningful (Qt
        // registers any widget that accepts touch events), and the call has to
        // be repeated whenever the native window is recreated, which is why both
        // facts are logged rather than assumed.
        void applyGestureConfig(const bool block) {
            const HWND hwnd = reinterpret_cast<HWND>(winId());
            ULONG touchFlags = 0;
            const bool touchWindow = IsTouchWindow(hwnd, &touchFlags) != FALSE;

            GESTURECONFIG config = {};
            config.dwID = 0; // 0 stands for the whole gesture set
            config.dwWant = 0;
            config.dwBlock = block ? GC_ALLGESTURES : 0;
            SetLastError(0);
            const BOOL ok = SetGestureConfig(hwnd, 0, 1, &config, sizeof(config));
            const DWORD error = ok ? 0 : GetLastError();
            m_canvas->logRaw(
                QStringLiteral("api      SetGestureConfig(hwnd=0x%1, dwBlock=%2) touchWindow=%3 "
                               "touchFlags=0x%4 -> %5%6")
                    .arg(reinterpret_cast<quintptr>(hwnd), 0, 16)
                    .arg(block ? QStringLiteral("GC_ALLGESTURES") : QStringLiteral("0"))
                    .arg(touchWindow ? 1 : 0)
                    .arg(touchFlags, 0, 16)
                    .arg(ok ? QStringLiteral("TRUE") : QStringLiteral("FALSE"))
                    .arg(ok ? QString() : QStringLiteral(" err=%1").arg(error)));
            logGestureConfig(hwnd);
            m_canvas->setFocus();
        }

        // Reading the configuration back separates "the call was accepted" from
        // "the call did nothing", which is the difference between a mechanism
        // that is wrong and one that is merely ignored.
        void logGestureConfig(const HWND hwnd) const {
            UINT count = 1;
            GESTURECONFIG config = {};
            config.dwID = 0;
            if (GetGestureConfig(hwnd, 0, 0, &count, &config, sizeof(config))) {
                m_canvas->logRaw(
                    QStringLiteral("api      GetGestureConfig: dwID=0 dwWant=0x%1 dwBlock=0x%2")
                        .arg(config.dwWant, 0, 16)
                        .arg(config.dwBlock, 0, 16));
                return;
            }
            m_canvas->logRaw(
                QStringLiteral("api      GetGestureConfig failed err=%1").arg(GetLastError()));
        }
#endif

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
        QPushButton *m_acceptTabletButton = nullptr;
        QPushButton *m_gestureConfigButton = nullptr;
        QPushButton *m_pressAndHoldButton = nullptr;
        QLabel *m_statusLabel = nullptr;
    };

#if defined(Q_OS_WIN)
    // tpcshrd.h carries these next to a pile of MIDL-generated tablet
    // interfaces, which is not worth pulling into a Qt translation unit, so the
    // two values are spelled out here. Both are stable since Windows 7:
    // WM_TABLET_QUERYSYSTEMGESTURESTATUS is WM_TABLET_DEFBASE (0x02C0) + 12.
    constexpr UINT wmTabletQuerySystemGestureStatus = 0x02CC;
    constexpr LRESULT tabletDisablePressAndHold = 0x00000001;

    // The raw-message view of Windows' own press-and-hold, which is the one part
    // of the long press no Qt event can see. Three things are logged:
    //
    //   - every pointer message that decides what Qt's sticky m_pointerType
    //     holds, because that is what decides whether a promoted mouse event
    //     enters the application as BySystem (which the editor swallows) or as
    //     NotSynthesized (which it passes straight through),
    //   - whether the platform asks the window about system gestures at all,
    //   - whether a hold still produces the legacy right button and the menu.
    //
    // The filter sits at the application level, which is where Qt runs native
    // filters for messages it does not treat as input: qwindowscontext.cpp skips
    // that pass only for the messages its own event dispatcher delivered, and
    // WM_TABLET_QUERYSYSTEMGESTURESTATUS is not one of them. That makes this the
    // layer that can answer the query, and the layer the editor's own probes
    // already use.
    class SystemGestureFilter final : public QAbstractNativeEventFilter {
    public:
        explicit SystemGestureFilter(ProbeCanvas *canvas) : m_canvas(canvas) {
        }

        bool nativeEventFilter(const QByteArray &eventType, void *message,
                               qintptr *result) override {
            if (eventType != QByteArrayLiteral("windows_generic_MSG"))
                return false;
            const auto *msg = static_cast<const MSG *>(message);
            switch (msg->message) {
                case WM_POINTERDOWN:
                case WM_POINTERUP:
                    observePointer(msg);
                    return false;
                case WM_GESTURE:
                    // Qt recognizes this type and then does nothing with it
                    // (qwindowscontext.cpp, "case QtWindows::GestureEvent:
                    // break"), so any feedback the legacy stack draws for a hold
                    // is DefWindowProc's work.
                    m_canvas->logRaw(QStringLiteral("raw      WM_GESTURE id=%1")
                                         .arg(static_cast<int>(msg->wParam)));
                    return false;
                case wmTabletQuerySystemGestureStatus:
                    return answerSystemGestureQuery(result);
                case WM_LBUTTONDOWN:
                case WM_LBUTTONUP:
                case WM_LBUTTONDBLCLK:
                case WM_RBUTTONDOWN:
                case WM_RBUTTONUP:
                case WM_CONTEXTMENU:
                    logLegacyMouse(msg->message);
                    return false;
                default:
                    return false;
            }
        }

    private:
        void observePointer(const MSG *msg) {
            const quint32 pointerId = GET_POINTERID_WPARAM(msg->wParam);
            POINTER_INPUT_TYPE type = PT_POINTER;
            if (!GetPointerType(pointerId, &type))
                return;
            // Qt caches exactly this value in m_pointerType and never clears it,
            // so the last pointer message before a promoted mouse message is what
            // Qt compares against.
            m_pointerType = type;
            m_canvas->logRaw(QStringLiteral("raw      %1 ptr=%2 (%3)")
                                 .arg(QLatin1String(msg->message == WM_POINTERDOWN
                                                        ? "WM_POINTERDOWN"
                                                        : "WM_POINTERUP"),
                                      pointerTypeLabel(type))
                                 .arg(static_cast<int>(type)));
        }

        bool answerSystemGestureQuery(qintptr *result) {
            const bool block = m_canvas->pressAndHoldBlocked();
            m_canvas->logRaw(
                QStringLiteral("raw      WM_TABLET_QUERYSYSTEMGESTURESTATUS -> %1")
                    .arg(block ? QStringLiteral("TABLET_DISABLE_PRESSANDHOLD")
                               : QStringLiteral("0, left to DefWindowProc")));
            if (!block)
                return false;
            *result = tabletDisablePressAndHold;
            return true;
        }

        void logLegacyMouse(const UINT message) const {
            const ULONG_PTR extra = GetMessageExtraInfo();
            m_canvas->logRaw(
                QStringLiteral("raw      %1 extra=0x%2 from=%3 lastPointer=%4")
                    .arg(legacyMouseName(message))
                    .arg(static_cast<quintptr>(extra), 0, 16)
                    .arg(signatureSource(extra), pointerTypeLabel(m_pointerType)));
        }

        static QString legacyMouseName(const UINT message) {
            switch (message) {
                case WM_LBUTTONDOWN:
                    return QStringLiteral("WM_LBUTTONDOWN");
                case WM_LBUTTONUP:
                    return QStringLiteral("WM_LBUTTONUP");
                case WM_LBUTTONDBLCLK:
                    return QStringLiteral("WM_LBUTTONDBLCLK");
                case WM_RBUTTONDOWN:
                    return QStringLiteral("WM_RBUTTONDOWN");
                case WM_RBUTTONUP:
                    return QStringLiteral("WM_RBUTTONUP");
                case WM_CONTEXTMENU:
                    return QStringLiteral("WM_CONTEXTMENU");
                default:
                    return QStringLiteral("WM_0x%1").arg(message, 0, 16);
            }
        }

        // Windows stamps every mouse message it promoted from a contact with a
        // signature in the message extra info: the low byte is the contact index
        // and the rest identifies touch or pen. Qt reads the same signature, but
        // then decides the event source from the sticky pointer type instead
        // (qwindowspointerhandler.cpp:819-835), which is why both are logged.
        static QString signatureSource(const ULONG_PTR extra) {
            switch (static_cast<quint32>(extra & 0xFFFFFF00u)) {
                case 0xFF515700u:
                    return QStringLiteral("touch");
                case 0xFF515800u:
                    return QStringLiteral("pen");
                default:
                    return QStringLiteral("none");
            }
        }

        static QString pointerTypeLabel(const POINTER_INPUT_TYPE type) {
            switch (type) {
                case PT_POINTER:
                    return QStringLiteral("PT_POINTER");
                case PT_TOUCH:
                    return QStringLiteral("PT_TOUCH");
                case PT_PEN:
                    return QStringLiteral("PT_PEN");
                case PT_MOUSE:
                    return QStringLiteral("PT_MOUSE");
                case PT_TOUCHPAD:
                    return QStringLiteral("PT_TOUCHPAD");
                default:
                    return QStringLiteral("type %1").arg(static_cast<int>(type));
            }
        }

        ProbeCanvas *m_canvas = nullptr;
        POINTER_INPUT_TYPE m_pointerType = PT_POINTER;
    };
#endif

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

#if defined(Q_OS_WIN)
    PenRawStateFilter penRawState(window.canvas());
    application.installNativeEventFilter(&penRawState);
    SystemGestureFilter systemGesture(window.canvas());
    application.installNativeEventFilter(&systemGesture);
#endif

    return QApplication::exec();
}
