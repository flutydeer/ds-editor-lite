#ifndef DS_EDITOR_LITE_LIBRESVIPLOADSESSION_H
#define DS_EDITOR_LITE_LIBRESVIPLOADSESSION_H

#include "OpendspxImportLoadSession.h"

// Import session for LibreSVIP-bridged formats (SVP, USTX, VSQX, ...):
// the parse task runs the external libresvip-cli converter, whose output is
// DSPX data deserialized into an opendspx::Model. Configuration (track
// selection) and materialization are inherited from OpendspxImportLoadSession
// - the bridged model is the same DSPX model a native import would produce.
class LibreSVIPLoadSession final : public OpendspxImportLoadSession {
    Q_OBJECT

public:
    LibreSVIPLoadSession(IProjectFormatHandler *formatHandler, QString filePath,
                         ProjectLoadPurpose purpose, quint64 requestId, bool interactive = true,
                         bool importTempo = true, bool importTimeSignature = true,
                         QObject *parent = nullptr);

private:
    Task *createParseTask() override;
    ParseOutcome takeParsedModel(Task *task) override;
};

#endif // DS_EDITOR_LITE_LIBRESVIPLOADSESSION_H
