#ifndef DS_EDITOR_LITE_DSPXIMPORTLOADSESSION_H
#define DS_EDITOR_LITE_DSPXIMPORTLOADSESSION_H

#include "OpendspxImportLoadSession.h"

// DSPX import session: background parse (OpenDspxProjectTask), interactive
// track / timeline selection and Append materialization. Configuration and
// materialization are shared with LibreSVIP-bridged imports through
// OpendspxImportLoadSession; the only format-specific piece here is the
// native DSPX parse task and its result extraction.
class DspxImportLoadSession final : public OpendspxImportLoadSession {
    Q_OBJECT

public:
    DspxImportLoadSession(IProjectFormatHandler *formatHandler, QString filePath,
                          ProjectLoadPurpose purpose, quint64 requestId, bool interactive = true,
                          bool importTempo = true, bool importTimeSignature = true,
                          QObject *parent = nullptr);

private:
    Task *createParseTask() override;
    ParseOutcome takeParsedModel(Task *task) override;
};

#endif // DS_EDITOR_LITE_DSPXIMPORTLOADSESSION_H
