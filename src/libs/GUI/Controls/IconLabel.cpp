#include <lite/GUI/Controls/IconLabel.h>

#include <lite/GUI/Theme/ThemeManager.h>

#include <QSvgRenderer>

IconLabel::IconLabel(QWidget *parent) : QLabel(parent) {
    setAlignment(Qt::AlignCenter);
    setFixedSize(m_squareSize, m_squareSize);
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, &IconLabel::rebuildIcon);
}

void IconLabel::setIcon(const QString &svgPath) {
    m_svgPath = svgPath;
    rebuildIcon();
}

void IconLabel::setSquareSize(const int squareSize) {
    if (m_squareSize == squareSize)
        return;
    m_squareSize = squareSize;
    setFixedSize(m_squareSize, m_squareSize);
    if (!m_svgPath.isEmpty())
        rebuildIcon();
}

void IconLabel::setColorToken(const QString &tokenName) {
    if (m_colorToken == tokenName)
        return;
    m_colorToken = tokenName;
    m_hasCustomPalette = false;
    if (!m_svgPath.isEmpty())
        rebuildIcon();
}

void IconLabel::setCustomPalette(const IconUtils::SvgIconColorPalette &palette) {
    m_hasCustomPalette = true;
    m_customPalette = palette;
    m_colorToken.clear();
    if (!m_svgPath.isEmpty())
        rebuildIcon();
}

QString IconLabel::iconPath() const {
    return m_svgPath;
}

void IconLabel::rebuildIcon() {
    if (m_svgPath.isEmpty()) {
        clear();
        return;
    }

    const QSize iconSize = QSvgRenderer(m_svgPath).defaultSize();
    if (iconSize.isEmpty()) {
        clear();
        return;
    }

    IconUtils::SvgIconColorPalette palette;
    if (!m_colorToken.isEmpty()) {
        const auto color = ThemeManager::instance()->semanticColor(m_colorToken);
        palette.normal = color.isValid() ? color : IconUtils::defaultActionPalette().normal;
    } else {
        palette = m_hasCustomPalette ? m_customPalette : IconUtils::defaultActionPalette();
    }
    setPixmap(IconUtils::createTintedSvgIcon(m_svgPath, iconSize, palette).pixmap(iconSize));
}
