#include "InferTaskCommon.h"

#include "Modules/Inference/ExecutionProvider.h"

#include <mutex>

namespace {
    // DirectML driver operations share one process-wide serialization boundary;
    // CUDA and CPU keep their existing parallel execution.
    std::mutex g_directMLSerializationMutex;
}

InferDirectMLSerializationGuard::InferDirectMLSerializationGuard() {
    // Serialize on the provider the engine actually runs, not the one the
    // settings file asked for: after a fallback to CPU no DirectML driver call
    // can happen, so locking would only serialize work that is already safe.
    if (ExecutionProviderUtils::effective() == ExecutionProvider::DirectML) {
        g_directMLSerializationMutex.lock();
        m_locked = true;
    }
}

InferDirectMLSerializationGuard::~InferDirectMLSerializationGuard() {
    if (m_locked)
        g_directMLSerializationMutex.unlock();
}
