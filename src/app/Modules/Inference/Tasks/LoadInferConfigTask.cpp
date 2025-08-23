//
// Created by fluty on 24-9-25.
//

#if false
#include "LoadInferConfigTask.h"

#include "Modules/Inference/InferEngine.h"

#include <QDebug>

LoadInferConfigTask::LoadInferConfigTask(const QString &path) : m_path(path) {
    TaskStatus status;
    status.title = tr("Loading inference config...");
    status.message = m_path;
    status.isIndetermine = true;
    setStatus(status);
}

void LoadInferConfigTask::runTask() {
    qDebug() << "Loading inference config...";
    success = inferEngine->loadInferences(m_path);
}
#endif