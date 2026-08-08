#include "OpendspxImportLoadSession.h"

#include "Model/AppModel/SingingClipPhonemeNormalizer.h"
#include "Modules/ProjectConverters/DspxConfigPage.h"
#include "Modules/ProjectConverters/DspxProjectConverterUi.h"
#include "Modules/ProjectFormats/IProjectFormatHandler.h"
#include "Modules/ProjectFormats/ProjectImportConfigDialog.h"
#include <lite/ProjectConverters/DspxProjectConverter.h>
#include <lite/ProjectModel/AppModel/AppModel.h>

#include <opendspx/clip.h>
#include <opendspx/model.h>
#include <opendspx/singingclip.h>
#include <opendspx/track.h>

#include "UI/Dialogs/Base/Dialog.h"

#include <QCoreApplication>
#include <QFileInfo>

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
                info.rangeText = OpendspxImportLoadSession::tr("Mixed");
            else if (hasSinging)
                info.rangeText = OpendspxImportLoadSession::tr("Singing");
            else if (hasAudio)
                info.rangeText = OpendspxImportLoadSession::tr("Audio");
            else
                info.rangeText = OpendspxImportLoadSession::tr("Empty");
            info.noteCount = noteCount;
            info.selectedByDefault = hasSinging || hasAudio;
            infos.append(info);
        }
        return infos;
    }
} // namespace

OpendspxImportLoadSession::OpendspxImportLoadSession(IProjectFormatHandler *formatHandler,
                                                     QString filePath,
                                                     const ProjectLoadPurpose purpose,
                                                     const quint64 requestId, QObject *parent)
    : ProjectLoadSessionBase(std::move(filePath), requestId, parent),
      m_formatHandler(formatHandler), m_purpose(purpose) {
}

OpendspxImportLoadSession::~OpendspxImportLoadSession() = default;

void OpendspxImportLoadSession::onStart() {
    startParseTask();
}

bool OpendspxImportLoadSession::shouldPublishProgress() const {
    return true;
}

void OpendspxImportLoadSession::handleParseResult(Task *task) {
    auto outcome = takeParsedModel(task);
    if (!outcome.errorMessage.isEmpty()) {
        fail({tr("Failed to import project"), outcome.errorMessage});
        return;
    }

    m_model = std::move(outcome.model);
    startConfiguration();
}

void OpendspxImportLoadSession::startConfiguration() {
    auto *dialog = new ProjectImportConfigDialog(Dialog::globalParent());
    dialog->setWindowTitle(tr("Configure Import"));
    auto *page = m_formatHandler->createConfigPage(dialog);
    auto *dspxPage = qobject_cast<DspxConfigPage *>(page ? page->widget() : nullptr);
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
    dialog->deleteLater();
    if (isTerminal())
        return;
    if (!accepted) {
        emitCanceled();
        return;
    }

    materialize(input);
}

void OpendspxImportLoadSession::materialize(const DspxUserInput &input) {
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
        fail({tr("Failed to import project"),
              QCoreApplication::translate("OpendspxImportLoadSession",
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
        fail({tr("Failed to import project"), errorMessage});
        return;
    }

    SingingClipPhonemeNormalizer::normalizeEditedOffsets(resultModel);

    if (m_purpose == ProjectLoadPurpose::Open) {
        // Open replaces the whole document: the Ui converter pushes the
        // source loop region into AppStatus, matching native DSPX Open.
        emit progressChanged({tr("Opening Project"), tr("Applying project..."), 0, 100, 0, true});
        AppModel openModel;
        LoopSettings openLoopSettings;
        QString openErrorMessage;
        DspxProjectConverterUi openConverter;
        if (!openConverter.loadParsedProject(*filteredModel, &openModel, openLoopSettings,
                                             openErrorMessage,
                                             IProjectConverter::ImportMode::NewProject)) {
            fail({tr("Failed to open project"), openErrorMessage});
            return;
        }
        SingingClipPhonemeNormalizer::normalizeEditedOffsets(openModel);
        ReplaceProjectPayload payload;
        payload.model = openModel.takeProjectData();
        payload.loopSettings = openLoopSettings;
        payload.sourceKind = ProjectSourceKind::Foreign;
        payload.sourcePath = m_filePath;
        payload.displayName = QFileInfo(m_filePath).completeBaseName();
        finishWithResult(std::move(payload));
        return;
    }

    AppendProjectPayload payload;
    payload.model = resultModel.takeProjectData();
    payload.importTempo = input.timeline.importTempo;
    payload.importTimeSignature = input.timeline.importTimeSignature;
    payload.sourcePath = m_filePath;
    finishWithResult(std::move(payload));
}
