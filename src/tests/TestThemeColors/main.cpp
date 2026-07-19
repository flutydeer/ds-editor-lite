#include "UI/Utils/Theme/ThemeColorResolver.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDirIterator>
#include <QFile>
#include <QList>
#include <QSet>
#include <QStringList>

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
        QFile file(QString::fromUtf8(TEST_SOURCE_DIR) +
                   QStringLiteral("/src/app/Resources/theme/lite-dark/colors.json"));
        if (!expect(file.open(QIODevice::ReadOnly),
                    QStringLiteral("bundled colors.json should open"), file.errorString())) {
            return false;
        }

        QString error;
        const auto colors = parse(file.readAll(), error);
        bool success =
            expect(colors.has_value(), QStringLiteral("bundled colors.json should parse"), error);
        if (colors) {
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
            for (const auto &token : requiredTokens) {
                success &=
                    expect(colors->tokens.contains(token),
                           QStringLiteral("bundled colors.json missing required token"), token);
            }
        }
        return success;
    }

    bool testBundledStyleSheets() {
        QFile file(QString::fromUtf8(TEST_SOURCE_DIR) +
                   QStringLiteral("/src/app/Resources/theme/lite-dark/colors.json"));
        if (!expect(file.open(QIODevice::ReadOnly),
                    QStringLiteral("bundled colors.json should open for stylesheet test"),
                    file.errorString())) {
            return false;
        }

        QString error;
        const auto colors = parse(file.readAll(), error);
        if (!expect(colors.has_value(),
                    QStringLiteral("bundled colors should parse for stylesheet test"), error)) {
            return false;
        }

        bool success = true;
        QDirIterator iterator(QString::fromUtf8(TEST_SOURCE_DIR) +
                                  QStringLiteral("/src/app/Resources/theme/lite-dark"),
                              {QStringLiteral("*.qss")}, QDir::Files);
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
    success &= testBundledStyleSheets();
    return success ? 0 : 1;
}
