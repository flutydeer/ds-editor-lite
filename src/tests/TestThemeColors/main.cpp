#include "Model/AppOptions/Options/AppearanceOption.h"
#include "UI/Utils/Theme/ThemeColorResolver.h"
#include "UI/Utils/Theme/ThemeLoader.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>

namespace {

    bool expect(bool condition, const QString &scenario, const QString &detail = {}) {
        if (condition)
            return true;
        qCritical().noquote() << scenario << detail;
        return false;
    }

    std::optional<ThemeColorTable> parse(const QByteArray &json, QString &error) {
        return ThemeColorResolver::parse(json, &error);
    }

    bool testValidColorsAndSubstitution() {
        const QByteArray json = R"JSON({
            "palette": {
                "neutral.base": "#123456",
                "accent.base": "oklch(70% 0.12 260 / 50%)"
            },
            "tokens": {
                "surface.window": "{neutral.base}",
                "border.focus": "{accent.base}",
                "icon.focus": "{border.focus}"
            }
        })JSON";

        QString error;
        const auto colors = parse(json, error);
        bool success =
            expect(colors.has_value(), QStringLiteral("valid colors should parse"), error);
        if (!colors)
            return false;

        success &=
            expect(colors->tokens.value(QStringLiteral("surface.window")) == QColor("#123456"),
                   QStringLiteral("palette alias should resolve"));
        success &= expect(colors->tokens.value(QStringLiteral("border.focus")).alpha() == 128,
                          QStringLiteral("OKLCH alpha should resolve to 50%"));
        success &= expect(colors->tokens.value(QStringLiteral("icon.focus")) ==
                              colors->tokens.value(QStringLiteral("border.focus")),
                          QStringLiteral("token alias should resolve"));

        QSet<QString> usedTokens;
        const auto styleSheet = ThemeColorResolver::applyToStyleSheet(
            QStringLiteral("QWidget { color: ${surface.window}; border-color: ${icon.focus}; }"),
            *colors, &usedTokens, &error);
        success &= expect(styleSheet.has_value(),
                          QStringLiteral("valid placeholders should resolve"), error);
        if (styleSheet) {
            success &= expect(styleSheet->contains(QStringLiteral("#123456")),
                              QStringLiteral("opaque color should use #RRGGBB"));
            success &= expect(!styleSheet->contains(QStringLiteral("${")),
                              QStringLiteral("resolved QSS should contain no placeholders"));
        }
        success &= expect(usedTokens == QSet<QString>({QStringLiteral("surface.window"),
                                                       QStringLiteral("icon.focus")}),
                          QStringLiteral("used token set should be reported"));
        return success;
    }

    bool testInvalidDefinitions() {
        struct Case {
            QByteArray json;
            QString expectedError;
            const char *scenario;
        };

        const QList<Case> cases{
            {
             R"JSON({"palette":{"neutral.base":"#000"},"tokens":{"surface.window":"{missing.color}"}})JSON",
             QStringLiteral("Unknown color alias"),
             "unknown alias", },
            {
             R"JSON({"palette":{"neutral.base":"#000"},"tokens":{"surface.one":"{surface.two}","surface.two":"{surface.one}"}})JSON",
             QStringLiteral("alias cycle"),
             "alias cycle", },
            {
             R"JSON({"palette":{"neutral.base":"oklch(120% 0.1 20)"},"tokens":{"surface.window":"#000"}})JSON",
             QStringLiteral("out of range"),
             "out-of-range OKLCH", },
            {
             R"JSON({"palette":{"surface.window":"#000"},"tokens":{"surface.window":"#fff"}})JSON",                                                                                               QStringLiteral("both palette and tokens"),
             "cross-section duplicate", },
            {
             R"JSON({"palette":{"neutral.base":"#000","neutral.base":"#fff"},"tokens":{"surface.window":"#000"}})JSON",
             QStringLiteral("Duplicate JSON key"),
             "same-object duplicate", },
            {
             R"JSON({"palette":{"neutral.base":"{neutral.other}"},"tokens":{"surface.window":"#000"}})JSON",
             QStringLiteral("Unsupported color literal"),
             "palette alias", },
        };

        bool success = true;
        for (const auto &testCase : cases) {
            QString error;
            const auto colors = parse(testCase.json, error);
            success &= expect(!colors && error.contains(testCase.expectedError),
                              QString::fromUtf8(testCase.scenario), error);
        }
        return success;
    }

    bool testInvalidPlaceholders() {
        const QByteArray json = R"JSON({
            "palette": {"neutral.base": "#000"},
            "tokens": {"surface.window": "{neutral.base}"}
        })JSON";
        QString error;
        const auto colors = parse(json, error);
        if (!expect(colors.has_value(), QStringLiteral("placeholder fixture should parse"), error))
            return false;

        bool success = true;
        auto result = ThemeColorResolver::applyToStyleSheet(
            QStringLiteral("QWidget { color: ${neutral.base}; }"), *colors, nullptr, &error);
        success &= expect(!result && error.contains(QStringLiteral("references palette directly")),
                          QStringLiteral("direct palette placeholder should fail"), error);

        result = ThemeColorResolver::applyToStyleSheet(
            QStringLiteral("QWidget { color: ${text.unknown}; }"), *colors, nullptr, &error);
        success &= expect(!result && error.contains(QStringLiteral("Unknown QSS color token")),
                          QStringLiteral("unknown token placeholder should fail"), error);

        result = ThemeColorResolver::applyToStyleSheet(
            QStringLiteral("QWidget { color: ${surface.window; }"), *colors, nullptr, &error);
        success &= expect(!result && error.contains(QStringLiteral("Malformed")),
                          QStringLiteral("malformed placeholder should fail"), error);
        return success;
    }

    bool testBundledTokenDraft() {
        const QStringList requiredTokens{
            QStringLiteral("surface.window"),
            QStringLiteral("text.primary"),
            QStringLiteral("control.fill.checkedPressed"),
            QStringLiteral("control.border.focus"),
            QStringLiteral("control.foreground.disabled"),
            QStringLiteral("toast.background"),
            QStringLiteral("editor.canvas"),
            QStringLiteral("editor.trackHover"),
            QStringLiteral("piano.roll.blackRow"),
            QStringLiteral("curve.anchorSelected"),
            QStringLiteral("meter.trackBackground"),
            QStringLiteral("timeline.ruler.subdivisionFrom"),
            QStringLiteral("timeline.task.runningLow"),
            QStringLiteral("timeline.task.runningHigh"),
            QStringLiteral("phoneme.waveform"),
            QStringLiteral("speakerMix.track"),
            QStringLiteral("speakerMix.emptyState.fill"),
            QStringLiteral("mix.fader.trackInactive"),
            QStringLiteral("button.mute.checkedHover.fill"),
            QStringLiteral("button.solo.checkedPressed.fill"),
        };

        bool success = true;
        for (const auto &themeId : {QStringLiteral("lite-dark"), QStringLiteral("lite-light")}) {
            QFile file(QString::fromUtf8(TEST_SOURCE_DIR) +
                       QStringLiteral("/src/app/Resources/theme/%1/colors.json").arg(themeId));
            if (!expect(file.open(QIODevice::ReadOnly),
                        QStringLiteral("bundled colors.json should open"),
                        themeId + QStringLiteral(": ") + file.errorString())) {
                success = false;
                continue;
            }

            QString error;
            const auto colors = parse(file.readAll(), error);
            success &=
                expect(colors.has_value(), QStringLiteral("bundled colors.json should parse"),
                       themeId + QStringLiteral(": ") + error);
            if (!colors)
                continue;

            for (const auto &token : requiredTokens) {
                success &= expect(colors->tokens.contains(token),
                                  QStringLiteral("bundled colors.json missing required token"),
                                  themeId + QStringLiteral(": ") + token);
            }
        }
        return success;
    }

    bool testAppearanceThemePreference() {
        AppearanceOption option;
        bool success = expect(option.themeId == AppearanceOption::systemThemePreferenceId(),
                              QStringLiteral("system theme should be the default preference"));
        success &= expect(option.value().value(QStringLiteral("themeId")).toString() ==
                              AppearanceOption::systemThemePreferenceId(),
                          QStringLiteral("system theme preference should serialize by stable ID"));

        option.load(QJsonObject{
            {QStringLiteral("themeId"), AppearanceOption::lightThemePreferenceId()}
        });
        success &= expect(option.themeId == AppearanceOption::lightThemePreferenceId(),
                          QStringLiteral("light theme preference should load"));
        success &= expect(option.value().value(QStringLiteral("themeId")).toString() ==
                              AppearanceOption::lightThemePreferenceId(),
                          QStringLiteral("theme preference should serialize by stable ID"));

        option.load(QJsonObject{
            {QStringLiteral("themeId"), AppearanceOption::systemThemePreferenceId()}
        });
        success &= expect(option.themeId == AppearanceOption::systemThemePreferenceId(),
                          QStringLiteral("system theme preference should load"));

        option.load(QJsonObject{
            {QStringLiteral("themeId"), QStringLiteral("lite-dark")}
        });
        success &= expect(option.themeId == AppearanceOption::systemThemePreferenceId(),
                          QStringLiteral("legacy theme ids should be ignored"));

        option.load(QJsonObject{
            {QStringLiteral("themeId"), QStringLiteral("unknown-theme")}
        });
        success &= expect(option.themeId == AppearanceOption::systemThemePreferenceId(),
                          QStringLiteral("invalid theme preference should be ignored"));
        return success;
    }

    bool testBundledStyleSheets() {
        bool success = true;
        for (const auto &themeId : {QStringLiteral("lite-dark"), QStringLiteral("lite-light")}) {
            const auto themePath = QString::fromUtf8(TEST_SOURCE_DIR) +
                                   QStringLiteral("/src/app/Resources/theme/%1").arg(themeId);
            QFile file(themePath + QStringLiteral("/colors.json"));
            if (!expect(file.open(QIODevice::ReadOnly),
                        QStringLiteral("bundled colors.json should open for stylesheet test"),
                        themeId + QStringLiteral(": ") + file.errorString())) {
                success = false;
                continue;
            }

            QString error;
            const auto colors = parse(file.readAll(), error);
            if (!expect(colors.has_value(),
                        QStringLiteral("bundled colors should parse for stylesheet test"),
                        themeId + QStringLiteral(": ") + error)) {
                success = false;
                continue;
            }

            QDirIterator iterator(themePath, {QStringLiteral("*.qss")}, QDir::Files);
            while (iterator.hasNext()) {
                QFile styleSheetFile(iterator.next());
                if (!expect(styleSheetFile.open(QIODevice::ReadOnly),
                            QStringLiteral("bundled stylesheet should open"),
                            styleSheetFile.errorString())) {
                    success = false;
                    continue;
                }

                const auto resolved = ThemeColorResolver::applyToStyleSheet(
                    QString::fromUtf8(styleSheetFile.readAll()), *colors, nullptr, &error);
                success &= expect(resolved.has_value(),
                                  QStringLiteral("bundled stylesheet tokens should resolve"),
                                  styleSheetFile.fileName() + QStringLiteral(": ") + error);
            }
        }
        return success;
    }

    bool writeFile(const QString &path, const QByteArray &content) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        return file.write(content) == content.size();
    }

    bool testExternalThemeRoot() {
        QTemporaryDir tempDir;
        if (!expect(tempDir.isValid(), QStringLiteral("temporary theme root should be created")))
            return false;

        const auto themeDir = QDir(tempDir.path()).filePath(QStringLiteral("external-test"));
        if (!QDir().mkpath(themeDir))
            return false;

        const QByteArray colors = R"JSON({
            "palette": {"neutral.base": "#123456"},
            "tokens": {"surface.window": "{neutral.base}"}
        })JSON";
        const QByteArray manifest = R"JSON({
            "name": "External Test",
            "author": "Test",
            "colorType": "light",
            "colors": "colors.json",
            "styleSheets": ["base.qss"],
            "appColorPalette": "app-color-palette.json",
            "lyricStyleSheet": "lyric.qss"
        })JSON";
        const QByteArray palette = R"JSON({
            "baseColors": ["#111111", "#222222", "#333333", "#444444", "#555555", "#666666",
                            "#777777", "#888888", "#999999", "#aaaaaa", "#bbbbbb", "#cccccc"]
        })JSON";

        bool success = true;
        success &= expect(writeFile(QDir(themeDir).filePath("manifest.json"), manifest),
                          QStringLiteral("external manifest should be writable"));
        success &= expect(writeFile(QDir(themeDir).filePath("colors.json"), colors),
                          QStringLiteral("external colors should be writable"));
        success &= expect(
            writeFile(QDir(themeDir).filePath("base.qss"), "QWidget { color: ${surface.window}; }"),
            QStringLiteral("external QSS should be writable"));
        success &= expect(writeFile(QDir(themeDir).filePath("app-color-palette.json"), palette),
                          QStringLiteral("external palette should be writable"));
        success &= expect(writeFile(QDir(themeDir).filePath("lyric.qss"),
                                    "QWidget { color: ${surface.window}; }"),
                          QStringLiteral("external lyric QSS should be writable"));
        if (!success)
            return false;

        qputenv("DS_EDITOR_THEME_DIR", tempDir.path().toUtf8());
        const auto candidates = ThemeLoader::themeCandidates();
        success &= expect(candidates.contains(QStringLiteral("external-test")),
                          QStringLiteral("external theme should be discoverable"));

        const auto loaded = ThemeLoader::load(QStringLiteral("external-test"));
        success &= expect(loaded.has_value(), QStringLiteral("external theme should load"),
                          ThemeLoader::lastError());
        if (loaded) {
            success &= expect(loaded->name == QStringLiteral("External Test"),
                              QStringLiteral("external manifest should be used"));
            success &= expect(loaded->styleSheet.contains(QStringLiteral("#123456")),
                              QStringLiteral("external QSS should be resolved"));
        }
        qunsetenv("DS_EDITOR_THEME_DIR");
        return success;
    }

    bool testBundledThemeLoadingAndFallback() {
        qunsetenv("DS_EDITOR_THEME_DIR");
        const auto bundled = ThemeLoader::load(QStringLiteral("lite-dark"));
        bool success = expect(bundled.has_value(), QStringLiteral("bundled theme should load"),
                              ThemeLoader::lastError());
        const auto bundledLight = ThemeLoader::load(QStringLiteral("lite-light"));
        success &=
            expect(bundledLight.has_value(), QStringLiteral("bundled light theme should load"),
                   ThemeLoader::lastError());
        if (bundledLight) {
            success &=
                expect(bundledLight->colorType == QStringLiteral("light"),
                       QStringLiteral("bundled light theme should declare light color type"));
        }

        QTemporaryDir tempDir;
        if (!tempDir.isValid())
            return false;
        const auto overrideDir = QDir(tempDir.path()).filePath(QStringLiteral("lite-dark"));
        QDir().mkpath(overrideDir);
        success &= expect(writeFile(QDir(overrideDir).filePath("manifest.json"), "{}"),
                          QStringLiteral("invalid override manifest should be writable"));

        qputenv("DS_EDITOR_THEME_DIR", tempDir.path().toUtf8());
        const auto invalidOverride = ThemeLoader::load(QStringLiteral("lite-dark"));
        success &= expect(!invalidOverride,
                          QStringLiteral("invalid external override should fail atomically"));
        qunsetenv("DS_EDITOR_THEME_DIR");

        const auto bundledAfterFailure = ThemeLoader::load(QStringLiteral("lite-dark"));
        success &=
            expect(bundledAfterFailure.has_value(),
                   QStringLiteral("bundled theme should remain available after override failure"),
                   ThemeLoader::lastError());
        return success;
    }

}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    bool success = true;
    success &= testValidColorsAndSubstitution();
    success &= testInvalidDefinitions();
    success &= testInvalidPlaceholders();
    success &= testBundledTokenDraft();
    success &= testAppearanceThemePreference();
    success &= testBundledStyleSheets();
    success &= testExternalThemeRoot();
    success &= testBundledThemeLoadingAndFallback();
    return success ? 0 : 1;
}
