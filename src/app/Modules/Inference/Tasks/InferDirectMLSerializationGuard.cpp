#include "InferTaskCommon.h"

#include "Model/AppOptions/AppOptions.h"

#include <mutex>

namespace {
    // DirectML driver operations share one process-wide serialization boundary;
    // CUDA and CPU keep their existing parallel execution.
    std::mutex g_directMLSerializationMutex;
}

InferDirectMLSerializationGuard::InferDirectMLSerializationGuard() {
    if (appOptions->inference()->executionProvider == QStringLiteral("DirectML")) {
        g_directMLSerializationMutex.lock();
        m_locked = true;
    }
}

InferDirectMLSerializationGuard::~InferDirectMLSerializationGuard() {
    if (m_locked)
        g_directMLSerializationMutex.unlock();
}
