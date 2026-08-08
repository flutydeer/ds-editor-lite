#include "DspxImportLoadSession.h"

#include "Controller/Tasks/OpenDspxProjectTask.h"
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"
#include "Modules/ProjectConverters/DspxConfigPage.h"
#include "Modules/ProjectFormats/IProjectFormatHandler.h"
#include "Modules/ProjectFormats/ProjectImportConfigDialog.h"
#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/Tasking/TaskManager.h>

#include <opendspx/clip.h>
#include <opendspx/model.h>
#include <opendspx/singingclip.h>
#include <opendspx/track.h>

#include "UI/Dialogs/Base/Dialog.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <algorithm>
#include <utility>

namespace {
    // UI-facing track summary extracted from the parsed DSPX model. rangeText
    // carries the track kind (Singing / Audio / Mixed) for the selector.
    QList<MidiImportTrackInfo> buildDspxTrackInfos(const opendspx::Model &model) {
        QList<MidiImportTrackInfo> infos;
        infos.reserve(static_cast<int>(model.content.tracks.size()));
        for (const auto &track : model.content.tracks) {
            MidiImportTrackInfo info;
            info.name = QByteArray::fromStdString(track.name);
            int noteCount = 0;
            bool hasSinging = false;
            bool hasAudio = false;
            for (const auto &clip : track.clips) {
                if (!clip)
                    continue;
                if (clip->type == opendspx::Clip::Type::Singing) {
                    hasSinging = true;
                    const auto singing = std::static_pointer_cast<opendspx::SingingClip>(clip);
                    noteCount += static_cast<int>(singing->notes.size());
                    if (!singing->notes.empty())
                        hasAudio = false; // singing content dominates the kind label
                } else {
                    hasAudio = true;
                }
            }
            if (hasSinging && hasAudio)
                info.rangeText = DspxImportLoadSession::tr("Mixed");
            else if (hasSinging)
                info.rangeText = DspxImportLoadSession::tr("Singing");
            else if (hasAudio)
                info.rangeText = DspxImportLoadSession::tr("Audio");
            else
                info.rangeText = DspxImportLoadSession::tr("Empty");
            info.noteCount = noteCount;
            info.selectedByDefault = hasSinging || hasAudio;
            infos.append(info);
        }
        return infos;
    }
}

DspxImportLoadSession::DspxImportLoadSession(IProjectFormatHandler *formatHandler, QString filePath,
                                             const quint64 requestId, QObject *parent)
    : IProjectLoadSession(parent), m_formatHandler(formatHandler), m_filePath(std::move(filePath)),
      m_requestId(requestId) {
}

DspxImportLoadSession::~DspxImportLoadSession() {
    detachTask();
}

void DspxImportLoadSession::start() {
    if (m_started || m_terminal)
        return;
    m_started = true;
    startParseTask();
}

void DspxImportLoadSession::cancel() {
    if (m_terminal)
        return;
    m_terminal = true;
    detachTask();
    emit canceled();
}

PreparedProject DspxImportLoadSession::takeResult() {
    return std::move(m_result);
}

quint64 DspxImportLoadSession::requestId() const {
    return m_requestId;
}

void DspxImportLoadSession::startParseTask() {
    if (m_terminal || m_task)
        return;
    const auto task = new OpenDspxProjectTask(m_filePath, m_requestId);
    m_task = task;
    connect(task, &Task::statusUpdated, this, [this, task](const TaskStatus &status) {
        if (task == m_task && !m_terminal)
            publishProgress(status);
    });
    connect(task, &Task::finished, this, [this, task] { handleTaskFinished(task); });
    connect(task, &Task::finished, task, &QObject::deleteLater);
    taskManager->addAndStartTask(task);
}

void DspxImportLoadSession::handleTaskFinished(OpenDspxProjectTask *task) {
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
        emit failed({tr("Failed to import project"), parseResult.errorMessage});
        return;
    }

    m_model = std::move(parseResult.model);
    startConfiguration();
}

void DspxImportLoadSession::startConfiguration() {
    auto *dialog = new ProjectImportConfigDialog(Dialog::globalParent());
    m_dialog = dialog;
    dialog->setWindowTitle(tr("Configure DSPX Import"));
    auto *page = m_formatHandler->createConfigPage(dialog);
    auto *dspxPage = qobject_cast<DspxConfigPage *>(page ? page->widget() : nullptr);
    m_configPage = dspxPage;
    dialog->setPage(dspxPage);
    dspxPage->setTrackInfoList(buildDspxTrackInfos(*m_model));
    // Tempo / time signature default to enabled: imports usually merge
    // parts of the same song, where adopting the source timeline is
    // harmless (and desired on a fresh project). Loop stays untouched.
    dspxPage->setImportTempo(true);
    dspxPage->setImportTimeSignature(true);
    dialog->resize(720, 480);

    const auto accepted = dialog->exec() == QDialog::Accepted;
    DspxUserInput input;
    if (accepted && dspxPage)
        input = dspxPage->collectInput();
    m_configPage = nullptr;
    m_dialog = nullptr;
    dialog->deleteLater();
    if (m_terminal)
        return;
    if (!accepted) {
        m_terminal = true;
        emit canceled();
        return;
    }

    materialize(input);
}

void DspxImportLoadSession::materialize(const DspxUserInput &input) {
    // Copy the parsed model and drop unselected tracks before conversion.
    // Track values are cheap to copy (clip refs are shared pointers).
    auto filteredModel = std::make_unique<opendspx::Model>(*m_model);
    auto &tracks = filteredModel->content.tracks;
    const auto &selected = input.tracks.selectedTrackIndices;
    std::vector<opendspx::Track> kept;
    kept.reserve(selected.size());
    for (const auto index : selected) {
        if (index >= 0 && static_cast<qsizetype>(index) < static_cast<qsizetype>(tracks.size()))
            kept.push_back(std::move(tracks[static_cast<qsizetype>(index)]));
    }
    tracks = std::move(kept);
    if (tracks.empty()) {
        m_terminal = true;
        emit failed({tr("Failed to import project"),
                     QCoreApplication::translate("DspxImportLoadSession",
                                                 "No tracks were selected for import.")});
        return;
    }

    emit progressChanged({tr("Importing Project"), tr("Applying project..."), 0, 100, 0, true});
    AppModel resultModel;
    LoopSettings loopSettings;
    QString errorMessage;
    // Base converter on purpose: the imported loop region must not reach
    // AppStatus (Import never touches loop).
    DspxProjectConverter converter;
    if (!converter.loadParsedProject(*filteredModel, &resultModel, loopSettings, errorMessage,
                                     IProjectConverter::ImportMode::AppendToProject)) {
        m_terminal = true;
        emit failed({tr("Failed to import project"), errorMessage});
        return;
    }

    SingingClipPhonemeNormalizer::normalizeEditedOffsets(resultModel);
    AppendProjectPayload payload;
    payload.model = resultModel.takeProjectData();
    payload.importTempo = input.timeline.importTempo;
    payload.importTimeSignature = input.timeline.importTimeSignature;
    payload.sourcePath = m_filePath;
    m_result = std::move(payload);
    m_terminal = true;
    emit ready();
}

void DspxImportLoadSession::publishProgress(const TaskStatus &status) {
    emit progressChanged({status.title, status.message, status.minimum, status.maximum,
                          status.progress, status.isIndetermine});
}

void DspxImportLoadSession::detachTask() {
    if (!m_task)
        return;
    const auto task = m_task;
    m_task = nullptr;
    taskManager->terminateTask(task);
    if (taskManager->tasks().contains(task))
        taskManager->removeTask(task);
}
