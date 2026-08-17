#include "AppColorPalette.h"
#include <lite/GUI/Utils/ColorUtils.h>
#include <lite/GUI/Theme/ThemeManager.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

AppColorPalette *AppColorPalette::instance() {
    static AppColorPalette obj;
    return &obj;
}

AppColorPalette::AppColorPalette() {
    m_palette.fill(QColor(155, 186, 255), colorCount);
    rebuildEditColors();

    // The theme system loads the palette; pull the current one and follow future
    // theme changes. Both orderings are covered: if a theme was already applied,
    // the immediate pull picks it up; later changes arrive via themeChanged.
    const auto pull = [this] { setColors(ThemeManager::instance()->currentPaletteColors()); };
    pull();
    QObject::connect(ThemeManager::instance(), &ThemeManager::themeChanged, pull);
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
    rebuildEditColors();
    return true;
}

void AppColorPalette::rebuildEditColors() {
    // Blend each business color over the editor's white-key row background to get a
    // pseudo-transparent look that stays distinct between light and dark themes.
    // The result is baked per-index, so painting is O(1) and avoids dynamic alpha.
    const auto canvas =
        ThemeManager::instance()->semanticColor(QStringLiteral("editor.canvasAlternate"));
    const double kFillAlpha = 64.0 / 255.0;        // matches clipBackgroundTransparent
    const double kBorderAlpha = 160.0 / 255.0;     // stronger edge, keep hue
    const double kForegroundAlpha = 210.0 / 255.0; // readable text on pale fill

    const auto blend = [](const QColor &bg, const QColor &fg, const double a) {
        return QColor::fromRgb(qRound(bg.red() + (fg.red() - bg.red()) * a),
                               qRound(bg.green() + (fg.green() - bg.green()) * a),
                               qRound(bg.blue() + (fg.blue() - bg.blue()) * a));
    };

    m_noteEditFill.resize(colorCount);
    m_noteEditBorder.resize(colorCount);
    m_noteEditForeground.resize(colorCount);
    for (int i = 0; i < colorCount; ++i) {
        const auto base = m_palette.at(i);
        m_noteEditFill[i] = canvas.isValid() ? blend(canvas, base, kFillAlpha) : base;
        m_noteEditBorder[i] = canvas.isValid() ? blend(canvas, base, kBorderAlpha) : base;
        m_noteEditForeground[i] = canvas.isValid() ? blend(canvas, base, kForegroundAlpha) : base;
    }
}

int AppColorPalette::normalizedIndex(int index) const {
    return ((index % colorCount) + colorCount) % colorCount;
}

bool AppColorPalette::isLight() const {
    return ThemeManager::instance()->colorType() == ThemeManager::ThemeColorType::Light;
}

QColor AppColorPalette::lightestNeutral() const {
    // editor.canvasAlternate resolves to the lightest neutral in the light theme.
    const auto color =
        ThemeManager::instance()->semanticColor(QStringLiteral("editor.canvasAlternate"));
    return color.isValid() ? color : QColor(Qt::white);
}

double AppColorPalette::selectedLightnessDelta() const {
    return isLight() ? 0.10 : 0.15; // keep the selection vivid on the darkened light base
}

QColor AppColorPalette::baseColor(int index) const {
    return m_palette[normalizedIndex(index)];
}

QColor AppColorPalette::clipBackground(int index) const {
    return baseColor(index);
}

QColor AppColorPalette::clipBackgroundSelected(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), selectedLightnessDelta());
}

QColor AppColorPalette::clipBackgroundTransparent(int index) const {
    return ColorUtils::adjustAlpha(baseColor(index), 64);
}

QColor AppColorPalette::clipBorder(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), -0.1);
}

QColor AppColorPalette::clipForeground(int index) const {
    if (isLight())
        return lightestNeutral();
    auto lch = ColorUtils::srgbToOkLCH(baseColor(index));
    return lch.L > 0.7 ? QColor(0, 0, 0) : QColor(255, 255, 255);
}

QColor AppColorPalette::noteBackground(int index) const {
    return baseColor(index);
}

QColor AppColorPalette::noteBackgroundSelected(int index) const {
    return ColorUtils::adjustLightness(baseColor(index), selectedLightnessDelta());
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
    if (isLight())
        return lightestNeutral();
    auto lch = ColorUtils::srgbToOkLCH(baseColor(index));
    return lch.L > 0.7 ? QColor(0, 0, 0) : QColor(255, 255, 255);
}

QColor AppColorPalette::noteForegroundOverlapped(int index) const {
    auto fg = noteForeground(index);
    fg.setAlpha(127);
    return fg;
}

QColor AppColorPalette::noteBackgroundEditingPitch(int index) const {
    return m_noteEditFill.at(normalizedIndex(index));
}

QColor AppColorPalette::noteBorderEditingPitch(int index) const {
    return m_noteEditBorder.at(normalizedIndex(index));
}

QColor AppColorPalette::noteForegroundEditingPitch(int index) const {
    return m_noteEditForeground.at(normalizedIndex(index));
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
