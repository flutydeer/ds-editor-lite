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
    bool finishCurrentInferDurationTask(InferDurationTask *task = nullptr);

    void addInferPitchTask(InferPitchTask &task);
    void cancelInferPitchTask(int taskId);
    bool finishCurrentInferPitchTask(InferPitchTask *task = nullptr);

    void addInferVarianceTask(InferVarianceTask &task);
    void cancelInferVarianceTask(int taskId);
    bool finishCurrentInferVarianceTask(InferVarianceTask *task = nullptr);

    void addInferAcousticTask(InferAcousticTask &task);
    void cancelInferAcousticTask(int taskId);
    bool finishCurrentInferAcousticTask(InferAcousticTask *task = nullptr);

private:
    explicit InferController(QObject *parent = nullptr);
    ~InferController() override;

    Q_DECLARE_PRIVATE(InferController)
    InferControllerPrivate *d_ptr;
};



#endif // INFERCONTROLLER_H
