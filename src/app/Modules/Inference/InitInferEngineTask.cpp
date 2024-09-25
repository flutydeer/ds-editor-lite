//
// Created by fluty on 24-9-25.
//

#include "InitInferEngineTask.h"

#include "InferenceEngine.h"

#include <QDebug>
// #include <QThread>

InitInferEngineTask::InitInferEngineTask(QObject *parent) : Task(parent) {
    TaskStatus status;
    status.title = tr("Launching inference engine...");
    status.message = "";
    status.isIndetermine = true;
    setStatus(status);
}

void InitInferEngineTask::runTask() {
    qDebug() << "Launching inference engine...";
    // QThread::sleep(5);
    success = inferEngine->initialize(errorMessage);
}