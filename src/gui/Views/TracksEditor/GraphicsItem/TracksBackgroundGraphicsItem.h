//
// Created by fluty on 2024/1/22.
//

#ifndef TRACKSBACKGROUNDGRAPHICSITEM_H
#define TRACKSBACKGROUNDGRAPHICSITEM_H

#include "Views/Common/TimeGridGraphicsItem.h"

class TracksBackgroundGraphicsItem final : public TimeGridGraphicsItem {
    Q_OBJECT

public slots:
    void onTrackCountChanged(int count);
    void onTrackSelectionChanged(int trackIndex);

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    int m_trackCount = 0;
    int m_trackIndex = -1;
};

#endif // TRACKSBACKGROUNDGRAPHICSITEM_H
