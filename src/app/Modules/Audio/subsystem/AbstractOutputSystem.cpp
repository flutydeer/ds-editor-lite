#include "AbstractOutputSystem.h"

#include <TalcsDevice/AudioDevice.h>
#include <TalcsDevice/OutputContext.h>

AbstractOutputSystem::AbstractOutputSystem(QObject *parent) : QObject(parent) {
}

AbstractOutputSystem::~AbstractOutputSystem() = default;

void AbstractOutputSystem::setFileBufferingReadAheadSize(const qint64 size) {
    if (m_fileBufferingReadAheadSize != size) {
        m_fileBufferingReadAheadSize = size;
        emit fileBufferingReadAheadSizeChanged(size);
    }
}

qint64 AbstractOutputSystem::fileBufferingReadAheadSize() const {
    return m_fileBufferingReadAheadSize;
}

bool AbstractOutputSystem::isReady() const {
    return m_context->device() && m_context->device()->isOpen();
}

talcs::AbstractOutputContext *AbstractOutputSystem::context() const {
    return m_context;
}

void AbstractOutputSystem::setContext(talcs::AbstractOutputContext *context) {
    m_context = context;
}