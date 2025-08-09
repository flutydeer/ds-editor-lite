//
// Created by fluty on 24-9-25.
//

#include "InitInferEngineTask.h"

#include "Modules/Inference/InferEngine.h"

#include <QDebug>

InitInferEngineTask::InitInferEngineTask(QObject *parent) : Task(parent) {
    TaskStatus status;
    status.title = tr("Initialize inference engine");
    status.message = "";
    status.isIndetermine = true;
    setStatus(status);
}

void InitInferEngineTask::runTask() {
    qDebug() << "Initialize inference engine...";
    success = inferEngine->initialize(errorMessage);
    if (!success) {
        qCritical().noquote().nospace()
            << "Failed to initialize inference engine: " << errorMessage;
    }
}