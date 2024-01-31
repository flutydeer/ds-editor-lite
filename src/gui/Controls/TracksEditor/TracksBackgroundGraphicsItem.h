//
// Created by fluty on 2024/1/22.
//

#ifndef TRACKSBACKGROUNDGRAPHICSITEM_H
#define TRACKSBACKGROUNDGRAPHICSITEM_H

#include "../Base/TimeGridGraphicsItem.h"

class TracksBackgroundGraphicsItem final : public TimeGridGraphicsItem {
    Q_OBJECT

public slots:
    void onTrackCountChanged(int count);

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    int m_trackCount = 0;
};

#endif // TRACKSBACKGROUNDGRAPHICSITEM_H
