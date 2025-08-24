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
    if (!inferEngine->initialize(errorMessage)) {
        success.store(false, std::memory_order_release);
        qCritical().noquote().nospace()
            << "Failed to initialize inference engine: " << errorMessage;
    } else {
        success.store(true, std::memory_order_release);
    }
}