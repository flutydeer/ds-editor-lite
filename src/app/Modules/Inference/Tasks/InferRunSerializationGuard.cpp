#include "InferTaskCommon.h"

#include "Model/AppOptions/AppOptions.h"

#include <mutex>

namespace {
    // Serializes ONNX inference Run() calls under the DirectML EP. DML's
    // DmlCommandRecorder is not thread-safe across sessions: concurrent Run()
    // from different inference tasks (e.g. two pieces' pipelines running in
    // parallel on the task thread pool) crashes in the graphics driver.
    // This lock is only acquired when the user selected DirectML, keeping the
    // CUDA parallel execution intact. See InferRunSerializationGuard in the
    // header.
    std::mutex g_inferRunSerializationMutex;
}

InferRunSerializationGuard::InferRunSerializationGuard() {
    if (appOptions->inference()->executionProvider == QStringLiteral("DirectML")) {
        g_inferRunSerializationMutex.lock();
        m_locked = true;
    }
}

InferRunSerializationGuard::~InferRunSerializationGuard() {
    if (m_locked)
        g_inferRunSerializationMutex.unlock();
}
