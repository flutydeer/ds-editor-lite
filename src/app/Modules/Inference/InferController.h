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
class InferVarianceTask;
class InferAcousticTask;
class Task;

class InferController final : public QObject {
    Q_OBJECT

public:
    LITE_SINGLETON_DECLARE_INSTANCE(InferController)
    Q_DISABLE_COPY_MOVE(InferController)

    void addInferDurationTask(InferDurationTask &task);
    void cancelInferDurationTask(int taskId);
    void finishCurrentInferDurationTask();

    void addInferPitchTask(InferPitchTask &task);
    void cancelInferPitchTask(int taskId);
    void finishCurrentInferPitchTask();

    void addInferVarianceTask(InferVarianceTask &task);
    void cancelInferVarianceTask(int taskId);
    void finishCurrentInferVarianceTask();

    void addInferAcousticTask(InferAcousticTask &task);
    void cancelInferAcousticTask(int taskId);
    void finishCurrentInferAcousticTask();

private:
    explicit InferController(QObject *parent = nullptr);
    ~InferController() override;

    Q_DECLARE_PRIVATE(InferController)
    InferControllerPrivate *d_ptr;
};



#endif // INFERCONTROLLER_H
