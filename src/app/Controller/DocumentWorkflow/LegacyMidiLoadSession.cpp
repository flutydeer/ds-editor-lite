#include "LegacyMidiLoadSession.h"

#include "Model/AppModel/AppModel.h"
#include "Model/AppModel/LoopSettings.h"
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"
#include "Modules/ProjectConverters/MidiConverter.h"

#include <QFileInfo>

#include <utility>

LegacyMidiLoadSession::LegacyMidiLoadSession(QString filePath, const ProjectLoadPurpose purpose,
                                             const quint64 requestId, QObject *parent)
    : IProjectLoadSession(parent), m_filePath(std::move(filePath)), m_purpose(purpose),
      m_requestId(requestId) {
}

void LegacyMidiLoadSession::start() {
    if (m_started || m_terminal)
        return;
    m_started = true;

    AppModel resultModel;
    MidiConverter converter;
    MidiConverter::LoadOptions options;
    QString errorMessage;
    const auto mode = m_purpose == ProjectLoadPurpose::Open
                          ? IProjectConverter::ImportMode::NewProject
                          : IProjectConverter::ImportMode::AppendToProject;
    const auto status =
        converter.loadInteractive(m_filePath, &resultModel, errorMessage, mode, options);
    if (status == MidiConverter::LoadStatus::Canceled) {
        m_terminal = true;
        emit canceled();
        return;
    }
    if (status == MidiConverter::LoadStatus::Failed) {
        m_terminal = true;
        emit failed({tr("Failed to load MIDI"), errorMessage});
        return;
    }

    SingingClipPhonemeNormalizer::normalizeEditedOffsets(resultModel);
    if (m_purpose == ProjectLoadPurpose::Open) {
        ReplaceProjectPayload payload;
        payload.model = resultModel.takeProjectData();
        payload.loopSettings = LoopSettings();
        payload.sourceKind = ProjectSourceKind::Foreign;
        payload.sourcePath = m_filePath;
        payload.displayName = QFileInfo(m_filePath).baseName();
        m_result = std::move(payload);
    } else {
        AppendProjectPayload payload;
        payload.model = resultModel.takeProjectData();
        payload.importTempo = options.importTempo;
        payload.importTimeSignature = options.importTimeSignature;
        payload.sourcePath = m_filePath;
        m_result = std::move(payload);
    }

    m_terminal = true;
    emit ready();
}

void LegacyMidiLoadSession::cancel() {
    if (m_terminal)
        return;
    m_terminal = true;
    emit canceled();
}

PreparedProject LegacyMidiLoadSession::takeResult() {
    return std::move(m_result);
}

quint64 LegacyMidiLoadSession::requestId() const {
    return m_requestId;
}
