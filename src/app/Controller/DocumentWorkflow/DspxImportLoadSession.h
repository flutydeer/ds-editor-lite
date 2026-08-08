#ifndef DS_EDITOR_LITE_DSPXIMPORTLOADSESSION_H
#define DS_EDITOR_LITE_DSPXIMPORTLOADSESSION_H

#include "ProjectLoadSessionBase.h"

#include "Modules/ProjectFormats/UserInput.h"

#include <memory>

class IProjectFormatHandler;

namespace opendspx {
    struct Model;
}

// DSPX import session: background parse, interactive track / timeline
// selection and Append materialization. The source loop region is never
// imported (Import never touches loop): the base DspxProjectConverter is used
// so loop settings stay out of AppStatus. Singer references are carried
// through as identifiers, so no package readiness wait is needed.
class DspxImportLoadSession final : public ProjectLoadSessionBase {
    Q_OBJECT

public:
    DspxImportLoadSession(IProjectFormatHandler *formatHandler, QString filePath, quint64 requestId,
                          QObject *parent = nullptr);
    ~DspxImportLoadSession() override;

private:
    void onStart() override;
    Task *createParseTask() override;
    void handleParseResult(Task *task) override;
    bool shouldPublishProgress() const override;

    void startConfiguration();
    void materialize(const DspxUserInput &input);

    IProjectFormatHandler *m_formatHandler = nullptr;
    std::unique_ptr<opendspx::Model> m_model;
};

#endif // DS_EDITOR_LITE_DSPXIMPORTLOADSESSION_H
