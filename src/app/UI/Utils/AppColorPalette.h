#ifndef APPCOLORPALETTE_H
#define APPCOLORPALETTE_H

#include <QColor>
#include <QList>

#include "Global/AppGlobal.h"
#include "Utils/Singleton.h"

class AppColorPalette {
public:
    static constexpr int colorCount = AppGlobal::paletteColorCount;

    LITE_SINGLETON_DECLARE_INSTANCE(AppColorPalette)

    bool load(const QString &jsonFilePath);

    /// Set a pre-validated palette. Returns false if the size is wrong.
    bool setColors(const QList<QColor> &colors);

    QColor baseColor(int index) const;

    QColor clipBackground(int index) const;
    QColor clipBackgroundSelected(int index) const;
    QColor clipBackgroundTransparent(int index) const;
    QColor clipBorder(int index) const;
    QColor clipForeground(int index) const;

    QColor noteBackground(int index) const;
    QColor noteBackgroundSelected(int index) const;
    QColor noteBackgroundOverlapped(int index) const;
    QColor noteBorder(int index) const;
    QColor noteBorderOverlapped(int index) const;
    QColor noteForeground(int index) const;
    QColor noteForegroundOverlapped(int index) const;

    QColor noteBackgroundEditingPitch(int index) const;
    QColor noteBorderEditingPitch(int index) const;
    QColor noteForegroundEditingPitch(int index) const;

    QColor phonemeEdited(int index) const;
    QColor phonemeFill(int index) const;

    QColor paramFillTop(int index) const;
    QColor paramFillBottom(int index) const;
    QColor paramFillFlat(int index) const;
    QColor paramLine(int index) const;
    QColor speakerMixParamFill(int index) const;
    QColor speakerMixDotFill(int index) const;

    QColor keyHighlight(int index) const;

    QColor trackHeaderColor(int index) const;

private:
    AppColorPalette();
    ~AppColorPalette() = default;
    Q_DISABLE_COPY_MOVE(AppColorPalette)

    int normalizedIndex(int index) const;

    QList<QColor> m_palette;
};

#endif // APPCOLORPALETTE_H
