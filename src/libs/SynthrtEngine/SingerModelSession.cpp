#include "SingerModelSession.h"

#include <utility>

#include <QDebug>

SingerModelSession::SingerModelSession(SingerIdentifier identifier, ds::infer::ModelSet modelSet)
    : m_identifier(std::move(identifier)), m_modelSet(std::move(modelSet)) {
}

SingerModelSession::~SingerModelSession() {
    std::lock_guard lock(m_modelSetMutex);
    if (auto result = m_modelSet.unloadAll(); !result) {
        qWarning() << "SingerModelSession: ModelSet::unloadAll failed for" << m_identifier << ":"
                   << QString::fromUtf8(result.error().message());
    }
}

const SingerIdentifier &SingerModelSession::identifier() const noexcept {
    return m_identifier;
}

srt::core::Expected<SingerModelSession::Model>
    SingerModelSession::acquire(ds::infer::StageKind kind) {
    std::lock_guard lock(m_modelSetMutex);
    if (auto result = m_modelSet.load(kind); !result) {
        return result.takeError();
    }

    Model model;
    model.inference = m_modelSet.model(kind);
    if (const auto *stage = m_modelSet.stages().find(kind); stage) {
        model.importOptions = stage->options;
    }
    return model;
}
