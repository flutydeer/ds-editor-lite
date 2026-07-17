#include "AppEnvironment.h"

#include "Utils/FontManager.h"
#include "Utils/Log.h"
#include "Utils/SystemUtils.h"

#include <QMWidgets/ccombobox.h>
#include <QMWidgets/cmenu.h>

#include <QApplication>
#include <QStyleFactory>
#include <QStyleHints>

namespace AppEnvironment {

    void preInit() {
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
        QApplication::setOrganizationName("OpenVPI");
        QApplication::setApplicationName("DS Editor Lite");
        QApplication::setApplicationDisplayName("Lite");
        QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
        if (QSysInfo::productType() != "windows")
            QApplication::setStyle(QStyleFactory::create("windows"));
        else
            QApplication::setStyle(QStyleFactory::create("windowsvista"));
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
        QApplication::styleHints()->setColorScheme(Qt::ColorScheme::Dark);
#else
        qWarning("setColorScheme is not available in this version of Qt.");
#endif

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
