#include "TrackEditorContextMenuController.h"

#include "TrackEditorView.h"
#include "Controller/TrackController.h"
#include "Controller/PlaybackController.h"
#include "Global/ControllerGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Model/ClipboardDataModel/DecodedClipboardPayload.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Import/AudioFilePreparer.h"
#include "Modules/Extractors/MidiExtractController.h"
#include "UI/Views/Common/EditorMenuPreviewGuard.h"

#include <lite/GUI/Controls/Menu.h>
#include <lite/GUI/Utils/IconUtils.h>
#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <TalcsWidgets/AudioFileDialog.h>
#include <TalcsFormat/AbstractAudioFormatIO.h>

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMimeData>
#include <QWidget>

#include <memory>

namespace {

    std::shared_ptr<DecodedClipsPayload> decodeClipboardClips() {
        const auto *mimeData = QGuiApplication::clipboard()->mimeData();
        const auto format = ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip);
        if (!mimeData || !mimeData->hasFormat(format))
            return {};

        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(mimeData->data(format), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            return {};

        auto payload = std::make_shared<DecodedClipsPayload>(document.object());
        if (payload->isEmpty())
            return {};
        return payload;
    }

    TrackPastePreviewData makePreviewData(const ClipsInfo &info) {
        TrackPastePreviewData data;
        data.clips.reserve(info.clips.size());
        for (int index = 0; index < info.clips.size(); ++index) {
            const auto *clip = info.clips.at(index);
            if (!clip)
                continue;

            TrackPastePreviewClip preview;
            preview.type = clip->clipType();
            preview.properties = Clip::ClipCommonProperties(*clip);
            preview.trackIndexOffset = info.trackIndexOffsets.value(index, 0);
            if (const auto *singing = qobject_cast<const SingingClip *>(clip)) {
                preview.defaultLanguage = singing->defaultLanguage();
                preview.notes.reserve(singing->notes().count());
                for (const auto *note : singing->notes()) {
                    preview.notes.append(TrackPastePreviewNote{note->localStart(), note->length(),
                                                               note->keyIndex()});
                }
            } else if (const auto *audio = qobject_cast<const AudioClip *>(clip)) {
                preview.audioPath = audio->path();
                preview.audioInfo = audio->audioInfo();
            }
            data.clips.append(std::move(preview));
        }
        return data;
    }

} // namespace

TrackEditorContextMenuController::TrackEditorContextMenuController(QWidget *owner)
    : QObject(owner), m_owner(owner) {
}

void TrackEditorContextMenuController::showMenu(const TrackEditorMenuContext &context,
                                                ITrackPastePreviewHost *previewHost) {
    if (!m_owner || context.trackIndex < 0 || context.trackIndex >= appModel->tracks().size())
        return;

    Menu menu(m_owner);
    QAction *previewAction = nullptr;
    if (context.target == TrackEditorMenuContext::Target::Background) {
        auto *create = menu.addAction(tr("New singing clip"));
        create->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/midi_clip_16_filled.svg")));
        connect(create, &QAction::triggered, this, [context] {
            trackController->onNewSingingClip(context.trackIndex, context.snappedTick);
        });

        auto *insertAudio = menu.addAction(tr("Insert audio clip..."));
        insertAudio->setIcon(
            IconUtils::menuIcon(QStringLiteral(":/svg/icons/audio_clip_16_filled.svg")));
        connect(insertAudio, &QAction::triggered, this,
                [this, context] { insertAudioClip(context.trackIndex, context.snappedTick); });
        menu.addSeparator();

        const auto payload = decodeClipboardClips();
        auto *paste = menu.addAction(tr("&Paste"));
        previewAction = paste;
        paste->setIcon(
            IconUtils::menuIcon(QStringLiteral(":/svg/icons/clipboard_paste_16_regular.svg")));
        paste->setEnabled(payload != nullptr);
        if (payload) {
            const auto previewData = makePreviewData(payload->info());
            // The backend already snapped to the WYSIWYG grid; the preview
            // must match the position pasteClips() will actually use.
            const auto previewTick = context.snappedTick;
            connect(paste, &QAction::hovered, this,
                    [previewHost, previewData, previewTick, context] {
                        if (previewHost) {
                            previewHost->showTrackPastePreview(previewData, previewTick,
                                                               context.trackIndex);
                        }
                    });
            connect(paste, &QAction::triggered, this, [payload, context] {
                trackController->pasteClips(payload->info(), context.snappedTick,
                                            context.trackIndex);
            });
        }
    } else {
        if (context.target == TrackEditorMenuContext::Target::AudioClip) {
            auto *relink = new QAction(tr("Relink Audio File..."), &menu);
            relink->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/link_16_filled.svg")));
            connect(relink, &QAction::triggered, this,
                    [this, clipId = context.clipId] { relocateAudioClip(clipId); });

            auto *extractMidi = new QAction(tr("Extract MIDI Score"), &menu);
            extractMidi->setIcon(
                IconUtils::menuIcon(QStringLiteral(":/svg/icons/arrow_export_16_regular.svg")));
            connect(extractMidi, &QAction::triggered, this, [clipId = context.clipId] {
                if (const auto *audio =
                        qobject_cast<const AudioClip *>(appModel->findClipById(clipId))) {
                    midiExtractController->runExtractMidi(audio);
                }
            });

            if (context.audioMissing) {
                menu.addAction(relink);
                menu.addSeparator();
                menu.addAction(extractMidi);
            } else {
                menu.addAction(extractMidi);
                menu.addAction(relink);
            }
            menu.addSeparator();
        }

        auto *cut = menu.addAction(tr("Cu&t"));
        cut->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/cut_16_regular.svg")));
        connect(cut, &QAction::triggered, this, &TrackEditorContextMenuController::cutSelection);

        auto *copy = menu.addAction(tr("&Copy"));
        copy->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/copy_16_regular.svg")));
        connect(copy, &QAction::triggered, this, &TrackEditorContextMenuController::copySelection);

        auto *remove = menu.addAction(tr("&Delete"));
        remove->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/delete_16_regular.svg")));
        const auto selectedIds = context.selectedClipIds.isEmpty() ? QList<int>{context.clipId}
                                                                   : context.selectedClipIds;
        connect(remove, &QAction::triggered, this,
                [this, selectedIds] { deleteSelection(selectedIds); });
    }

    new EditorMenuPreviewGuard(&menu, previewAction, [previewHost] {
        if (previewHost)
            previewHost->clearTrackPastePreview();
    });
    menu.exec(context.globalPos);
}

void TrackEditorContextMenuController::cutSelection() const {
    trackController->cutSelectedClips();
}

void TrackEditorContextMenuController::copySelection() const {
    trackController->copySelectedClips();
}

void TrackEditorContextMenuController::pasteSelection() const {
    const auto payload = decodeClipboardClips();
    if (!payload)
        return;
    auto trackIndex = appStatus->selectedTrackIndex.get();
    if (trackIndex < 0)
        trackIndex = 0;
    if (trackIndex >= appModel->tracks().size())
        return;
    const auto tick = qRound(playbackController->position());
    const auto snappedTick =
        TimelineSnapUtils::snapNearest(tick, gridStepAt(tick), appModel->timeline());
    trackController->pasteClips(payload->info(), snappedTick, trackIndex);
}

void TrackEditorContextMenuController::deleteSelection(const QList<int> &clipIds) const {
    const auto ids = clipIds.isEmpty() ? appStatus->selectedClips.get() : clipIds;
    if (!ids.isEmpty())
        trackController->onRemoveClips(ids);
}

int TrackEditorContextMenuController::gridStepAt(int tick) const {
    const auto *view = qobject_cast<const TrackEditorView *>(m_owner);
    return view ? view->currentGridStep(tick) : 1;
}

void TrackEditorContextMenuController::selectAll() const {
    QList<int> ids;
    for (const auto *track : appModel->tracks()) {
        for (const auto *clip : track->clips())
            ids.append(clip->id());
    }
    trackController->setSelectedClips(ids);
    if (!ids.isEmpty())
        trackController->setActiveClip(ids.first());
}

void TrackEditorContextMenuController::insertAudioClip(const int trackIndex, const int tick) const {
    QString fileName;
    QVariant userData;
    QString entryClassName;
    auto io = talcs::AudioFileDialog::getOpenAudioFileIO(
        AudioContext::instance()->formatManager(), fileName, userData, entryClassName, m_owner,
        tr("Select an Audio File"), QStringLiteral("."));
    if (fileName.isNull() || trackIndex < 0 || trackIndex >= appModel->tracks().size()) {
        // The dialog-probed IO is only used for preparation; drop it when the
        // selection is abandoned.
        delete io;
        return;
    }
    // Selection and preparation are separated: the dialog picks the file and
    // probes the format, then the shared AudioFilePreparer pipeline decodes
    // and commits it (same path as drag-and-drop in later phases).
    const auto workspace = AudioFilePreparer::makeWorkspace(userData, entryClassName);
    trackController->onAddAudioClip(fileName, io, workspace,
                                    appModel->tracks().at(trackIndex)->id(), tick);
}

void TrackEditorContextMenuController::relocateAudioClip(const int clipId) const {
    if (!qobject_cast<AudioClip *>(appModel->findClipById(clipId)))
        return;
    QString fileName;
    QVariant userData;
    QString entryClassName;
    auto io = talcs::AudioFileDialog::getOpenAudioFileIO(
        AudioContext::instance()->formatManager(), fileName, userData, entryClassName, m_owner,
        tr("Select an Audio File"), QStringLiteral("."));
    if (fileName.isNull())
        return;
    trackController->onRelocateAudioClip(
        clipId, fileName, io, AudioFilePreparer::makeWorkspace(userData, entryClassName));
}
