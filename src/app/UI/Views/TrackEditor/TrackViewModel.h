//
// Created by fluty on 2024/2/8.
//

#ifndef TRACKVIEWMODEL_H
#define TRACKVIEWMODEL_H

#include <QObject>
#include "Utils/UniqueObject.h"

class TrackControlView;

class TrackViewModel : public QObject, public UniqueObject {
    Q_OBJECT
public:
    explicit TrackViewModel(int id, QObject *parent = nullptr) : QObject(parent), UniqueObject(id) {
    }

    TrackControlView *controlView = nullptr;
    bool isSelected = false;
    QList<AbstractClipView *> clips;
};



#endif // TRACKVIEWMODEL_H
