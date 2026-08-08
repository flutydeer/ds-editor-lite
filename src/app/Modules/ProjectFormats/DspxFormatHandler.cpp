#include "DspxFormatHandler.h"

#include "Controller/DocumentWorkflow/DspxImportLoadSession.h"
#include "Modules/ProjectConverters/DspxConfigPage.h"

ProjectFormatDescriptor DspxFormatHandler::descriptor() const {
    ProjectFormatDescriptor result;
    result.id = QStringLiteral("dspx");
    result.displayName = QStringLiteral("DiffSinger Project");
    result.extensions = {QStringLiteral("dspx")};
    result.canImport = true;
    return result;
}

bool DspxFormatHandler::probe(const QByteArray &header) const {
    // .dspx files are zip containers.
    return header.size() >= 4 && header.startsWith("PK\x03\x04");
}

IProjectLoadSession *DspxFormatHandler::createSession(const ProjectLoadRequest &request,
                                                      QObject *parent) {
    // DSPX Open keeps its dedicated session until migration step 6.
    if (request.purpose != ProjectLoadPurpose::Import)
        return nullptr;
    return new DspxImportLoadSession(this, request.filePath, request.requestId, parent);
}

IProjectConfigPage *DspxFormatHandler::createConfigPage(QWidget *parent) {
    return new DspxConfigPage({}, parent);
}
