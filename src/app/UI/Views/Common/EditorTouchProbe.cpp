#include "EditorTouchProbe.h"

#include "EditorTouchController.h"

#include <QAbstractNativeEventFilter>
#include <QCoreApplication>
#include <QDebug>
#include <QEvent>
#include <QMetaObject>
#include <QStringList>
#include <QTouchEvent>
#include <QWindow>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace {

    QString describe(const QObject *object) {
        if (!object)
            return QStringLiteral("(none)");
        const auto *name = object->metaObject()->className();
        if (object->objectName().isEmpty())
            return QLatin1String(name);
        return QStringLiteral("%1(%2)").arg(QLatin1String(name), object->objectName());
    }

    const char *stateName(const QEventPoint::State state) {
        switch (state) {
            case QEventPoint::State::Pressed:
                return "down";
            case QEventPoint::State::Updated:
                return "move";
            case QEventPoint::State::Stationary:
                return "hold";
            case QEventPoint::State::Released:
                return "up";
            default:
                return "?";
        }
    }

    const char *eventName(const QEvent::Type type) {
        switch (type) {
            case QEvent::TouchBegin:
                return "begin";
            case QEvent::TouchUpdate:
                return "update";
            case QEvent::TouchEnd:
                return "end";
            case QEvent::TouchCancel:
                return "CANCEL";
            default:
                return "?";
        }
    }

    // Sees every touch event on its way to any object, so a TouchCancel that no
    // widget is a target of still shows up here.
    class ApplicationFilter final : public QObject {
    public:
        explicit ApplicationFilter(QObject *parent) : QObject(parent) {
        }

        bool eventFilter(QObject *watched, QEvent *event) override {
            switch (event->type()) {
                case QEvent::TouchBegin:
                case QEvent::TouchUpdate:
                case QEvent::TouchEnd:
                case QEvent::TouchCancel:
                    break;
                default:
                    return false;
            }
            if (!EditorTouchController::isProbeEnabled())
                return false;

            const auto *touch = static_cast<QTouchEvent *>(event);
            QStringList points;
            for (const auto &point : touch->points()) {
                points.append(QStringLiteral("%1:%2")
                                  .arg(point.id())
                                  .arg(QLatin1String(stateName(point.state()))));
            }
            qDebug().noquote() << QStringLiteral("qt touch %1 -> %2 [%3]")
                                      .arg(QLatin1String(eventName(event->type())),
                                           describe(watched), points.join(u' '));
            return false;
        }
    };

#ifdef Q_OS_WIN
    // Windows tags every message it synthesized from a touch contact with this
    // signature in the message extra info (the low byte is the contact index).
    constexpr ULONG_PTR touchSignature = 0xFF515700;
    constexpr ULONG_PTR touchSignatureMask = 0xFFFFFF00;

#  ifndef WM_POINTERCAPTURECHANGED
    constexpr UINT wmPointerCaptureChanged = 0x024C;
#  else
    constexpr UINT wmPointerCaptureChanged = WM_POINTERCAPTURECHANGED;
#  endif

    bool fromTouch() {
        return (static_cast<ULONG_PTR>(GetMessageExtraInfo()) & touchSignatureMask) ==
               touchSignature;
    }

    const char *mouseMessageName(const UINT message) {
        switch (message) {
            case WM_LBUTTONDOWN:
                return "LBUTTONDOWN";
            case WM_LBUTTONUP:
                return "LBUTTONUP";
            case WM_LBUTTONDBLCLK:
                return "LBUTTONDBLCLK";
            case WM_RBUTTONDOWN:
                return "RBUTTONDOWN";
            case WM_RBUTTONUP:
                return "RBUTTONUP";
            case WM_CONTEXTMENU:
                return "CONTEXTMENU";
            default:
                return nullptr;
        }
    }

    // Watches the raw messages. WM_POINTERCAPTURECHANGED is the one that makes
    // Qt cancel the whole touch sequence, and the legacy mouse messages below
    // are what provoke it: Qt captures the mouse on any button press, and
    // changing the capture while a contact is down is what Windows reports as a
    // pointer capture change.
    class NativeFilter final : public QAbstractNativeEventFilter {
    public:
        bool nativeEventFilter(const QByteArray &eventType, void *message,
                               qintptr *result) override {
            Q_UNUSED(result)
            if (!message || !eventType.startsWith("windows_"))
                return false;
            if (!EditorTouchController::isProbeEnabled())
                return false;

            const auto *msg = static_cast<MSG *>(message);
            if (msg->message == wmPointerCaptureChanged) {
                qDebug().noquote()
                    << QStringLiteral("win WM_POINTERCAPTURECHANGED pointer=%1 -- Qt will cancel "
                                      "the touch sequence and drop every touch event until the "
                                      "next TouchBegin")
                           .arg(LOWORD(msg->wParam));
                return false;
            }
            if (const auto *name = mouseMessageName(msg->message)) {
                if (fromTouch()) {
                    qDebug().noquote()
                        << QStringLiteral("win %1 synthesized from touch (this is what makes Qt "
                                          "call SetCapture)")
                               .arg(QLatin1String(name));
                }
            }
            return false;
        }
    };
#endif // Q_OS_WIN

} // namespace

void EditorTouchProbe::install() {
    auto *app = QCoreApplication::instance();
    if (!app)
        return;
    static bool installed = false;
    if (installed)
        return;
    installed = true;

    app->installEventFilter(new ApplicationFilter(app));
#ifdef Q_OS_WIN
    // Never removed: it has to outlive every widget, and it does nothing at all
    // while the option is off.
    app->installNativeEventFilter(new NativeFilter);
#endif
}
