#ifndef LITE_ADAPTIVETOOLBARLAYOUT_P_H
#define LITE_ADAPTIVETOOLBARLAYOUT_P_H

#include <QList>
#include <QMap>
#include <QRect>
#include <QSize>

namespace AdaptiveToolBarLayoutDetails {

    struct Entry {
        QSize size;
        int group = 0;
        int order = 0;
        int priority = 0;
        bool applicable = true;
    };

    struct Plan {
        QList<int> inlineItems;
        QList<int> overflowItems;
        QList<QRect> itemRects;
        QMap<int, QRect> moreRects;
        QMap<int, QRect> groupRects;
        QSize fullSize;
    };

    // Priorities ascend from first removed to last removed. Presentation is independent.
    Plan calculate(const QList<Entry> &entries, int width, const QSize &moreSize, int itemSpacing,
                   int groupSpacing);
    Plan calculate(const QList<Entry> &entries, int width, const QMap<int, QSize> &moreSizes,
                   int itemSpacing, int groupSpacing);

}

#endif
