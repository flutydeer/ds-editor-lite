#ifndef ICONLABEL_H
#define ICONLABEL_H

#include <lite/GUI/Utils/IconUtils.h>

#include <QLabel>
#include <QString>

/// Non-interactive icon display label.
/// The widget keeps a fixed square container size (`setSquareSize`, default 24)
/// while the pixmap is rendered at the SVG's intrinsic size and centered, so
/// 16px art is drawn at 16px and not upscaled to fill the container.
/// Color uses the theme-token action palette and re-tints on theme change.
class IconLabel : public QLabel {
    Q_OBJECT
public:
    explicit IconLabel(QWidget *parent = nullptr);

    /// Sets the SVG source; the pixmap is rendered at its intrinsic size.
    void setIcon(const QString &svgPath);

    /// Sets the container square size in logical pixels (default 24).
    void setSquareSize(int squareSize);

    /// Tints the icon with the given theme token name (e.g. "text.secondary").
    /// Takes precedence over setCustomPalette(); resolved live on theme change.
    void setColorToken(const QString &tokenName);

    /// Overrides the palette used for tinting (default: icon.primary token).
    void setCustomPalette(const IconUtils::SvgIconColorPalette &palette);

    /// Returns the SVG source path (empty if none set).
    QString iconPath() const;

private slots:
    void rebuildIcon();

private:
    QString m_svgPath;
    QString m_colorToken;
    int m_squareSize = 24;
    bool m_hasCustomPalette = false;
    IconUtils::SvgIconColorPalette m_customPalette;
};

#endif // ICONLABEL_H
