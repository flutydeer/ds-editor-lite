#include "DspxLoadSession.h"

#include "IDocumentWorkflowUi.h"
#include "Controller/Tasks/OpenDspxProjectTask.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"
#include "Model/AppStatus/AppStatus.h"
#include "Modules/ProjectConverters/DspxProjectConverterUi.h"

#include <QFileInfo>

#include <utility>

DspxLoadSession::DspxLoadSession(QString filePath, const quint64 requestId, IDocumentWorkflowUi *ui,
                                 const bool allowWithoutPackageMetadata, QObject *parent)
    : ProjectLoadSessionBase(std::move(filePath), requestId, parent), m_ui(ui),
      m_allowWithoutPackageMetadata(allowWithoutPackageMetadata) {
}

void DspxLoadSession::onStart() {
    switch (appStatus->packageModuleStatus.get()) {
        case AppStatus::ModuleStatus::Ready:
            startParseTask();
            break;
        case AppStatus::ModuleStatus::Error:
            if (m_allowWithoutPackageMetadata ||
                (m_ui && m_ui->confirmOpenWithoutPackageMetadata()))
                startParseTask();
            else
                emitCanceled();
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

void DspxLoadSession::onCancel() {
    if (m_packageConnection) {
        disconnect(m_packageConnection);
        m_packageConnection = {};
    }
}

Task *DspxLoadSession::createParseTask() {
    return new OpenDspxProjectTask(m_filePath, m_requestId);
}

bool DspxLoadSession::shouldPublishProgress() const {
    return true;
}

void DspxLoadSession::handlePackageStatus() {
    if (isTerminal())
        return;
    if (appStatus->packageModuleStatus == AppStatus::ModuleStatus::Ready) {
        startParseTask();
    } else if (appStatus->packageModuleStatus == AppStatus::ModuleStatus::Error) {
        if (m_allowWithoutPackageMetadata || (m_ui && m_ui->confirmOpenWithoutPackageMetadata()))
            startParseTask();
        else {
            emitCanceled();
        }
    }
}

void DspxLoadSession::handleParseResult(Task *task) {
    auto *parseTask = static_cast<OpenDspxProjectTask *>(task);
    auto parseResult = parseTask->takeResult();
    if (!parseResult.success()) {
        fail({tr("Failed to open project"), parseResult.errorMessage});
        return;
    }

    emit progressChanged({tr("Opening Project"), tr("Applying project..."), 0, 100, 0, true});
    AppModel resultModel;
    LoopSettings loopSettings;
    QString errorMessage;
    DspxProjectConverterUi converter;
    if (!converter.loadParsedProject(*parseResult.model, &resultModel, loopSettings, errorMessage,
                                     IProjectConverter::ImportMode::NewProject)) {
        fail({tr("Failed to open project"), errorMessage});
        return;
    }

    SingingClipPhonemeNormalizer::normalizeEditedOffsets(resultModel);
    ReplaceProjectPayload payload;
    payload.model = resultModel.takeProjectData();
    payload.loopSettings = loopSettings;
    payload.sourceKind = ProjectSourceKind::Native;
    payload.sourcePath = m_filePath;
    payload.displayName = QFileInfo(m_filePath).fileName();
    finishWithResult(std::move(payload));
}
