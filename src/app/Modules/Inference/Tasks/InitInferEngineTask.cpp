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
        // An initialization failure is still a hard failure, but the reason has to reach the user.
        // runTask() runs on a thread-pool thread while AppStatus is owned by the application
        // thread, so, in the same shape as InferEngine publishing unavailableExecutionProvider, a
        // queued invocation bounces the assignment back to the application thread, and the main
        // window then shows a one-shot notice.
        QMetaObject::invokeMethod(
            qApp, [message = errorMessage] { appStatus->inferenceEngineError = message; },
            Qt::QueuedConnection);
    } else {
        success.store(true, std::memory_order_release);
    }
}