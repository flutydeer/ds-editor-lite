#include "UI/Utils/IconUtils.h"
#include "UI/Utils/Theme/ThemeLoader.h"
#include "UI/Utils/ThemeManager.h"

#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QString>

namespace {

    bool expect(bool condition, const QString &scenario, const QString &detail = {}) {
        if (condition)
            return true;
        qCritical().noquote() << scenario << detail;
        return false;
    }

    bool expectPalette(const IconUtils::SvgIconColorPalette &palette, const QColor &primary,
                       const QColor &disabled, const QString &scenario) {
        bool success = true;
        success &= expect(palette.normal == primary, scenario,
                          QStringLiteral("normal color mismatch"));
        success &= expect(palette.active == primary, scenario,
                          QStringLiteral("active color mismatch"));
        success &= expect(palette.selected == primary, scenario,
                          QStringLiteral("selected color mismatch"));
        success &= expect(palette.disabled == disabled, scenario,
                          QStringLiteral("disabled color mismatch"));
        return success;
    }

    bool testUninitializedFallback() {
        return expectPalette(IconUtils::defaultActionPalette(), QColor(240, 240, 240, 255),
                             QColor(240, 240, 240, 102),
                             QStringLiteral("uninitialized action palette should use fallback"));
    }

    bool testTheme(const QString &themeId) {
        const auto definition = ThemeLoader::load(themeId);
        bool success = expect(definition.has_value(),
                              QStringLiteral("theme definition should load: %1").arg(themeId),
                              ThemeLoader::lastError());
        if (!definition)
            return false;

        auto *themeManager = ThemeManager::instance();
        success &= expect(themeManager->applyTheme(themeId),
                          QStringLiteral("ThemeManager should apply: %1").arg(themeId),
                          ThemeLoader::lastError());

        const auto primary = definition->semanticColors.value(QStringLiteral("icon.primary"));
        const auto disabled = definition->semanticColors.value(QStringLiteral("icon.disabled"));
        success &= expect(primary.isValid(),
                          QStringLiteral("icon.primary should resolve: %1").arg(themeId));
        success &= expect(disabled.isValid(),
                          QStringLiteral("icon.disabled should resolve: %1").arg(themeId));
        success &= expect(themeManager->semanticColor(QStringLiteral("icon.primary")) == primary,
                          QStringLiteral("ThemeManager primary token mismatch: %1").arg(themeId));
        success &= expect(themeManager->semanticColor(QStringLiteral("icon.disabled")) == disabled,
                          QStringLiteral("ThemeManager disabled token mismatch: %1").arg(themeId));
        success &= expectPalette(IconUtils::defaultActionPalette(), primary, disabled,
                                 QStringLiteral("theme action palette mismatch: %1").arg(themeId));
        return success;
    }

    bool testFailedThemeKeepsSemanticColors() {
        auto *themeManager = ThemeManager::instance();
        const auto previousThemeId = themeManager->currentThemeId();
        const auto previousPrimary = themeManager->semanticColor(QStringLiteral("icon.primary"));
        const auto previousDisabled = themeManager->semanticColor(QStringLiteral("icon.disabled"));

        bool success = expect(!themeManager->applyTheme(QStringLiteral("missing-theme")),
                              QStringLiteral("missing theme should fail"));
        success &= expect(themeManager->currentThemeId() == previousThemeId,
                          QStringLiteral("failed theme should keep current theme ID"));
        success &= expect(themeManager->semanticColor(QStringLiteral("icon.primary")) ==
                              previousPrimary,
                          QStringLiteral("failed theme should keep icon.primary"));
        success &= expect(themeManager->semanticColor(QStringLiteral("icon.disabled")) ==
                              previousDisabled,
                          QStringLiteral("failed theme should keep icon.disabled"));
        success &= expectPalette(IconUtils::defaultActionPalette(), previousPrimary,
                                 previousDisabled,
                                 QStringLiteral("failed theme should keep action palette"));
        return success;
    }

}

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication application(argc, argv);

    bool success = true;
    success &= testUninitializedFallback();
    success &= testTheme(QStringLiteral("lite-dark"));
    success &= testTheme(QStringLiteral("lite-light"));
    success &= testFailedThemeKeepsSemanticColors();
    return success ? 0 : 1;
}
