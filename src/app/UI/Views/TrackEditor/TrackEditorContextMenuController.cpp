#include "TrackEditorContextMenuController.h"

#include "Controller/TrackController.h"
#include "Global/ControllerGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Model/ClipboardDataModel/ClipsInfo.h"
#include "Modules/Audio/AudioContext.h"
#include "Modules/Extractors/MidiExtractController.h"

#include <lite/GUI/Controls/Menu.h>
#include <lite/GUI/Utils/IconUtils.h>
#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/AudioClip.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <TalcsWidgets/AudioFileDialog.h>

#include <QClipboard>
#include <QDataStream>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QIODevice>
#include <QMimeData>
#include <QWidget>

#include <memory>

namespace {

    struct DecodedClipsPayload {
        ClipsInfo info;

        ~DecodedClipsPayload() {
            qDeleteAll(info.clips);
        }
    };

    std::shared_ptr<DecodedClipsPayload> decodeClipboardClips() {
        const auto *mimeData = QGuiApplication::clipboard()->mimeData();
        const auto format = ControllerGlobal::ElemMimeType.at(ControllerGlobal::Clip);
        if (!mimeData || !mimeData->hasFormat(format))
            return {};

        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(mimeData->data(format), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            return {};

        auto payload = std::make_shared<DecodedClipsPayload>();
        payload->info = ClipsInfo::deserializeFromJson(document.object());
        if (payload->info.clips.isEmpty())
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

    QJsonObject audioWorkspace(const QVariant &userData, const QString &entryClassName) {
        QByteArray dataBuffer;
        QDataStream stream(&dataBuffer, QIODevice::WriteOnly);
        stream << userData;
        return {
            {QStringLiteral("userData"),       QString::fromLatin1(dataBuffer.toBase64())},
            {QStringLiteral("entryClassName"), entryClassName                            },
        };
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
        paste->setIcon(
            IconUtils::menuIcon(QStringLiteral(":/svg/icons/clipboard_paste_16_regular.svg")));
        paste->setEnabled(payload != nullptr);
        if (payload) {
            const auto previewData = makePreviewData(payload->info);
            const auto quantize = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
            const auto previewTick =
                TimelineSnapUtils::snapNearest(context.rawTick, quantize, appModel->timeline());
            connect(paste, &QAction::hovered, this,
                    [previewHost, previewData, previewTick, context] {
                        if (previewHost) {
                            previewHost->showTrackPastePreview(previewData, previewTick,
                                                               context.trackIndex);
                        }
                    });
            connect(paste, &QAction::triggered, this, [payload, context] {
                trackController->pasteClips(payload->info, context.rawTick, context.trackIndex);
            });
        }
        for (auto *action : menu.actions()) {
            if (action != paste && !action->isSeparator()) {
                connect(action, &QAction::hovered, this, [previewHost] {
                    if (previewHost)
                        previewHost->clearTrackPastePreview();
                });
            }
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
        connect(cut, &QAction::triggered, trackController, &TrackController::cutSelectedClips);

        auto *copy = menu.addAction(tr("&Copy"));
        copy->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/copy_16_regular.svg")));
        connect(copy, &QAction::triggered, trackController, &TrackController::copySelectedClips);

        auto *remove = menu.addAction(tr("&Delete"));
        remove->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/delete_16_regular.svg")));
        const auto selectedIds = context.selectedClipIds.isEmpty() ? QList<int>{context.clipId}
                                                                   : context.selectedClipIds;
        connect(remove, &QAction::triggered, this,
                [selectedIds] { trackController->onRemoveClips(selectedIds); });
    }

    connect(&menu, &QMenu::aboutToHide, this, [previewHost] {
        if (previewHost)
            previewHost->clearTrackPastePreview();
    });
    menu.exec(context.globalPos);
}

void TrackEditorContextMenuController::insertAudioClip(const int trackIndex, const int tick) const {
    QString fileName;
    QVariant userData;
    QString entryClassName;
    auto io = talcs::AudioFileDialog::getOpenAudioFileIO(
        AudioContext::instance()->formatManager(), fileName, userData, entryClassName, m_owner,
        tr("Select an Audio File"), QStringLiteral("."));
    if (fileName.isNull() || trackIndex < 0 || trackIndex >= appModel->tracks().size())
        return;
    trackController->onAddAudioClip(fileName, io, audioWorkspace(userData, entryClassName),
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
    trackController->onRelocateAudioClip(clipId, fileName, io,
                                         audioWorkspace(userData, entryClassName));
}
