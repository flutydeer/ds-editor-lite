#include "MidiLoadSession.h"

#include "Controller/Tasks/MidiParseTask.h"
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/LoopSettings.h>
#include <lite/Tasking/TaskManager.h>

#include <opendspxconverter/midi/midiconverter.h>

#include <QCoreApplication>
#include <QFileInfo>

#include <sstream>
#include <utility>

namespace {
    // Re-derives the track layout when the interactive UI toggles
    // "separate MIDI channels" - re-parses the raw bytes and rebuilds the
    // track info list.
    class MidiTrackReconverterImpl final : public MidiTrackReconverter {
    public:
        explicit MidiTrackReconverterImpl(MidiParseData &data) : data(data) {
        }

        QList<MidiImportTrackInfo> reconvert(const bool separateChannels) override {
            opendspx::MidiConverter converter;
            opendspx::MidiConverter::Error error;
            std::stringstream ss(data.rawData.toStdString(), std::ios::in);
            auto updated = converter.convertMidiToIntermediate(ss, error, {separateChannels});
            if (error == opendspx::MidiConverter::Error::NoError)
                data.mediate = std::move(updated);
            return buildMidiTrackInfoList(data.mediate.tracks());
        }

        MidiParseData &data;
    };
}

MidiLoadSession::MidiLoadSession(QString filePath, const ProjectLoadPurpose purpose,
                                 const quint64 requestId, QObject *parent)
    : IProjectLoadSession(parent), m_filePath(std::move(filePath)), m_purpose(purpose),
      m_requestId(requestId) {
}

MidiLoadSession::~MidiLoadSession() {
    detachTask();
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

    MidiTrackReconverterImpl reconverter{m_parseData};
    MidiImportOptions choice;
    const auto defaultImportTimeline = m_purpose == ProjectLoadPurpose::Open;
    if (!m_converterUi.chooseImportOptions(m_parseData.trackInfos, reconverter,
                                           defaultImportTimeline, defaultImportTimeline, choice)) {
        m_terminal = true;
        emit canceled();
        return;
    }
    if (m_terminal)
        return;

    materialize(choice);
}

void MidiLoadSession::materialize(const MidiImportOptions &choice) {
    AppModel resultModel;
    const auto language = m_converterUi.importLanguage();
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
