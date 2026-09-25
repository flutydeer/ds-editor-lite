#include "EditorPenHoverWatcher.h"

#include "EditorPenStroke.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QEvent>
#include <QTabletEvent>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {

    // Everything but Windows: the hover tablet event is expected to carry the
    // state, so no native code is needed here. That expectation comes from the
    // platform backends rather than from a measurement, and it has NOT been
    // verified on X11 or macOS hardware — see
    // docs/design/touch-and-pen-input-design.md §6.5 and the four questions to
    // check first in §7.7. The evidence:
    //   - X11 accumulates the barrel buttons from XI button events, unrelated
    //     to the contact state: qxcbconnection_xi2.cpp:1497-1507 ("the tip,
    //     plus two barrel buttons", `tabletData->buttons |= b`).
    //   - macOS tracks the same thing from the NSEvent button mask:
    //     qnsview_tablet.mm:77.
    // If a platform turns out not to report it, the degraded behaviour is "no
    // hover cursor" — the stroke semantics do not depend on this class.
    class TabletEventPenHoverWatcher final : public EditorPenHoverWatcher {
    public:
        explicit TabletEventPenHoverWatcher(QObject *parent) : EditorPenHoverWatcher(parent) {
        }

        void observeHoverEvent(const QTabletEvent *event) override {
            if (!event)
                return;
            State next;
            next.inRange = true;
            next.inverted = event->pointerType() == QPointingDevice::PointerType::Eraser;
            // The same "anything but the tip" rule the stroke tracker uses: the
            // barrel button is not a fixed Qt::MouseButton across platforms.
            next.sideButton = EditorPenStroke::hasSideButton(event->buttons());
            setState(next);
        }
    };

#ifdef Q_OS_WIN
    class WindowsPenHoverWatcher;

    // Reads WM_POINTER messages before Qt does and publishes the pen flags Qt
    // discards. Never consumes a message: returning false is deliberate, the
    // Qt pointer handler still needs every one of them.
    class PenRawMessageFilter final : public QAbstractNativeEventFilter {
    public:
        explicit PenRawMessageFilter(WindowsPenHoverWatcher *watcher) : m_watcher(watcher) {
        }

        bool nativeEventFilter(const QByteArray &eventType, void *message,
                               qintptr *result) override;

    private:
        WindowsPenHoverWatcher *m_watcher;
    };

    class WindowsPenHoverWatcher final : public EditorPenHoverWatcher {
    public:
        explicit WindowsPenHoverWatcher(QObject *parent) : EditorPenHoverWatcher(parent) {
            if (auto *app = QCoreApplication::instance())
                app->installNativeEventFilter(new PenRawMessageFilter(this));
        }

        // The raw messages already carry everything, and they carry it earlier.
        void observeHoverEvent(const QTabletEvent *event) override {
            Q_UNUSED(event)
        }

        void onPenFlags(const bool barrel, const bool inverted) {
            State next;
            next.inRange = true;
            next.inverted = inverted;
            next.sideButton = barrel;
            setState(next);
        }

        void onPenLeft() {
            clear();
        }
    };

    bool PenRawMessageFilter::nativeEventFilter(const QByteArray &eventType, void *message,
                                                qintptr *result) {
        Q_UNUSED(result)
        if (!message || !eventType.startsWith("windows_"))
            return false;

        const auto *msg = static_cast<MSG *>(message);
        switch (msg->message) {
            case WM_POINTERENTER:
            case WM_POINTERLEAVE:
            case WM_POINTERUPDATE:
            case WM_POINTERDOWN:
            case WM_POINTERUP:
                break;
            default:
                return false;
        }

        const auto pointerId = static_cast<UINT>(LOWORD(msg->wParam));
        POINTER_INPUT_TYPE pointerType = PT_POINTER;
        if (!GetPointerType(pointerId, &pointerType) || pointerType != PT_PEN)
            return false;

        if (msg->message == WM_POINTERLEAVE) {
            m_watcher->onPenLeft();
            return false;
        }

        POINTER_PEN_INFO penInfo{};
        if (!GetPointerPenInfo(pointerId, &penInfo)) {
            // In range, but the flags are unknown: keep the range and drop the
            // hint rather than leave a stale one on screen.
            m_watcher->onPenFlags(false, false);
            return false;
        }
        // PEN_FLAG_ERASER and PEN_FLAG_INVERTED are both set by an inverted
        // pen; either one means "this is the eraser end".
        m_watcher->onPenFlags((penInfo.penFlags & PEN_FLAG_BARREL) != 0,
                              (penInfo.penFlags & (PEN_FLAG_INVERTED | PEN_FLAG_ERASER)) != 0);
        return false;
    }
#endif // Q_OS_WIN

} // namespace

EditorPenHoverWatcher::EditorPenHoverWatcher(QObject *parent) : QObject(parent) {
    // Proximity events are delivered to the application object rather than to a
    // widget, so the watcher has to filter them itself; that is the one thing it
    // cannot leave to the per-widget controllers.
    if (auto *app = QCoreApplication::instance())
        app->installEventFilter(this);
}

EditorPenHoverWatcher::~EditorPenHoverWatcher() = default;

EditorPenHoverWatcher::State EditorPenHoverWatcher::state() const {
    return m_state;
}

bool EditorPenHoverWatcher::eraseHint() const {
    return m_state.inRange && (m_state.inverted || m_state.sideButton);
}

namespace {
    // Never destroyed: it has to outlive every widget and holds nothing but
    // read-only state.
    EditorPenHoverWatcher *g_hoverWatcher = nullptr;
}

EditorPenHoverWatcher *EditorPenHoverWatcher::instance() {
    if (g_hoverWatcher)
        return g_hoverWatcher;
    auto *app = QCoreApplication::instance();
#ifdef Q_OS_WIN
    g_hoverWatcher = new WindowsPenHoverWatcher(app);
#else
    g_hoverWatcher = new TabletEventPenHoverWatcher(app);
#endif
    return g_hoverWatcher;
}

EditorPenHoverWatcher *EditorPenHoverWatcher::existing() {
    return g_hoverWatcher;
}

void EditorPenHoverWatcher::observeHoverEvent(const QTabletEvent *event) {
    Q_UNUSED(event)
}

bool EditorPenHoverWatcher::eventFilter(QObject *watched, QEvent *event) {
    Q_UNUSED(watched)
    if (event->type() == QEvent::TabletLeaveProximity) {
        clear();
        return false;
    }
    return false;
}

void EditorPenHoverWatcher::setState(const State &state) {
    if (m_state.inRange == state.inRange && m_state.inverted == state.inverted &&
        m_state.sideButton == state.sideButton) {
        return;
    }
    m_state = state;
    emit changed();
}

void EditorPenHoverWatcher::clear() {
    setState({});
}
