#include "EditorSystemGestureSuppressor.h"

#include "Model/AppOptions/AppOptions.h"
#include "Model/AppOptions/Options/AppearanceOption.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QList>
#include <QPointer>
#include <QWidget>
#include <QWindow>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {
    // Windows asks the window under the contact whether it wants the
    // press-and-hold gesture, once per contact. The message lives in tpcshrd.h
    // next to a pile of MIDL-generated tablet interfaces, which is not worth
    // pulling into a Qt translation unit, so the two values are spelled out.
    // Both are stable since Windows 7: WM_TABLET_QUERYSYSTEMGESTURESTATUS is
    // WM_TABLET_DEFBASE (0x02C0) + 12.
    constexpr UINT wmTabletQuerySystemGestureStatus = 0x02CC;
    constexpr LRESULT tabletDisablePressAndHold = 0x00000001;

    // One entry per touch-enabled editor widget, never more than a handful.
    // QPointer keeps a view that goes away from being answered for.
    QList<QPointer<QWidget>> &claimedWidgets() {
        static QList<QPointer<QWidget>> widgets;
        return widgets;
    }

#ifdef Q_OS_WIN
    // Is this native window one of the editor windows, or a native child of
    // one? GA_ROOT walks up to the top-level window, which is what the widgets
    // are registered as.
    bool coversWindow(const HWND window) {
        const HWND root = GetAncestor(window, GA_ROOT);
        if (!root)
            return false;
        for (const auto &widget : claimedWidgets()) {
            if (!widget)
                continue;
            const QWidget *topLevel = widget->window();
            const QWindow *handle = topLevel ? topLevel->windowHandle() : nullptr;
            if (handle && reinterpret_cast<HWND>(handle->winId()) == root)
                return true;
        }
        return false;
    }

    // Answers the query, and does nothing else: the pen hover shim in
    // EditorPenHoverWatcher reads the same message stream, and Qt's own pointer
    // handling has to keep seeing every message it is not the answer to.
    class PressAndHoldFilter final : public QAbstractNativeEventFilter {
    public:
        bool nativeEventFilter(const QByteArray &eventType, void *message,
                               qintptr *result) override {
            if (!message || !eventType.startsWith("windows_"))
                return false;

            const auto *msg = static_cast<MSG *>(message);
            if (msg->message != wmTabletQuerySystemGestureStatus)
                return false;

            // The escape hatch: with the gesture layer switched off nothing
            // takes the long press over, so the platform keeps it, square and
            // all. The option is read per query, which is what makes the switch
            // hot.
            if (!appOptions->appearance()->enableTouchGestures)
                return false;
            if (!coversWindow(msg->hwnd))
                return false;

            *result = tabletDisablePressAndHold;
            return true;
        }
    };
#endif // Q_OS_WIN

} // namespace

namespace EditorSystemGestureSuppressor {

    void addWindow(QWidget *editorWidget) {
        if (!editorWidget)
            return;

        auto &widgets = claimedWidgets();
        if (!widgets.contains(editorWidget))
            widgets.append(editorWidget);

        auto *app = QCoreApplication::instance();
        if (!app)
            return;
        static bool installed = false;
        if (installed)
            return;
        installed = true;
#ifdef Q_OS_WIN
        // Never removed: it has to outlive every widget, and it answers nothing
        // while the gesture layer is off or the window is not one of ours.
        app->installNativeEventFilter(new PressAndHoldFilter);
#else
        // Other platforms have no equivalent system gesture here, so the
        // touch layer's own long press is never contested.
        Q_UNUSED(app)
#endif
    }

} // namespace EditorSystemGestureSuppressor
