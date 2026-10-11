#include "tst_gui_components.h"

#include "Model/AppOptions/Options/AppearanceOption.h"
#include <lite/GUI/Theme/ThemeIds.h>
#include <lite/GUI/Theme/ThemeColorResolver.h>
#include <lite/GUI/Theme/ThemeLoader.h>
#include <lite/GUI/Theme/ThemeManager.h>
#include <lite/GUI/Utils/IconUtils.h>

#include <QApplication>
#include <QtTest/QTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QList>
#include <QSet>
#include <QStringList>
#include <QTemporaryDir>
#include <QScopeGuard>
#include <QSignalSpy>
#include <QSignalBlocker>
#include <QStyleHints>
#include <qpa/qplatformtheme.h>
#include <qpa/qwindowsysteminterface.h>
#include <private/qguiapplication_p.h>

namespace {

    class ControlledPlatformTheme final : public QPlatformTheme {
    public:
        Qt::ColorScheme colorScheme() const override {
            return requested == Qt::ColorScheme::Unknown ? systemScheme : requested;
        }

        void requestColorScheme(Qt::ColorScheme scheme) override {
            requested = scheme;
            publish();
        }

        void changeSystemScheme(Qt::ColorScheme scheme) {
            systemScheme = scheme;
            publish();
        }

    private:
        void publish() {
            // Use the same notification entry point as Qt's platform themes.
            QWindowSystemInterface::handleThemeChange<
                QWindowSystemInterface::SynchronousDelivery>();
        }

        Qt::ColorScheme requested = Qt::ColorScheme::Unknown;
        Qt::ColorScheme systemScheme = Qt::ColorScheme::Dark;
    };

    std::optional<ThemeColorTable> parse(const QByteArray &json, QString &error) {
        return ThemeColorResolver::parse(json, &error);
    }

    bool writeFile(const QString &path, const QByteArray &content) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            return false;
        return file.write(content) == content.size();
    }
}

namespace {
    struct ThemeEnvironment {
        ThemeEnvironment() {
            themeRootWasSet = qEnvironmentVariableIsSet("DS_EDITOR_THEME_DIR");
            previousThemeRoot = qgetenv("DS_EDITOR_THEME_DIR");
            qunsetenv("DS_EDITOR_THEME_DIR");
        }

        ~ThemeEnvironment() {
            if (themeRootWasSet)
                qputenv("DS_EDITOR_THEME_DIR", previousThemeRoot);
            else
                qunsetenv("DS_EDITOR_THEME_DIR");
        }

        QByteArray previousThemeRoot;
        bool themeRootWasSet = false;
    };
}

void GuiComponentTests::validColorsAndSubstitution() {
    ThemeEnvironment fixture;
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
    QVERIFY2((colors.has_value()), qPrintable(QStringLiteral("valid colors should parse") +
                                              QStringLiteral(": ") + error));

    QCOMPARE((colors->tokens.value(QStringLiteral("surface.window"))), (QColor("#123456")));
    QCOMPARE((colors->tokens.value(QStringLiteral("border.focus")).alpha()), (128));
    QCOMPARE((colors->tokens.value(QStringLiteral("icon.focus"))),
             (colors->tokens.value(QStringLiteral("border.focus"))));

    QSet<QString> usedTokens;
    const auto styleSheet = ThemeColorResolver::applyToStyleSheet(
        QStringLiteral("QWidget { color: ${surface.window}; border-color: ${icon.focus}; }"),
        *colors, &usedTokens, &error);
    QVERIFY2((styleSheet.has_value()),
             qPrintable(QStringLiteral("valid placeholders should resolve") + QStringLiteral(": ") +
                        error));
    QVERIFY2((styleSheet->contains(QStringLiteral("#123456"))),
             qPrintable(QStringLiteral("opaque color should use #RRGGBB")));
    QVERIFY2((!styleSheet->contains(QStringLiteral("${"))),
             qPrintable(QStringLiteral("resolved QSS should contain no placeholders")));
    QCOMPARE((usedTokens),
             (QSet<QString>({QStringLiteral("surface.window"), QStringLiteral("icon.focus")})));
}

void GuiComponentTests::invalidDefinitions_data() {
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<QString>("expectedError");
    QTest::newRow("unknown-alias")
        << QByteArray(
               R"JSON({"palette":{"neutral.base":"#000"},"tokens":{"surface.window":"{missing.color}"}})JSON")
        << QStringLiteral("Unknown color alias");
    QTest::newRow("alias-cycle")
        << QByteArray(
               R"JSON({"palette":{"neutral.base":"#000"},"tokens":{"surface.one":"{surface.two}","surface.two":"{surface.one}"}})JSON")
        << QStringLiteral("alias cycle");
    QTest::newRow("out-of-range-oklch")
        << QByteArray(
               R"JSON({"palette":{"neutral.base":"oklch(120% 0.1 20)"},"tokens":{"surface.window":"#000"}})JSON")
        << QStringLiteral("out of range");
    QTest::newRow("cross-section-duplicate")
        << QByteArray(
               R"JSON({"palette":{"surface.window":"#000"},"tokens":{"surface.window":"#fff"}})JSON")
        << QStringLiteral("both palette and tokens");
    QTest::newRow("same-object-duplicate")
        << QByteArray(
               R"JSON({"palette":{"neutral.base":"#000","neutral.base":"#fff"},"tokens":{"surface.window":"#000"}})JSON")
        << QStringLiteral("Duplicate JSON key");
    QTest::newRow("palette-alias")
        << QByteArray(
               R"JSON({"palette":{"neutral.base":"{neutral.other}"},"tokens":{"surface.window":"#000"}})JSON")
        << QStringLiteral("Unsupported color literal");
    QTest::newRow("escaped-key-duplicate")
        << QByteArray(
               R"JSON({"palette":{"neutral.base":"#000","neutral.\u0062ase":"#fff"},"tokens":{"surface.window":"{neutral.base}"}})JSON")
        << QStringLiteral("Duplicate JSON key");
    QTest::newRow("palette-array")
        << QByteArray(R"JSON({"palette":["#000","#fff"],"tokens":{"surface.window":"#000"}})JSON")
        << QStringLiteral("requires object fields");
    QTest::newRow("empty-tokens")
        << QByteArray(R"JSON({"palette":{"neutral.base":"#000"},"tokens":{}})JSON")
        << QStringLiteral("must not be empty");
    QTest::newRow("numeric-palette-value")
        << QByteArray(
               R"JSON({"palette":{"neutral.base":0},"tokens":{"surface.window":"#000"}})JSON")
        << QStringLiteral("must be a color string");
    QTest::newRow("non-string-token-value")
        << QByteArray(
               R"JSON({"palette":{"neutral.base":"#000"},"tokens":{"surface.window":false}})JSON")
        << QStringLiteral("must be a color string or alias");
    QTest::newRow("invalid-color-key")
        << QByteArray(
               R"JSON({"palette":{"neutral base":"#000"},"tokens":{"surface.window":"#000"}})JSON")
        << QStringLiteral("Invalid color key");
}

void GuiComponentTests::invalidDefinitions() {
    ThemeEnvironment fixture;
    QFETCH(QByteArray, json);
    QFETCH(QString, expectedError);
    QString error;
    const auto colors = parse(json, error);
    QVERIFY2(!colors, qPrintable(error));
    QVERIFY2(error.contains(expectedError), qPrintable(error));
}

void GuiComponentTests::invalidPlaceholders_data() {
    QTest::addColumn<QString>("styleSheet");
    QTest::addColumn<QString>("expectedError");
    QTest::newRow("palette-reference") << QStringLiteral("QWidget { color: ${neutral.base}; }")
                                       << QStringLiteral("references palette directly");
    QTest::newRow("unknown-token") << QStringLiteral("QWidget { color: ${text.unknown}; }")
                                   << QStringLiteral("Unknown QSS color token");
    QTest::newRow("malformed-token")
        << QStringLiteral("QWidget { color: ${surface.window; }") << QStringLiteral("Malformed");
    QTest::newRow("unterminated-placeholder")
        << QStringLiteral("QWidget { color: ${surface.window;") << QStringLiteral("Malformed");
}

void GuiComponentTests::invalidPlaceholders() {
    ThemeEnvironment fixture;
    QFETCH(QString, styleSheet);
    QFETCH(QString, expectedError);
    const QByteArray json = R"JSON({
        "palette": {"neutral.base": "#000"},
        "tokens": {"surface.window": "{neutral.base}"}
    })JSON";
    QString error;
    const auto colors = parse(json, error);
    QVERIFY2(colors.has_value(), qPrintable(error));
    const auto result = ThemeColorResolver::applyToStyleSheet(styleSheet, *colors, nullptr, &error);
    QVERIFY2(!result, qPrintable(error));
    QVERIFY2(error.contains(expectedError), qPrintable(error));
}

void GuiComponentTests::appearanceThemePreference() {
    ThemeEnvironment fixture;
    AppearanceOption option;
    QCOMPARE((option.themeId), (ThemeIds::systemThemePreferenceId()));
    QCOMPARE((option.value().value(QStringLiteral("themeId")).toString()),
             (ThemeIds::systemThemePreferenceId()));

    option.load(QJsonObject{
        {QStringLiteral("themeId"), ThemeIds::lightThemePreferenceId()}
    });
    QCOMPARE((option.themeId), (ThemeIds::lightThemePreferenceId()));
    QCOMPARE((option.value().value(QStringLiteral("themeId")).toString()),
             (ThemeIds::lightThemePreferenceId()));

    option.load(QJsonObject{
        {QStringLiteral("themeId"), ThemeIds::systemThemePreferenceId()}
    });
    QCOMPARE((option.themeId), (ThemeIds::systemThemePreferenceId()));

    option.load(QJsonObject{
        {QStringLiteral("themeId"), QStringLiteral("lite-dark")}
    });
    QCOMPARE((option.themeId), (ThemeIds::systemThemePreferenceId()));

    option.load(QJsonObject{
        {QStringLiteral("themeId"), QStringLiteral("unknown-theme")}
    });
    QCOMPARE((option.themeId), (ThemeIds::systemThemePreferenceId()));
}

void GuiComponentTests::bundledStyleSheets() {
    ThemeEnvironment fixture;
    for (const auto &themeId : {QStringLiteral("lite-dark"), QStringLiteral("lite-light")}) {
        const auto themePath = QString::fromUtf8(TEST_SOURCE_DIR) +
                               QStringLiteral("/src/app/Resources/theme/%1").arg(themeId);
        QFile file(themePath + QStringLiteral("/colors.json"));
        QVERIFY2((file.open(QIODevice::ReadOnly)),
                 qPrintable(QStringLiteral("bundled colors.json should open for stylesheet test") +
                            QStringLiteral(": ") + themeId + QStringLiteral(": ") +
                            file.errorString()));

        QString error;
        const auto colors = parse(file.readAll(), error);
        QVERIFY2((colors.has_value()),
                 qPrintable(QStringLiteral("bundled colors should parse for stylesheet test") +
                            QStringLiteral(": ") + themeId + QStringLiteral(": ") + error));

        QDirIterator iterator(themePath, {QStringLiteral("*.qss")}, QDir::Files);
        while (iterator.hasNext()) {
            QFile styleSheetFile(iterator.next());
            QVERIFY2((styleSheetFile.open(QIODevice::ReadOnly)),
                     qPrintable(QStringLiteral("bundled stylesheet should open") +
                                QStringLiteral(": ") + styleSheetFile.errorString()));

            const auto resolved = ThemeColorResolver::applyToStyleSheet(
                QString::fromUtf8(styleSheetFile.readAll()), *colors, nullptr, &error);
            QVERIFY2((resolved.has_value()),
                     qPrintable(QStringLiteral("bundled stylesheet tokens should resolve") +
                                QStringLiteral(": ") + styleSheetFile.fileName() +
                                QStringLiteral(": ") + error));
        }
    }
}

void GuiComponentTests::externalThemeRoot_data() {
    QTest::addColumn<QString>("brokenFile");
    QTest::addColumn<QByteArray>("replacement");
    QTest::addColumn<bool>("remove");
    QTest::newRow("valid-theme") << QString{} << QByteArray{} << false;
    QTest::newRow("malformed-manifest")
        << QStringLiteral("manifest.json") << QByteArray("{") << false;
    QTest::newRow("missing-colors") << QStringLiteral("colors.json") << QByteArray{} << true;
    QTest::newRow("invalid-colors") << QStringLiteral("colors.json") << QByteArray("{") << false;
    QTest::newRow("invalid-palette-color")
        << QStringLiteral("app-color-palette.json")
        << QByteArray(R"({"baseColors":["not-a-color"]})") << false;
    QTest::newRow("missing-palette")
        << QStringLiteral("app-color-palette.json") << QByteArray{} << true;
    QTest::newRow("malformed-palette")
        << QStringLiteral("app-color-palette.json") << QByteArray("[") << false;
    QTest::newRow("missing-stylesheet") << QStringLiteral("base.qss") << QByteArray{} << true;
    QTest::newRow("unresolved-stylesheet-color")
        << QStringLiteral("base.qss") << QByteArray("QWidget { color: ${missing.token}; }")
        << false;
    QTest::newRow("missing-lyric-style") << QStringLiteral("lyric.qss") << QByteArray{} << true;
    QTest::newRow("unresolved-lyric-color")
        << QStringLiteral("lyric.qss") << QByteArray("QWidget { color: ${missing.token}; }")
        << false;
}

void GuiComponentTests::externalThemeRoot() {
    QFETCH(QString, brokenFile);
    QFETCH(QByteArray, replacement);
    QFETCH(bool, remove);
    ThemeEnvironment fixture;
    QTemporaryDir tempDir;
    QVERIFY2((tempDir.isValid()),
             qPrintable(QStringLiteral("temporary theme root should be created")));

    const auto themeDir = QDir(tempDir.path()).filePath(QStringLiteral("external-test"));
    QVERIFY(QDir().mkpath(themeDir));

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

    QVERIFY2((writeFile(QDir(themeDir).filePath("manifest.json"), manifest)),
             qPrintable(QStringLiteral("external manifest should be writable")));
    QVERIFY2((writeFile(QDir(themeDir).filePath("colors.json"), colors)),
             qPrintable(QStringLiteral("external colors should be writable")));
    QVERIFY2(
        (writeFile(QDir(themeDir).filePath("base.qss"), "QWidget { color: ${surface.window}; }")),
        qPrintable(QStringLiteral("external QSS should be writable")));
    QVERIFY2((writeFile(QDir(themeDir).filePath("app-color-palette.json"), palette)),
             qPrintable(QStringLiteral("external palette should be writable")));
    QVERIFY2(
        (writeFile(QDir(themeDir).filePath("lyric.qss"), "QWidget { color: ${surface.window}; }")),
        qPrintable(QStringLiteral("external lyric QSS should be writable")));

    qputenv("DS_EDITOR_THEME_DIR", tempDir.path().toUtf8());
    const auto candidates = ThemeLoader::themeCandidates();
    QVERIFY2((candidates.contains(QStringLiteral("external-test"))),
             qPrintable(QStringLiteral("external theme should be discoverable")));

    const auto loaded = ThemeLoader::load(QStringLiteral("external-test"));
    QVERIFY2((loaded.has_value()), qPrintable(QStringLiteral("external theme should load") +
                                              QStringLiteral(": ") + ThemeLoader::lastError()));
    QCOMPARE((loaded->name), (QStringLiteral("External Test")));
    QVERIFY2((loaded->styleSheet.contains(QStringLiteral("#123456"))),
             qPrintable(QStringLiteral("external QSS should be resolved")));
    if (!brokenFile.isEmpty()) {
        auto *manager = ThemeManager::instance();
        const auto previousTheme = manager->currentThemeId();
        QWidget root;
        manager->addStyleRoot(&root);
        const auto restoreTheme = qScopeGuard([&] {
            manager->removeStyleRoot(&root);
            if (!previousTheme.isEmpty())
                manager->applyTheme(previousTheme);
        });
        QVERIFY(manager->applyTheme(QStringLiteral("lite-dark")));
        const auto beforeId = manager->currentThemeId();
        const auto beforeStyle = root.styleSheet();
        const auto beforePalette = root.palette();
        const auto beforeColor = manager->semanticColor(QStringLiteral("icon.primary"));
        const auto path = QDir(themeDir).filePath(brokenFile);
        QFile original(path);
        QVERIFY(original.open(QIODevice::ReadOnly));
        const auto originalBytes = original.readAll();
        original.close();
        QVERIFY(remove ? QFile::remove(path) : writeFile(path, replacement));
        QVERIFY(!manager->applyTheme(QStringLiteral("external-test")));
        QVERIFY(!ThemeLoader::lastError().isEmpty());
        QCOMPARE(manager->currentThemeId(), beforeId);
        QCOMPARE(root.styleSheet(), beforeStyle);
        QCOMPARE(manager->styleSheet(), beforeStyle);
        QCOMPARE(root.palette(), beforePalette);
        QCOMPARE(manager->semanticColor(QStringLiteral("icon.primary")), beforeColor);
        QVERIFY(writeFile(path, originalBytes));
        QVERIFY2(manager->applyTheme(QStringLiteral("external-test")),
                 qPrintable(ThemeLoader::lastError()));
        QCOMPARE(manager->currentThemeId(), QStringLiteral("external-test"));
        QVERIFY(root.styleSheet().contains(QStringLiteral("#123456")));
    }
    qunsetenv("DS_EDITOR_THEME_DIR");
}

void GuiComponentTests::bundledThemeLoadingAndFallback() {
    ThemeEnvironment fixture;
    qunsetenv("DS_EDITOR_THEME_DIR");
    const auto bundled = ThemeLoader::load(QStringLiteral("lite-dark"));
    QVERIFY2((bundled.has_value()), qPrintable(QStringLiteral("bundled theme should load") +
                                               QStringLiteral(": ") + ThemeLoader::lastError()));
    const auto bundledLight = ThemeLoader::load(QStringLiteral("lite-light"));
    QVERIFY2((bundledLight.has_value()),
             qPrintable(QStringLiteral("bundled light theme should load") + QStringLiteral(": ") +
                        ThemeLoader::lastError()));
    QCOMPARE((bundledLight->colorType), (QStringLiteral("light")));

    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    const auto overrideDir = QDir(tempDir.path()).filePath(QStringLiteral("lite-dark"));
    QVERIFY(QDir().mkpath(overrideDir));
    QVERIFY2((writeFile(QDir(overrideDir).filePath("manifest.json"), "{}")),
             qPrintable(QStringLiteral("invalid override manifest should be writable")));

    qputenv("DS_EDITOR_THEME_DIR", tempDir.path().toUtf8());
    const auto invalidOverride = ThemeLoader::load(QStringLiteral("lite-dark"));
    QVERIFY2((!invalidOverride),
             qPrintable(QStringLiteral("invalid external override should fail atomically")));
    qunsetenv("DS_EDITOR_THEME_DIR");

    const auto bundledAfterFailure = ThemeLoader::load(QStringLiteral("lite-dark"));
    QVERIFY2(
        (bundledAfterFailure.has_value()),
        qPrintable(QStringLiteral("bundled theme should remain available after override failure") +
                   QStringLiteral(": ") + ThemeLoader::lastError()));
}

void GuiComponentTests::iconPalette_data() {
    QTest::addColumn<QString>("themeId");
    QTest::newRow("dark") << QStringLiteral("lite-dark");
    QTest::newRow("light") << QStringLiteral("lite-light");
}

void GuiComponentTests::iconPalette() {
    ThemeEnvironment fixture;
    QFETCH(QString, themeId);
    const auto definition = ThemeLoader::load(themeId);
    QVERIFY2(definition.has_value(), qPrintable(ThemeLoader::lastError()));
    auto *manager = ThemeManager::instance();
    QVERIFY2(manager->applyTheme(themeId), qPrintable(ThemeLoader::lastError()));

    const auto primary = definition->semanticColors.value(QStringLiteral("icon.primary"));
    const auto disabled = definition->semanticColors.value(QStringLiteral("icon.disabled"));
    QVERIFY(primary.isValid());
    QVERIFY(disabled.isValid());
    QCOMPARE(manager->semanticColor(QStringLiteral("icon.primary")), primary);
    QCOMPARE(manager->semanticColor(QStringLiteral("icon.disabled")), disabled);
    const auto palette = IconUtils::defaultActionPalette();
    QCOMPARE(palette.normal, primary);
    QCOMPARE(palette.active, primary);
    QCOMPARE(palette.selected, primary);
    QCOMPARE(palette.disabled, disabled);
}

void GuiComponentTests::systemThemeNotificationsUpdateBoundWidgets() {
    ThemeEnvironment fixture;
    auto *manager = ThemeManager::instance();
    auto *hints = QGuiApplication::styleHints();
    const auto previousTheme = manager->currentThemeId();
    const auto previousScheme = hints->colorScheme();
    auto *previousPlatform = QGuiApplicationPrivate::platform_theme;
    ControlledPlatformTheme platform;
    QWidget root;
    manager->addStyleRoot(&root);
    manager->addWindow(&root);
    const auto restore = qScopeGuard([&] {
        manager->removeWindow(&root);
        manager->removeStyleRoot(&root);
        QGuiApplicationPrivate::platform_theme = previousPlatform;
        const QSignalBlocker blocker(hints);
        manager->applyTheme(previousTheme);
        hints->setColorScheme(previousScheme);
    });
    QGuiApplicationPrivate::platform_theme = &platform;
    QSignalSpy changed(manager, &ThemeManager::themeChanged);
    QVERIFY(manager->applyThemePreference(ThemeIds::systemThemePreferenceId()));
    QCOMPARE(manager->currentThemeId(), ThemeIds::defaultThemeId());
    const auto darkStyle = root.styleSheet();
    const auto darkIcon = manager->semanticColor(QStringLiteral("icon.primary"));
    changed.clear();

    platform.changeSystemScheme(Qt::ColorScheme::Light);
    QCOMPARE(manager->currentThemeId(), ThemeIds::lightThemeId());
    QCOMPARE(changed.size(), 1);
    QVERIFY(root.styleSheet() != darkStyle);
    QCOMPARE(root.styleSheet(), manager->styleSheet());
    QVERIFY(manager->semanticColor(QStringLiteral("icon.primary")) != darkIcon);
    QCOMPARE(IconUtils::defaultActionPalette().normal,
             manager->semanticColor(QStringLiteral("icon.primary")));

    platform.changeSystemScheme(Qt::ColorScheme::Dark);
    QCOMPARE(manager->currentThemeId(), ThemeIds::defaultThemeId());
    QCOMPARE(changed.size(), 2);
    QCOMPARE(root.styleSheet(), darkStyle);
    QCOMPARE(manager->semanticColor(QStringLiteral("icon.primary")), darkIcon);

    QVERIFY(manager->applyTheme(ThemeIds::lightThemeId()));
    const auto lightStyle = root.styleSheet();
    changed.clear();
    platform.requestColorScheme(Qt::ColorScheme::Dark);
    QCOMPARE(manager->currentThemeId(), ThemeIds::lightThemeId());
    QVERIFY(changed.isEmpty());
    QCOMPARE(root.styleSheet(), lightStyle);
}

void GuiComponentTests::failedThemeKeepsSemanticColors() {
    ThemeEnvironment fixture;
    auto *manager = ThemeManager::instance();
    QVERIFY(manager->applyTheme(QStringLiteral("lite-dark")));
    const auto previousThemeId = manager->currentThemeId();
    const auto previousPrimary = manager->semanticColor(QStringLiteral("icon.primary"));
    const auto previousDisabled = manager->semanticColor(QStringLiteral("icon.disabled"));

    QVERIFY(!manager->applyTheme(QStringLiteral("missing-theme")));
    QCOMPARE(manager->currentThemeId(), previousThemeId);
    QCOMPARE(manager->semanticColor(QStringLiteral("icon.primary")), previousPrimary);
    QCOMPARE(manager->semanticColor(QStringLiteral("icon.disabled")), previousDisabled);
    const auto palette = IconUtils::defaultActionPalette();
    QCOMPARE(palette.normal, previousPrimary);
    QCOMPARE(palette.active, previousPrimary);
    QCOMPARE(palette.selected, previousPrimary);
    QCOMPARE(palette.disabled, previousDisabled);
}
