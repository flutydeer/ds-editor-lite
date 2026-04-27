#ifndef TRACKCOLORPALETTE_H
#define TRACKCOLORPALETTE_H

#include <QColor>
#include <QList>

#include "Utils/Singleton.h"

class TrackColorPalette {
public:
    static constexpr int colorCount = 12;

    LITE_SINGLETON_DECLARE_INSTANCE(TrackColorPalette)

    bool load(const QString &jsonFilePath);

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

    QColor phonemeEdited(int index) const;
    QColor phonemeFill(int index) const;

    QColor paramFillTop(int index) const;
    QColor paramFillBottom(int index) const;
    QColor paramFillFlat(int index) const;
    QColor paramLine(int index) const;

    QColor keyHighlight(int index) const;

    QColor trackHeaderColor(int index) const;

    int nextColorIndex();

private:
    TrackColorPalette();
    ~TrackColorPalette() = default;
    Q_DISABLE_COPY_MOVE(TrackColorPalette)

    int normalizedIndex(int index) const;

    QList<QColor> m_palette;
    int m_nextIndex = 0;
};

#endif // TRACKCOLORPALETTE_H
