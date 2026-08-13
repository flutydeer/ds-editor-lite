#include "AppEnvironment.h"

#include "Utils/ApplicationLocale.h"
#include "Utils/FontManager.h"
#include <lite/ProductMetadata.h>
#include <lite/Support/Log.h>
#include <lite/Support/SystemUtils.h>

#include <QMWidgets/ccombobox.h>
#include <QMWidgets/cmenu.h>

#include <QApplication>
#include <QStyleFactory>

namespace AppEnvironment {

    void preInit() {
        ApplicationLocale::initialize();

        // output log to file
        qInstallMessageHandler(Log::handler);
        qputenv("QT_ASSUME_STDERR_HAS_CONSOLE", "1");
        qputenv("QT_ENABLE_HIGHDPI_SCALING", "1");
        QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
            Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
        if (QSysInfo::productType() == "windows")
            QGuiApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    }

    void postInit() {
        QApplication::setEffectEnabled(Qt::UI_AnimateTooltip, false);
        QApplication::setOrganizationName(
            QString::fromLatin1(LiteProductMetadata::Publisher));
        QApplication::setApplicationName(
            QString::fromLatin1(LiteProductMetadata::ProductName));
        QApplication::setApplicationDisplayName(
            QString::fromLatin1(LiteProductMetadata::ProductName));
        QApplication::setApplicationVersion(
            QString::fromLatin1(LiteProductMetadata::Version));
        QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
        if (QSysInfo::productType() != "windows")
            QApplication::setStyle(QStyleFactory::create("windows"));
        else
            QApplication::setStyle(QStyleFactory::create("windowsvista"));
        CMenu::setDefaultCornerPreference(CMenu::Round);
        CComboBox::setDefaultCornerPreference(CComboBox::Round);

        auto f = SystemUtils::isWindows() ? QFont("Microsoft Yahei UI") : QFont();
        f.setHintingPreference(QFont::PreferNoHinting);
        f.setPixelSize(13);
        QApplication::setFont(f);

        // Initialize FontManager to load custom fonts early (stays Meyers static)
        FontManager::instance();
    }

} // namespace AppEnvironment
