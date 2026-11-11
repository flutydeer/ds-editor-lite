#include "WindowChromeCapture.h"

#include "Model/AppOptions/AppOptions.h"
#include "UI/Views/MainTitleBar/MainTitleBar.h"
#include <lite/GUI/Utils/WindowFrameUtils.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QTimer>
#include <QWidget>
#include <QFile>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QWindow>

namespace {
    // Long enough for the first paint, the theme style sheet and the initial
    // document to settle on a cold CI runner.
    constexpr int kDefaultDelayMs = 6000;

    QJsonArray rectJson(const QRect &rect) {
        return {rect.x(), rect.y(), rect.width(), rect.height()};
    }

    QJsonArray marginsJson(const QMargins &margins) {
        return {margins.left(), margins.top(), margins.right(), margins.bottom()};
    }
} // namespace

void WindowChromeCapture::armIfRequested(QWidget &window) {
    const auto directory = qEnvironmentVariable("LITE_CHROME_CAPTURE").trimmed();
    if (directory.isEmpty())
        return;

    bool hasDelay = false;
    const auto requestedDelay =
        qEnvironmentVariableIntValue("LITE_CHROME_CAPTURE_DELAY_MS", &hasDelay);
    const auto delay = hasDelay && requestedDelay > 0 ? requestedDelay : kDefaultDelayMs;

    const bool nativeFrame = appOptions->appearance()->effectiveUseNativeFrame();
    const auto mode = nativeFrame ? QStringLiteral("native-frame") : QStringLiteral("frameless");
    const auto basePath = QDir(directory).filePath(QStringLiteral("title-bar-%1").arg(mode));
    QDir().mkpath(QFileInfo(basePath).absolutePath());

    qInfo("Chrome capture armed: %s.png in %d ms", qPrintable(basePath), delay);
    QTimer::singleShot(delay, &window, [&window, basePath, mode, nativeFrame] {
        // Observe the production window without merging or relaying it out here.
        // In particular, the native-frame reference must keep its native title bar.
        const auto *handle = window.windowHandle();
        const auto *titleBar = window.findChild<MainTitleBar *>();
        const auto titleBarRect =
            titleBar ? QRect(titleBar->mapTo(&window, QPoint()), titleBar->size()) : QRect();
        QJsonObject geometry{
            {QStringLiteral("mode"),             mode                                 },
            {QStringLiteral("platform"),         QGuiApplication::platformName()      },
            {QStringLiteral("devicePixelRatio"), window.devicePixelRatioF()           },
            {QStringLiteral("fullScreen"),       window.isFullScreen()                },
            {QStringLiteral("maximized"),        window.isMaximized()                 },
            {QStringLiteral("widgetRect"),       rectJson(window.rect())              },
            {QStringLiteral("contentsRect"),     rectJson(window.contentsRect())      },
            {QStringLiteral("contentsMargins"),  marginsJson(window.contentsMargins())},
            {QStringLiteral("respectsSafeArea"),
             window.testAttribute(Qt::WA_ContentsMarginsRespectsSafeArea)             },
            {QStringLiteral("safeAreaMargins"),
             marginsJson(handle ? handle->safeAreaMargins() : QMargins())             },
            {QStringLiteral("frameMargins"),
             marginsJson(handle ? handle->frameMargins() : QMargins())                },
            {QStringLiteral("titleBarRect"),     rectJson(titleBarRect)               }
        };
        const auto imagePath = basePath + QStringLiteral(".png");
        QString captureMethod;
        const bool captured =
            WindowFrameUtils::captureWindowChrome(&window, imagePath, &captureMethod);
        geometry.insert(QStringLiteral("captureMethod"), captureMethod);
        const auto json = QJsonDocument(geometry).toJson();
        qInfo().noquote() << "Chrome Qt geometry:"
                          << QJsonDocument(geometry).toJson(QJsonDocument::Compact);
        QFile report(basePath + QStringLiteral(".json"));
        if (!report.open(QIODevice::WriteOnly) || report.write(json) != json.size()) {
            qCritical("Chrome geometry report failed: %s", qPrintable(report.fileName()));
            QCoreApplication::exit(2);
            return;
        }
        report.close();

        if (!captured) {
            qCritical("Chrome capture failed: %s", qPrintable(imagePath));
            QCoreApplication::exit(2);
            return;
        }
        qInfo("Chrome capture written: %s", qPrintable(imagePath));

        // Keep the image and report on failure so CI artifacts explain the gap.
        if (QGuiApplication::platformName() == QStringLiteral("cocoa") && !nativeFrame &&
            !window.isFullScreen() &&
            (!handle || !titleBar || window.contentsRect().top() != 0 || titleBarRect.top() != 0 ||
             handle->frameMargins().top() != 0)) {
            qCritical("Frameless chrome still reserves space above the custom title bar");
            QCoreApplication::exit(3);
            return;
        }
        QCoreApplication::quit();
    });
}
