#ifndef DS_EDITOR_LITE_DSPXIMPORTLOADSESSION_H
#define DS_EDITOR_LITE_DSPXIMPORTLOADSESSION_H

#include "IProjectLoadSession.h"

#include <lite/Tasking/Task.h>

#include "Modules/ProjectFormats/UserInput.h"

#include <memory>

class IProjectFormatHandler;
class OpenDspxProjectTask;
class DspxConfigPage;
class ProjectImportConfigDialog;

namespace opendspx {
    struct Model;
}

// DSPX import session: background parse, interactive track / timeline
// selection and Append materialization. The source loop region is never
// imported (Import never touches loop): the base DspxProjectConverter is used
// so loop settings stay out of AppStatus. Singer references are carried
// through as identifiers, so no package readiness wait is needed.
class DspxImportLoadSession final : public IProjectLoadSession {
    Q_OBJECT

public:
    DspxImportLoadSession(IProjectFormatHandler *formatHandler, QString filePath, quint64 requestId,
                          QObject *parent = nullptr);
    ~DspxImportLoadSession() override;

    void start() override;
    void cancel() override;
    PreparedProject takeResult() override;
    [[nodiscard]] quint64 requestId() const override;

private:
    void startParseTask();
    void handleTaskFinished(OpenDspxProjectTask *task);
    void startConfiguration();
    void materialize(const DspxUserInput &input);
    void publishProgress(const TaskStatus &status);
    void detachTask();

    IProjectFormatHandler *m_formatHandler = nullptr;
    QString m_filePath;
    quint64 m_requestId = 0;
    OpenDspxProjectTask *m_task = nullptr;
    ProjectImportConfigDialog *m_dialog = nullptr;
    DspxConfigPage *m_configPage = nullptr;
    std::unique_ptr<opendspx::Model> m_model;
    PreparedProject m_result;
    bool m_started = false;
    bool m_terminal = false;
};

#endif // DS_EDITOR_LITE_DSPXIMPORTLOADSESSION_H
