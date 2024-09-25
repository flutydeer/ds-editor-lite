//
// Created by fluty on 24-9-25.
//

#ifndef INFERENCEENGINE_H
#define INFERENCEENGINE_H

#define inferEngine InferenceEngine::instance()

#include "Utils/Singleton.h"

#include <QMutex>
#include <QObject>

#include <dsonnxinfer/Environment.h>

using namespace dsonnxinfer;

class InferenceEngine final : public QObject, public Singleton<InferenceEngine> {
    Q_OBJECT

public:
    InferenceEngine();
    ~InferenceEngine() override;
    InferenceEngine(InferenceEngine const &) = delete;

    bool initialize(QString &error);
    bool initialized();
    bool loadConfig(const QString &path);
    QString config();

private:
    QMutex m_mutex;
    bool m_initialized = false;

    Environment m_env;
    QString m_configPath;
};



#endif // INFERENCEENGINE_H
