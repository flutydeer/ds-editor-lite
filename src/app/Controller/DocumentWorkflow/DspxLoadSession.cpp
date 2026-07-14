#include "DspxLoadSession.h"

#include "IDocumentWorkflowUi.h"
#include "Controller/Tasks/OpenDspxProjectTask.h"
#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/ProjectConverters/DspxProjectConverter.h"
#include "Modules/Task/TaskManager.h"

#include <QFileInfo>

#include <utility>

DspxLoadSession::DspxLoadSession(QString filePath, const quint64 requestId, IDocumentWorkflowUi *ui,
                                 QObject *parent)
    : IProjectLoadSession(parent), m_filePath(std::move(filePath)), m_requestId(requestId),
      m_ui(ui) {
}

DspxLoadSession::~DspxLoadSession() {
    if (m_packageConnection)
        disconnect(m_packageConnection);
    detachTask();
}

void DspxLoadSession::start() {
    if (m_started || m_terminal)
        return;
    m_started = true;

    switch (appStatus->packageModuleStatus.get()) {
        case AppStatus::ModuleStatus::Ready:
            startTask();
            break;
        case AppStatus::ModuleStatus::Error:
            if (m_ui && m_ui->confirmOpenWithoutPackageMetadata())
                startTask();
            else {
                m_terminal = true;
                emit canceled();
            }
            break;
        case AppStatus::ModuleStatus::Unknown:
        case AppStatus::ModuleStatus::Loading: {
            emit progressChanged(
                {tr("Opening Project"), tr("Scanning singer packages..."), 0, 100, 0, true});
            m_packageConnection =
                connect(appStatus, &AppStatus::moduleStatusChanged, this,
                        [this](const AppStatus::ModuleType module, const AppStatus::ModuleStatus) {
                            if (module == AppStatus::ModuleType::Package)
                                handlePackageStatus();
                        });
            handlePackageStatus();
            break;
        }
    }
}

void DspxLoadSession::cancel() {
    if (m_terminal)
        return;
    m_terminal = true;
    if (m_packageConnection) {
        disconnect(m_packageConnection);
        m_packageConnection = {};
    }
    detachTask();
    emit canceled();
}

PreparedProject DspxLoadSession::takeResult() {
    return std::move(m_result);
}

quint64 DspxLoadSession::requestId() const {
    return m_requestId;
}

void DspxLoadSession::startTask() {
    if (m_terminal || m_task)
        return;
    if (m_packageConnection) {
        disconnect(m_packageConnection);
        m_packageConnection = {};
    }

    const auto task = new OpenDspxProjectTask(m_filePath, m_requestId);
    m_task = task;
    publishProgress(task->status());
    connect(task, &Task::statusUpdated, this, [this, task](const TaskStatus &status) {
        if (task == m_task && !m_terminal)
            publishProgress(status);
    });
    connect(task, &Task::finished, this, [this, task] { handleTaskFinished(task); });
    connect(task, &Task::finished, task, &QObject::deleteLater);
    taskManager->addAndStartTask(task);
}

void DspxLoadSession::handlePackageStatus() {
    if (m_terminal)
        return;
    if (appStatus->packageModuleStatus == AppStatus::ModuleStatus::Ready) {
        startTask();
    } else if (appStatus->packageModuleStatus == AppStatus::ModuleStatus::Error) {
        if (m_ui && m_ui->confirmOpenWithoutPackageMetadata())
            startTask();
        else {
            m_terminal = true;
            emit canceled();
        }
    }
}

void DspxLoadSession::handleTaskFinished(OpenDspxProjectTask *task) {
    if (taskManager->tasks().contains(task))
        taskManager->removeTask(task);
    if (task != m_task || m_terminal)
        return;
    m_task = nullptr;

    if (task->terminated()) {
        m_terminal = true;
        emit canceled();
        return;
    }

    auto parseResult = task->takeResult();
    if (!parseResult.success()) {
        m_terminal = true;
        emit failed({tr("Failed to open project"), parseResult.errorMessage});
        return;
    }

    emit progressChanged({tr("Opening Project"), tr("Applying project..."), 0, 100, 0, true});
    AppModel resultModel;
    LoopSettings loopSettings;
    QString errorMessage;
    DspxProjectConverter converter;
    if (!converter.loadParsedProject(*parseResult.model, &resultModel, loopSettings, errorMessage,
                                     IProjectConverter::ImportMode::NewProject)) {
        m_terminal = true;
        emit failed({tr("Failed to open project"), errorMessage});
        return;
    }

    SingingClipPhonemeNormalizer::normalizeEditedOffsets(resultModel);
    ReplaceProjectPayload payload;
    payload.model = resultModel.takeProjectData();
    payload.loopSettings = loopSettings;
    payload.sourceKind = ProjectSourceKind::Native;
    payload.sourcePath = m_filePath;
    payload.displayName = QFileInfo(m_filePath).fileName();
    m_result = std::move(payload);
    m_terminal = true;
    emit ready();
}

void DspxLoadSession::publishProgress(const TaskStatus &status) {
    emit progressChanged({status.title, status.message, status.minimum, status.maximum,
                          status.progress, status.isIndetermine});
}

void DspxLoadSession::detachTask() {
    if (!m_task)
        return;
    const auto task = m_task;
    m_task = nullptr;
    taskManager->terminateTask(task);
    if (taskManager->tasks().contains(task))
        taskManager->removeTask(task);
}
