#ifndef DS_EDITOR_LITE_OPENDSPXIMPORTLOADSESSION_H
#define DS_EDITOR_LITE_OPENDSPXIMPORTLOADSESSION_H

#include "ProjectLoadSessionBase.h"

#include "Modules/ProjectFormats/UserInput.h"

#include <memory>

class IProjectFormatHandler;

namespace opendspx {
    struct Model;
}

// Import session for formats whose parse result is an opendspx::Model:
// native DSPX (OpenDspxProjectTask) and LibreSVIP-bridged formats (converted
// to DSPX data by an external process). Owns the shared interactive
// configuration (DspxConfigPage track / timeline selection) and the Append
// materialization (base DspxProjectConverter, Import never touches loop).
// Subclasses only provide the parse task result extraction.
class OpendspxImportLoadSession : public ProjectLoadSessionBase {
    Q_OBJECT

public:
    OpendspxImportLoadSession(IProjectFormatHandler *formatHandler, QString filePath,
                              quint64 requestId, QObject *parent = nullptr);
    ~OpendspxImportLoadSession() override;

protected:
    struct ParseOutcome {
        std::unique_ptr<opendspx::Model> model;
        QString errorMessage;
    };

    // Extracts the parsed model from the finished parse task; on failure the
    // errorMessage is non-empty and model is null.
    virtual ParseOutcome takeParsedModel(Task *task) = 0;

    void onStart() override;
    void handleParseResult(Task *task) override;
    bool shouldPublishProgress() const override;

    void startConfiguration();
    void materialize(const DspxUserInput &input);

    IProjectFormatHandler *m_formatHandler = nullptr;
    std::unique_ptr<opendspx::Model> m_model;
};

#endif // DS_EDITOR_LITE_OPENDSPXIMPORTLOADSESSION_H
