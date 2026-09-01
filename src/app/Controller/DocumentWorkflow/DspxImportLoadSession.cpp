#include "DspxImportLoadSession.h"

#include "Controller/Tasks/OpenDspxProjectTask.h"

#include <utility>

DspxImportLoadSession::DspxImportLoadSession(IProjectFormatHandler *formatHandler, QString filePath,
                                             const ProjectLoadPurpose purpose,
                                             const quint64 requestId, const bool interactive,
                                             const bool importTempo, const bool importTimeSignature,
                                             QObject *parent)
    : OpendspxImportLoadSession(formatHandler, std::move(filePath), purpose, requestId, interactive,
                                importTempo, importTimeSignature, parent) {
}

Task *DspxImportLoadSession::createParseTask() {
    return new OpenDspxProjectTask(m_filePath, m_requestId);
}

OpendspxImportLoadSession::ParseOutcome DspxImportLoadSession::takeParsedModel(Task *task) {
    auto *parseTask = static_cast<OpenDspxProjectTask *>(task);
    auto parseResult = parseTask->takeResult();
    return {std::move(parseResult.model), parseResult.errorMessage};
}
