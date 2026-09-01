#include "DspxFormatHandler.h"

#include "Controller/DocumentWorkflow/DspxImportLoadSession.h"
#include "Controller/DocumentWorkflow/DspxLoadSession.h"
#include "Modules/ProjectConverters/DspxConfigPage.h"

ProjectFormatDescriptor DspxFormatHandler::descriptor() const {
    ProjectFormatDescriptor result;
    result.id = QStringLiteral("dspx");
    result.displayName = QStringLiteral("DiffSinger Project");
    result.extensions = {QStringLiteral("dspx")};
    result.canOpen = true;
    result.canImport = true;
    return result;
}

bool DspxFormatHandler::probe(const QByteArray &header) const {
    // .dspx files are zip containers.
    return header.size() >= 4 && header.startsWith("PK\x03\x04");
}

IProjectLoadSession *DspxFormatHandler::createSession(const ProjectLoadRequest &request,
                                                      IDocumentWorkflowUi *ui, QObject *parent) {
    if (request.purpose == ProjectLoadPurpose::Open)
        return new DspxLoadSession(request.filePath, request.requestId, ui, !request.interactive,
                                   parent);
    return new DspxImportLoadSession(this, request.filePath, request.purpose, request.requestId,
                                     request.interactive, request.importTempo,
                                     request.importTimeSignature, parent);
}

IProjectConfigPage *DspxFormatHandler::createConfigPage(QWidget *parent) {
    return new DspxConfigPage({}, parent);
}
