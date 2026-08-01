//
// LitePalette — command-line OKLCH ramp generator.
//
// Converts a series of OKLCH colors to sRGB hex using the shared ColorUtils
// implementation (including gamut mapping), so the output matches exactly
// what the theme system produces from colors.json.
//
// Example:
//   LitePalette --C 0.015 --H 265 --L0 0.1 --step 0.04
//

#include <lite/GUI/Utils/ColorUtils.h>

#include <QColor>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

#include <cmath>

namespace {

    QString trimTrailingZeros(const QString &text) {
        if (!text.contains(QLatin1Char('.')))
            return text;
        QString result = text;
        while (result.endsWith(QLatin1Char('0')))
            result.chop(1);
        if (result.endsWith(QLatin1Char('.')))
            result.chop(1);
        return result;
    }

    QString formatLightness(double lightness) {
        return trimTrailingZeros(QString::number(lightness * 100.0, 'f', 3));
    }

    QString formatOklch(const ColorUtils::OkLCH &lch) {
        return QStringLiteral("oklch(%1% %2 %3)")
            .arg(formatLightness(lch.L))
            .arg(trimTrailingZeros(QString::number(lch.C, 'f', 4)))
            .arg(trimTrailingZeros(QString::number(lch.H, 'f', 2)));
    }

} // namespace

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("LitePalette"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(
        "Generate an OKLCH lightness ramp converted to sRGB hex colors.\n"
        "Uses ColorUtils::oklchToSRGB (the same conversion as the theme "
        "system, including gamut mapping)."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption chromaOption(
        QStringLiteral("C"), QStringLiteral("Chroma C. Default: 0.015."),
        QStringLiteral("chroma"), QStringLiteral("0.015"));
    const QCommandLineOption hueOption(
        QStringLiteral("H"), QStringLiteral("Hue H in degrees. Default: 265."),
        QStringLiteral("hue"), QStringLiteral("265"));
    const QCommandLineOption startOption(
        QStringLiteral("L0"), QStringLiteral("Start lightness L. Default: 0.1."),
        QStringLiteral("start"), QStringLiteral("0.1"));
    const QCommandLineOption stepOption(
        QStringLiteral("step"), QStringLiteral("Lightness step. Default: 0.04."),
        QStringLiteral("step"), QStringLiteral("0.04"));
    const QCommandLineOption countOption(
        QStringLiteral("count"), QStringLiteral("Number of steps. Default: "
            "generate until L exceeds 1.0."),
        QStringLiteral("count"));
    const QCommandLineOption jsonOption(
        QStringLiteral("json"), QStringLiteral("Emit a JSON array instead of plain lines."));

    parser.addOption(chromaOption);
    parser.addOption(hueOption);
    parser.addOption(startOption);
    parser.addOption(stepOption);
    parser.addOption(countOption);
    parser.addOption(jsonOption);
    parser.process(app);

    const double chroma = parser.value(chromaOption).toDouble();
    const double hue = parser.value(hueOption).toDouble();
    const double start = parser.value(startOption).toDouble();
    const double step = parser.value(stepOption).toDouble();
    const bool useJson = parser.isSet(jsonOption);

    if (step <= 0.0) {
        QTextStream(stderr) << QStringLiteral("Error: --step must be positive.\n");
        return 1;
    }

    int count = 0;
    if (parser.isSet(countOption)) {
        bool ok = false;
        count = parser.value(countOption).toInt(&ok);
        if (!ok || count < 0) {
            QTextStream(stderr)
                << QStringLiteral("Error: --count must be a non-negative integer.\n");
            return 1;
        }
    } else {
        count = static_cast<int>(std::floor((1.0 - start) / step + 1e-9)) + 1;
        if (count < 0)
            count = 0;
    }

    QTextStream out(stdout);

    if (useJson) {
        QJsonArray array;
        for (int i = 0; i < count; ++i) {
            const double lightness = start + i * step;
            const ColorUtils::OkLCH lch{lightness, chroma, hue};
            const QColor color = ColorUtils::oklchToSRGB(lch);
            QJsonObject item;
            item.insert(QStringLiteral("L"), lightness);
            item.insert(QStringLiteral("C"), chroma);
            item.insert(QStringLiteral("H"), hue);
            item.insert(QStringLiteral("oklch"), formatOklch(lch));
            item.insert(QStringLiteral("hex"), color.name().toUpper());
            array.append(item);
        }
        out << QJsonDocument(array).toJson(QJsonDocument::Indented);
    } else {
        for (int i = 0; i < count; ++i) {
            const double lightness = start + i * step;
            const ColorUtils::OkLCH lch{lightness, chroma, hue};
            const QColor color = ColorUtils::oklchToSRGB(lch);
            out << formatOklch(lch) << "  " << color.name().toUpper() << '\n';
        }
    }

    out.flush();
    return 0;
}
