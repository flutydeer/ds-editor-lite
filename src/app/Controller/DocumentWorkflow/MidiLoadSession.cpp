#include "MidiLoadSession.h"

#include "Controller/Tasks/MidiParseTask.h"
#include "Controller/Tasks/MidiReprocessTask.h"
#include "Model/AppModel/SingingClipPhonemeNormalizer.h"
#include "Modules/ProjectConverters/MidiConfigPage.h"
#include "Modules/ProjectFormats/IProjectConfigPage.h"
#include "Modules/ProjectFormats/IProjectFormatHandler.h"
#include "Modules/ProjectFormats/ProjectImportConfigDialog.h"
#include <lite/ProjectConverters/MidiTextCodecConverter.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/LoopSettings.h>
#include "UI/Dialogs/Base/Dialog.h"

#include <QCoreApplication>
#include <QFileInfo>

#include <utility>

MidiLoadSession::MidiLoadSession(IProjectFormatHandler *formatHandler, QString filePath,
                                 const ProjectLoadPurpose purpose, const quint64 requestId,
                                 const bool interactive, QByteArray encoding, const bool importTempo,
                                 const bool importTimeSignature, QObject *parent)
    : ProjectLoadSessionBase(std::move(filePath), requestId, parent),
      m_formatHandler(formatHandler), m_purpose(purpose), m_interactive(interactive),
      m_encoding(std::move(encoding)), m_importTempo(importTempo),
      m_importTimeSignature(importTimeSignature) {
}

void MidiLoadSession::onStart() {
    startParseTask();
}

void MidiLoadSession::startConfiguration() {
    if (!m_interactive) {
        MidiUserInput input;
        QByteArray lyrics;
        for (const auto &info : m_parseData.trackInfos) {
            for (const auto &lyric : info.lyrics)
                lyrics.append(lyric);
        }
        input.encoding.codec = m_encoding;
        if (input.encoding.codec.isEmpty())
            input.encoding.codec = MidiTextCodecConverter::detectEncoding(lyrics);
        if (input.encoding.codec.isEmpty())
            input.encoding.codec = MidiTextCodecConverter::defaultCodec();
        for (int index = 0; index < m_parseData.trackInfos.size(); ++index) {
            if (m_parseData.trackInfos.at(index).selectedByDefault)
                input.tracks.selectedTrackIndices.append(index);
        }
        input.channels.separateChannels = true;
        input.timeline.importTempo = m_importTempo;
        input.timeline.importTimeSignature = m_importTimeSignature;
        materialize(input);
        return;
    }
    auto *dialog = new ProjectImportConfigDialog(Dialog::globalParent());
    dialog->setWindowTitle(tr("Configure MIDI Import"));
    auto *page = m_formatHandler->createConfigPage(dialog);
    auto *midiPage = qobject_cast<MidiConfigPage *>(page ? page->widget() : nullptr);
    m_configPage = page;
    dialog->setPage(midiPage);
    midiPage->setTrackInfoList(m_parseData.trackInfos);
    // Tempo / time signature default to enabled for both Open and Import:
    // imports usually merge parts of the same song, where adopting the
    // source timeline is harmless (and desired on a fresh project).
    midiPage->setImportTempo(true);
    midiPage->setImportTimeSignature(true);
    midiPage->detectCodec();
    connect(midiPage, &MidiConfigPage::separateMidiChannelsChanged, this,
            [this](const bool enabled) { requestReprocess(enabled); });
    dialog->resize(720, 480);

    const auto accepted = dialog->exec() == QDialog::Accepted;
    MidiUserInput input;
    if (accepted && midiPage)
        input = midiPage->collectInput();
    m_configPage = nullptr;
    dialog->deleteLater();
    if (isTerminal())
        return;
    if (!accepted) {
        emitCanceled();
        return;
    }

    materialize(input);
}

Task *MidiLoadSession::createParseTask() {
    return new MidiParseTask(m_filePath, m_requestId);
}

void MidiLoadSession::handleParseResult(Task *task) {
    auto *parseTask = static_cast<MidiParseTask *>(task);
    m_parseData = parseTask->takeResult();
    if (!m_parseData.valid) {
        fail({tr("Failed to load MIDI"), m_parseData.errorMessage});
        return;
    }

    startConfiguration();
}

Task *MidiLoadSession::createReprocessTask() {
    return new MidiReprocessTask(m_parseData.rawData, m_separateChannels);
}

void MidiLoadSession::handleReprocessResult(Task *task) {
    auto *reprocessTask = static_cast<MidiReprocessTask *>(task);
    auto result = reprocessTask->takeResult();
    if (result.errorMessage.isEmpty()) {
        m_parseData.mediate = std::move(result.mediate);
        if (m_configPage) {
            if (auto *page = qobject_cast<MidiConfigPage *>(m_configPage->widget())) {
                page->setTrackInfoList(result.trackInfos);
                page->detectCodec();
            }
        }
    }
    // On re-parse errors keep the previous track list; the user can still
    // proceed with the last valid layout.
}

void MidiLoadSession::requestReprocess(const bool separateChannels) {
    m_separateChannels = separateChannels;
    ProjectLoadSessionBase::requestReprocess();
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
        fail({tr("Failed to load MIDI"), generated.errorMessage});
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
        fail({tr("Failed to load MIDI"),
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
        finishWithResult(std::move(payload));
    } else {
        AppendProjectPayload payload;
        payload.model = resultModel.takeProjectData();
        payload.importTempo = choice.importTempo && !generated.tempos.isEmpty();
        payload.importTimeSignature =
            choice.importTimeSignature && !generated.timeSignatures.isEmpty();
        payload.sourcePath = m_filePath;
        finishWithResult(std::move(payload));
    }
}
