#include "AppEnvironment.h"

#include "Utils/ApplicationLocale.h"
#include "Utils/FontManager.h"
#include <lite/ProductMetadata.h>
#include <lite/Support/Log.h>
#include <lite/Support/SystemUtils.h>

#include <QMWidgets/ccombobox.h>
#include <QMWidgets/cmenu.h>

#include <QApplication>
#include <QCoreApplication>
#include <QResource>
#include <QStyleFactory>

static void initializeResources() {
    Q_INIT_RESOURCE(lite_res);
}

namespace AppEnvironment {

    void preInit(const AppHostMode hostMode) {
        initializeResources();
        ApplicationLocale::initialize();

        // output log to file
        qInstallMessageHandler(Log::handler);
        qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
        if (hostMode != AppHostMode::Gui)
            return;

        qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
            Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
        if (QSysInfo::productType() == "windows")
            QGuiApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    }

    void postInit(const AppHostMode hostMode) {
        QCoreApplication::setOrganizationName(QString::fromLatin1(LiteProductMetadata::Publisher));
        QCoreApplication::setApplicationName(QString::fromLatin1(LiteProductMetadata::ProductName));
        QCoreApplication::setApplicationVersion(QString::fromLatin1(LiteProductMetadata::Version));
        if (hostMode != AppHostMode::Gui)
            return;

        QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
        QApplication::setApplicationDisplayName(
            QString::fromLatin1(LiteProductMetadata::ProductName));
        QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
        // These backends do not provide the native handles required by platform styles.
        const auto platform = QGuiApplication::platformName();
        if (platform == QStringLiteral("offscreen") || platform == QStringLiteral("minimal"))
            QApplication::setStyle(QStyleFactory::create("fusion"));
        else if (QSysInfo::productType() != "windows")
            QApplication::setStyle(QStyleFactory::create("windows"));
        else
            QApplication::setStyle(QStyleFactory::create("windowsvista"));
        CMenu::setDefaultCornerPreference(CMenu::Round);
        CComboBox::setDefaultCornerPreference(CComboBox::Round);

        auto f = QFont();
        f.setHintingPreference(QFont::PreferNoHinting);
        f.setPixelSize(13);
        QApplication::setFont(f);

        // Initialize FontManager to load custom fonts early (stays Meyers static)
        FontManager::instance();
    }

} // namespace AppEnvironment
