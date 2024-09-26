//
// Created by OrangeCat on 24-9-3.
//

#ifndef INFERCONTROLLER_H
#define INFERCONTROLLER_H

#define inferController InferController::instance()

#include "Utils/Singleton.h"

#include <QObject>

class InferControllerPrivate;

class InferController final : public QObject, public Singleton<InferController> {
    Q_OBJECT

public:
    explicit InferController();

private:
    Q_DECLARE_PRIVATE(InferController)
    InferControllerPrivate *d_ptr;
};



#endif // INFERCONTROLLER_H
