#include "TrackColorPalette.h"
#include "ColorUtils.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

LITE_SINGLETON_IMPLEMENT_INSTANCE(TrackColorPalette)

TrackColorPalette::TrackColorPalette() {
    m_palette.fill(QColor(155, 186, 255), colorCount);
}

bool TrackColorPalette::load(const QString &jsonFilePath) {
    QFile file(jsonFilePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;

    const auto arr = doc.object().value("trackColors").toArray();
    if (arr.size() != colorCount)
        return false;

    QList<QColor> colors;
    for (const auto &val : arr) {
        QColor c(val.toString());
        if (!c.isValid())
            return false;
        colors.append(c);
    }
    m_palette = colors;
    return true;
}

int TrackColorPalette::normalizedIndex(int index) const {
    return ((index % colorCount) + colorCount) % colorCount;
}

QColor TrackColorPalette::baseColor(int index) const {
    return m_palette[normalizedIndex(index)];
}

QColor TrackColorPalette::clipBackground(int index) const {
    return baseColor(index);
}

QColor TrackColorPalette::clipBackgroundSelected(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), 0.15);
}

QColor TrackColorPalette::clipBackgroundTransparent(int index) const {
    return ColorUtils::adjustAlpha(baseColor(index), 64);
}

QColor TrackColorPalette::clipBorder(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), -0.1);
}

QColor TrackColorPalette::clipForeground(int index) const {
    auto lch = ColorUtils::srgbToOkLCH(baseColor(index));
    return lch.L > 0.7 ? QColor(0, 0, 0) : QColor(255, 255, 255);
}

QColor TrackColorPalette::noteBackground(int index) const {
    return baseColor(index);
}

QColor TrackColorPalette::noteBackgroundSelected(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), 0.15);
}

QColor TrackColorPalette::noteBackgroundOverlapped(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), -0.15);
}

QColor TrackColorPalette::noteBorder(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), -0.1);
}

QColor TrackColorPalette::noteBorderOverlapped(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), -0.15);
}

QColor TrackColorPalette::noteForeground(int index) const {
    auto lch = ColorUtils::srgbToOkLCH(baseColor(index));
    return lch.L > 0.7 ? QColor(0, 0, 0) : QColor(255, 255, 255);
}

QColor TrackColorPalette::noteForegroundOverlapped(int index) const {
    auto fg = noteForeground(index);
    fg.setAlpha(127);
    return fg;
}

QColor TrackColorPalette::phonemeEdited(int index) const {
    return baseColor(index);
}

QColor TrackColorPalette::phonemeFill(int index) const {
    return ColorUtils::adjustAlpha(baseColor(index), 50);
}

QColor TrackColorPalette::paramFillTop(int index) const {
    return ColorUtils::adjustAlpha(baseColor(index), 200);
}

QColor TrackColorPalette::paramFillBottom(int index) const {
    return ColorUtils::adjustAlpha(baseColor(index), 10);
}

QColor TrackColorPalette::paramFillFlat(int index) const {
    return ColorUtils::adjustAlpha(baseColor(index), 120);
}

QColor TrackColorPalette::paramLine(int index) const {
    return baseColor(index);
}

QColor TrackColorPalette::keyHighlight(int index) const {
    return baseColor(index);
}

QColor TrackColorPalette::trackHeaderColor(int index) const {
    return baseColor(index);
}

int TrackColorPalette::nextColorIndex() {
    int idx = m_nextIndex;
    m_nextIndex = (m_nextIndex + 1) % colorCount;
    return idx;
}
