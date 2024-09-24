//
// Created by fluty on 24-9-25.
//

#ifndef INFERENCEENGINE_H
#define INFERENCEENGINE_H

#include "Utils/Singleton.h"

#include <QMutex>
#include <QObject>

#include <dsonnxinfer/Environment.h>

class InferenceEngine : public QObject, public Singleton<InferenceEngine> {
    Q_OBJECT
public:
    InferenceEngine();
    bool initialize();
    bool initialized();
    bool loadConfig(const QString &path);
    QString config();

private:
    QMutex m_mutex;
    bool m_initialized = false;

    dsonnxinfer::Environment m_env;
    QString m_configPath;
};



#endif // INFERENCEENGINE_H
