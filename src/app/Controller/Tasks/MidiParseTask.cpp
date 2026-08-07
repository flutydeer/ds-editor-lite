#include "MidiParseTask.h"

#include <utility>

MidiParseTask::MidiParseTask(QString filePath, const quint64 requestId)
    : m_filePath(std::move(filePath)), m_requestId(requestId) {
}

quint64 MidiParseTask::requestId() const {
    return m_requestId;
}

MidiParseData MidiParseTask::takeResult() {
    return std::move(m_result);
}

void MidiParseTask::runTask() {
    if (isTerminateRequested())
        return;
    m_result = MidiFileParser::parse(m_filePath);
    if (isTerminateRequested())
        m_result = {};
}
