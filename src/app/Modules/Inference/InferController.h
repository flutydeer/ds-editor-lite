//
// Created by OrangeCat on 24-9-3.
//

#ifndef INFERCONTROLLER_H
#define INFERCONTROLLER_H

#define inferController InferController::instance()

#include "Utils/Singleton.h"

#include <QObject>

class InferControllerPrivate;
class InferDurationTask;
class InferPitchTask;
class Task;

class InferController final : public QObject {
    Q_OBJECT

public:
    LITE_SINGLETON_DECLARE_INSTANCE(InferController)
    Q_DISABLE_COPY_MOVE(InferController)

    void addInferDurationTask(InferDurationTask& task);
    void cancelInferDurationTask(int taskId);

    void addInferPitchTask(InferPitchTask& task);
    void cancelInferPitchTask(int taskId);

private:
    explicit InferController(QObject *parent = nullptr);
    ~InferController() override;

    Q_DECLARE_PRIVATE(InferController)
    InferControllerPrivate *d_ptr;
};



#endif // INFERCONTROLLER_H
