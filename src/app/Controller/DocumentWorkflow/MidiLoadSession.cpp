#include "MidiLoadSession.h"

#include "Controller/Tasks/MidiParseTask.h"
#include "Controller/Tasks/MidiReprocessTask.h"
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"
#include "Modules/ProjectConverters/MidiConfigPage.h"
#include "Modules/ProjectFormats/IProjectFormatHandler.h"
#include "Modules/ProjectFormats/ProjectImportConfigDialog.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/LoopSettings.h>
#include <lite/Tasking/TaskManager.h>
#include "UI/Dialogs/Base/Dialog.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <utility>

MidiLoadSession::MidiLoadSession(IProjectFormatHandler *formatHandler, QString filePath,
                                 const ProjectLoadPurpose purpose, const quint64 requestId,
                                 QObject *parent)
    : IProjectLoadSession(parent), m_formatHandler(formatHandler), m_filePath(std::move(filePath)),
      m_purpose(purpose), m_requestId(requestId) {
}

MidiLoadSession::~MidiLoadSession() {
    detachTask();
    detachReprocessTask();
}

void MidiLoadSession::start() {
    if (m_started || m_terminal)
        return;
    m_started = true;
    startParseTask();
}

void MidiLoadSession::cancel() {
    if (m_terminal)
        return;
    m_terminal = true;
    detachTask();
    detachReprocessTask();
    emit canceled();
}

PreparedProject MidiLoadSession::takeResult() {
    return std::move(m_result);
}

quint64 MidiLoadSession::requestId() const {
    return m_requestId;
}

void MidiLoadSession::startParseTask() {
    if (m_terminal || m_task)
        return;
    const auto task = new MidiParseTask(m_filePath, m_requestId);
    m_task = task;
    connect(task, &Task::finished, this, [this, task] { handleTaskFinished(task); });
    connect(task, &Task::finished, task, &QObject::deleteLater);
    taskManager->addAndStartTask(task);
}

void MidiLoadSession::handleTaskFinished(MidiParseTask *task) {
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

    m_parseData = task->takeResult();
    if (!m_parseData.valid) {
        m_terminal = true;
        emit failed({tr("Failed to load MIDI"), m_parseData.errorMessage});
        return;
    }

    startConfiguration();
}

void MidiLoadSession::startConfiguration() {
    auto *dialog = new ProjectImportConfigDialog(Dialog::globalParent());
    m_dialog = dialog;
    dialog->setWindowTitle(tr("Configure MIDI Import"));
    auto *page = m_formatHandler->createConfigPage(dialog);
    auto *midiPage = qobject_cast<MidiConfigPage *>(page ? page->widget() : nullptr);
    m_configPage = midiPage;
    dialog->setPage(midiPage);
    midiPage->setTrackInfoList(m_parseData.trackInfos);
    const auto defaultImportTimeline = m_purpose == ProjectLoadPurpose::Open;
    midiPage->setImportTempo(defaultImportTimeline);
    midiPage->setImportTimeSignature(defaultImportTimeline);
    midiPage->detectCodec();
    connect(midiPage, &MidiConfigPage::separateMidiChannelsChanged, this,
            [this](const bool enabled) { requestReprocess(enabled); });
    dialog->resize(720, 480);

    const auto accepted = dialog->exec() == QDialog::Accepted;
    MidiUserInput input;
    if (accepted && midiPage)
        input = midiPage->collectInput();
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

void MidiLoadSession::requestReprocess(const bool separateChannels) {
    if (m_terminal)
        return;
    detachReprocessTask();
    const auto generation = ++m_reprocessGeneration;
    const auto task = new MidiReprocessTask(m_parseData.rawData, separateChannels);
    m_reprocessTask = task;
    connect(task, &Task::finished, this,
            [this, generation, task] { handleReprocessFinished(generation, task); });
    connect(task, &Task::finished, task, &QObject::deleteLater);
    taskManager->addAndStartTask(task);
}

void MidiLoadSession::handleReprocessFinished(const quint64 generation, MidiReprocessTask *task) {
    if (taskManager->tasks().contains(task))
        taskManager->removeTask(task);
    if (task != m_reprocessTask || generation != m_reprocessGeneration || m_terminal)
        return;
    m_reprocessTask = nullptr;
    if (task->terminated())
        return;

    auto result = task->takeResult();
    if (result.errorMessage.isEmpty()) {
        m_parseData.mediate = std::move(result.mediate);
        if (m_configPage)
            m_configPage->setTrackInfoList(result.trackInfos);
        if (m_configPage)
            m_configPage->detectCodec();
    }
    // On re-parse errors keep the previous track list; the user can still
    // proceed with the last valid layout.
}

void MidiLoadSession::materialize(const MidiUserInput &input) {
    AppModel resultModel;
    const auto language = m_converterUi.importLanguage();
    MidiImportOptions choice;
    choice.codec = input.encoding.codec;
    choice.selectedTrackIndices = input.tracks.selectedTrackIndices;
    choice.importTempo = input.timeline.importTempo;
    choice.importTimeSignature = input.timeline.importTimeSignature;
    auto generated = MidiTrackGenerator::generateTracks(m_parseData, choice, language,
                                                        m_converterUi.defaultLyric(language),
                                                        resultModel.timeline());
    if (!generated.errorMessage.isEmpty()) {
        m_terminal = true;
        emit failed({tr("Failed to load MIDI"), generated.errorMessage});
        return;
    }

    // Apply the imported timeline first so generated audio clips anchor their
    // realtime truth under the final tempo map.
    if (generated.hasTimeline) {
        auto newTimeline = resultModel.timeline();
        if (!generated.timeSignatures.isEmpty())
            newTimeline.setTimeSignatures(generated.timeSignatures);
        if (!generated.tempos.isEmpty())
            newTimeline.setTempos(generated.tempos);
        if (newTimeline != resultModel.timeline())
            resultModel.setTimeline(std::move(newTimeline));
    }

    if (generated.tracks.isEmpty()) {
        m_terminal = true;
        emit failed({tr("Failed to load MIDI"),
                     QCoreApplication::translate("MidiConverter",
                                                 "No MIDI tracks were selected for import.")});
        return;
    }
    int count = 0;
    for (const auto track : generated.tracks) {
        resultModel.insertTrack(track, count);
        count++;
    }

    SingingClipPhonemeNormalizer::normalizeEditedOffsets(resultModel);

    if (m_purpose == ProjectLoadPurpose::Open) {
        ReplaceProjectPayload payload;
        payload.model = resultModel.takeProjectData();
        payload.loopSettings = LoopSettings();
        payload.sourceKind = ProjectSourceKind::Foreign;
        payload.sourcePath = m_filePath;
        payload.displayName = QFileInfo(m_filePath).completeBaseName();
        m_result = std::move(payload);
    } else {
        AppendProjectPayload payload;
        payload.model = resultModel.takeProjectData();
        payload.importTempo = choice.importTempo && !generated.tempos.isEmpty();
        payload.importTimeSignature =
            choice.importTimeSignature && !generated.timeSignatures.isEmpty();
        payload.sourcePath = m_filePath;
        m_result = std::move(payload);
    }

    m_terminal = true;
    emit ready();
}

void MidiLoadSession::detachTask() {
    if (!m_task)
        return;
    const auto task = m_task;
    m_task = nullptr;
    taskManager->terminateTask(task);
    if (taskManager->tasks().contains(task))
        taskManager->removeTask(task);
}

void MidiLoadSession::detachReprocessTask() {
    if (!m_reprocessTask)
        return;
    const auto task = m_reprocessTask;
    m_reprocessTask = nullptr;
    taskManager->terminateTask(task);
    if (taskManager->tasks().contains(task))
        taskManager->removeTask(task);
}
