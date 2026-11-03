#include "InitInferEngineTask.h"

#include "Modules/Inference/InferLogging.h"

#include "Modules/Inference/InferEngine.h"

#include "Model/AppStatus/AppStatus.h"

#include <QCoreApplication>
#include <QDebug>

InitInferEngineTask::InitInferEngineTask(QObject *parent) : Task(parent) {
    TaskStatus status;
    status.title = tr("Initialize inference engine");
    status.message = "";
    status.isIndetermine = true;
    setStatus(status);
}

void InitInferEngineTask::runTask() {
    qCDebug(logInferTask) << "Initialize inference engine...";
    if (!inferEngine->initialize(errorMessage)) {
        success.store(false, std::memory_order_release);
        qCritical().noquote().nospace()
            << "Failed to initialize inference engine: " << errorMessage;
        // Publish initialization failure on AppStatus's application thread.
        QMetaObject::invokeMethod(
            qApp, [message = errorMessage] { appStatus->inferenceEngineError = message; },
            Qt::QueuedConnection);
    } else {
        success.store(true, std::memory_order_release);
    }
}
