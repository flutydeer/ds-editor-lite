//
// Created by fluty on 2023/11/14.
//

#ifndef DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
#define DATASET_TOOLS_TRACKSGRAPHICSVIEW_H

#include "../Base/CommonGraphicsView.h"
#include "AbstractClipGraphicsItem.h"
#include "Model/AppModel.h"

class TracksGraphicsView final : public CommonGraphicsView {
    Q_OBJECT

public:
    explicit TracksGraphicsView();

// signals:
//     void resized(QSize size);

// private:
//     void resizeEvent(QResizeEvent *event) override;
};

#endif // DATASET_TOOLS_TRACKSGRAPHICSVIEW_H
