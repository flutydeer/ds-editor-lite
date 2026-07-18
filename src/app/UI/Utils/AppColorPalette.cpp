#include "AppColorPalette.h"
#include "ColorUtils.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

LITE_SINGLETON_IMPLEMENT_INSTANCE(AppColorPalette)

AppColorPalette::AppColorPalette() {
    m_palette.fill(QColor(155, 186, 255), colorCount);
}

bool AppColorPalette::load(const QString &jsonFilePath) {
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;

    const auto arr = doc.object().value("baseColors").toArray();
    if (arr.size() != colorCount)
        return false;

    QList<QColor> colors;
    for (const auto &val : arr) {
        QColor c(val.toString());
        if (!c.isValid())
            return false;
        colors.append(c);
    }
    return setColors(colors);
}

bool AppColorPalette::setColors(const QList<QColor> &colors) {
    if (colors.size() != colorCount)
        return false;
    m_palette = colors;
    return true;
}

int AppColorPalette::normalizedIndex(int index) const {
    return ((index % colorCount) + colorCount) % colorCount;
}

QColor AppColorPalette::baseColor(int index) const {
    return m_palette[normalizedIndex(index)];
}

QColor AppColorPalette::clipBackground(int index) const {
    return baseColor(index);
}

QColor AppColorPalette::clipBackgroundSelected(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), 0.15);
}

QColor AppColorPalette::clipBackgroundTransparent(int index) const {
    return ColorUtils::adjustAlpha(baseColor(index), 64);
}

QColor AppColorPalette::clipBorder(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), -0.1);
}

QColor AppColorPalette::clipForeground(int index) const {
    auto lch = ColorUtils::srgbToOkLCH(baseColor(index));
    return lch.L > 0.7 ? QColor(0, 0, 0) : QColor(255, 255, 255);
}

QColor AppColorPalette::noteBackground(int index) const {
    return baseColor(index);
}

QColor AppColorPalette::noteBackgroundSelected(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), 0.15);
}

QColor AppColorPalette::noteBackgroundOverlapped(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), -0.15);
}

QColor AppColorPalette::noteBorder(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), -0.1);
}

QColor AppColorPalette::noteBorderOverlapped(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), -0.15);
}

QColor AppColorPalette::noteForeground(int index) const {
    auto lch = ColorUtils::srgbToOkLCH(baseColor(index));
    return lch.L > 0.7 ? QColor(0, 0, 0) : QColor(255, 255, 255);
}

QColor AppColorPalette::noteForegroundOverlapped(int index) const {
    auto fg = noteForeground(index);
    fg.setAlpha(127);
    return fg;
}

QColor AppColorPalette::noteBackgroundEditingPitch(int index) const {
    auto lch = ColorUtils::srgbToOkLCH(baseColor(index));
    lch.L = 0.32;
    lch.C *= 0.25;
    return ColorUtils::oklchToSRGB(lch);
}

QColor AppColorPalette::noteBorderEditingPitch(int index) const {
    auto lch = ColorUtils::srgbToOkLCH(baseColor(index));
    lch.L = 0.55;
    lch.C *= 0.5;
    return ColorUtils::oklchToSRGB(lch);
}

QColor AppColorPalette::noteForegroundEditingPitch(int index) const {
    return noteBorderEditingPitch(index);
}

QColor AppColorPalette::phonemeEdited(int index) const {
    return baseColor(index);
}

QColor AppColorPalette::phonemeFill(int index) const {
    return ColorUtils::adjustAlpha(baseColor(index), 50);
}

QColor AppColorPalette::paramFillTop(int index) const {
    return ColorUtils::adjustAlpha(baseColor(index), 200);
}

QColor AppColorPalette::paramFillBottom(int index) const {
    return ColorUtils::adjustAlpha(baseColor(index), 10);
}

QColor AppColorPalette::paramFillFlat(int index) const {
    return ColorUtils::adjustAlpha(baseColor(index), 120);
}

QColor AppColorPalette::paramLine(int index) const {
    return baseColor(index);
}

QColor AppColorPalette::speakerMixParamFill(int index) const {
    auto lch = ColorUtils::srgbToOkLCH(baseColor(index));
    lch.L = 0.4;
    lch.C = 0.05;
    return ColorUtils::oklchToSRGB(lch);
}

QColor AppColorPalette::speakerMixDotFill(int index) const {
    auto lch = ColorUtils::srgbToOkLCH(baseColor(index));
    lch.L = 0.55;
    lch.C = 0.05;
    return ColorUtils::oklchToSRGB(lch);
}

QColor AppColorPalette::keyHighlight(int index) const {
    return baseColor(index);
}

QColor AppColorPalette::trackHeaderColor(int index) const {
    return baseColor(index);
}
