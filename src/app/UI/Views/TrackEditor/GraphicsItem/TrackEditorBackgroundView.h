//
// Created by fluty on 2024/1/22.
//

#ifndef TRACKEDITORBACKGROUNDVIEW_H
#define TRACKEDITORBACKGROUNDVIEW_H

#include "UI/Views/Common/TimeGridView.h"

class TrackEditorBackgroundView final : public TimeGridView {
    Q_OBJECT

public slots:
    void onTrackCountChanged(int count);
    void onTrackSelectionChanged(int trackIndex);

private:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget) override;

    int m_trackCount = 0;
    int m_trackIndex = -1;
};

#endif // TRACKEDITORBACKGROUNDVIEW_H
