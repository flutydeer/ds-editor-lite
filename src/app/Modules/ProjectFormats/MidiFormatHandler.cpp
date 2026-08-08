#include "MidiFormatHandler.h"

#include "Controller/DocumentWorkflow/MidiLoadSession.h"
#include "Modules/ProjectConverters/MidiConfigPage.h"

ProjectFormatDescriptor MidiFormatHandler::descriptor() const {
    ProjectFormatDescriptor result;
    result.id = QStringLiteral("midi");
    result.displayName = QStringLiteral("MIDI");
    result.extensions = {QStringLiteral("mid"), QStringLiteral("midi")};
    result.canOpen = true;
    result.canImport = true;
    return result;
}

bool MidiFormatHandler::probe(const QByteArray &header) const {
    return header.size() >= 4 && header.startsWith("MThd");
}

IProjectLoadSession *MidiFormatHandler::createSession(const ProjectLoadRequest &request,
                                                      QObject *parent) {
    return new MidiLoadSession(this, request.filePath, request.purpose, request.requestId, parent);
}

IProjectConfigPage *MidiFormatHandler::createConfigPage(QWidget *parent) {
    return new MidiConfigPage({}, parent);
}
