#ifndef DSPXLOADSESSION_H
#define DSPXLOADSESSION_H

#include "IProjectLoadSession.h"

#include <QMetaObject>

class IDocumentWorkflowUi;
class OpenDspxProjectTask;
class TaskStatus;

class DspxLoadSession final : public IProjectLoadSession {
    Q_OBJECT

public:
    DspxLoadSession(QString filePath, quint64 requestId, IDocumentWorkflowUi *ui,
                    QObject *parent = nullptr);
    ~DspxLoadSession() override;

    void start() override;
    void cancel() override;
    PreparedProject takeResult() override;
    [[nodiscard]] quint64 requestId() const override;

private:
    void startTask();
    void handlePackageStatus();
    void handleTaskFinished(OpenDspxProjectTask *task);
    void publishProgress(const TaskStatus &status);
    void detachTask();

    QString m_filePath;
    quint64 m_requestId = 0;
    IDocumentWorkflowUi *m_ui = nullptr;
    OpenDspxProjectTask *m_task = nullptr;
    QMetaObject::Connection m_packageConnection;
    PreparedProject m_result;
    bool m_started = false;
    bool m_terminal = false;
};

#endif // DSPXLOADSESSION_H
