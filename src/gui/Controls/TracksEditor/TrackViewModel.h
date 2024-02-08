//
// Created by fluty on 2024/2/8.
//

#ifndef TRACKVIEWMODEL_H
#define TRACKVIEWMODEL_H

#include <QObject>

#include "AbstractClipGraphicsItem.h"
#include "TrackControlWidget.h"

class TrackViewModel : public QObject {
    Q_OBJECT
public:
    TrackControlWidget *widget;
    bool isSelected;
    QList<AbstractClipGraphicsItem *> clips;

    signals:

};



#endif //TRACKVIEWMODEL_H
