#include "PianoRollContextMenuController.h"

#include "PianoRollGraphicsViewHelper.h"
#include "Controller/ClipController.h"
#include "Controller/PlaybackController.h"
#include "Global/AppGlobal.h"
#include "Global/ControllerGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Model/ClipboardDataModel/DecodedClipboardPayload.h"
#include "UI/Dialogs/Note/PhonemeEditorDialog.h"
#include "UI/Views/Common/EditorMenuPreviewGuard.h"

#include <lite/GUI/Controls/Menu.h>
#include <lite/GUI/Utils/IconUtils.h>
#include <lite/MusicBase/TimelineSnapUtils.h>
#include <lite/ProjectModel/AppModel/AppModel.h>
#include <lite/ProjectModel/AppModel/Note.h>
#include <lite/ProjectModel/AppModel/SingingClip.h>

#include <QActionGroup>
#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMimeData>
#include <QWidget>

#include <memory>
#include <algorithm>

namespace {

    std::shared_ptr<DecodedNotesPayload> decodeClipboardNotes() {
        const auto *mimeData = QGuiApplication::clipboard()->mimeData();
        const auto format = ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams);
        if (!mimeData || !mimeData->hasFormat(format))
            return {};

        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(mimeData->data(format), &error);
        if (error.error != QJsonParseError::NoError || !document.isObject())
            return {};

        auto payload = std::make_shared<DecodedNotesPayload>(document.object());
        if (payload->isEmpty())
            return {};
        return payload;
    }

    PianoRollPastePreviewData makePreviewData(const NotesParamsInfo &info) {
        PianoRollPastePreviewData data;
        if (info.selectedNotes.isEmpty())
            return data;

        auto minimumStart = info.selectedNotes.first()->localStart();
        for (const auto *note : info.selectedNotes)
            minimumStart = std::min(minimumStart, note->localStart());

        data.notes.reserve(info.selectedNotes.size());
        for (const auto *note : info.selectedNotes) {
            const auto pronunciation = note->pronunciation();
            data.notes.append({note->localStart() - minimumStart, note->length(), note->keyIndex(),
                               note->lyric(), pronunciation.result(), pronunciation.isEdited(),
                               note->overlapped()});
        }
        return data;
    }

} // namespace

PianoRollContextMenuController::PianoRollContextMenuController(QWidget *owner)
    : QObject(owner), m_owner(owner) {
}

void PianoRollContextMenuController::showMenu(const PianoRollMenuContext &context,
                                              SingingClip *clip,
                                              IPianoRollPastePreviewHost *previewHost,
                                              IAnchorCommandHost *anchorHost) {
    if (!m_owner || !clip)
        return;

    // Right-click on the pronunciation glyph opens a dedicated quick-switch
    // menu with only the candidate pronunciations for that note.
    if (context.pronunciationTarget) {
        showPronunciationOnlyMenu(context, clip);
        return;
    }

    Menu menu(m_owner);
    QAction *previewAction = nullptr;
    if (context.target == PianoRollMenuContext::Target::Anchor) {
        if (!anchorHost)
            return;
        auto *linear = menu.addAction(tr("Linear"));
        linear->setCheckable(true);
        linear->setChecked(context.anchorMode == PianoRollAnchorMode::Linear);
        linear->setEnabled(context.anchorInterpolationEnabled);
        connect(linear, &QAction::triggered, this, [anchorHost] {
            anchorHost->setSelectedAnchorInterpolation(PianoRollAnchorMode::Linear);
        });

        auto *hermite = menu.addAction(tr("Hermite"));
        hermite->setCheckable(true);
        hermite->setChecked(context.anchorMode == PianoRollAnchorMode::Hermite);
        hermite->setEnabled(context.anchorInterpolationEnabled);
        connect(hermite, &QAction::triggered, this, [anchorHost] {
            anchorHost->setSelectedAnchorInterpolation(PianoRollAnchorMode::Hermite);
        });

        auto *group = new QActionGroup(&menu);
        group->setExclusive(true);
        group->addAction(linear);
        group->addAction(hermite);
        if (context.anchorMode == PianoRollAnchorMode::Mixed) {
            linear->setChecked(false);
            hermite->setChecked(false);
        }

        menu.addSeparator();
        auto *remove = menu.addAction(tr("&Delete"));
        remove->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/delete_16_regular.svg")));
        connect(remove, &QAction::triggered, this,
                [anchorHost] { anchorHost->deleteSelectedAnchors(); });
    } else if (context.target == PianoRollMenuContext::Target::Note) {
        // For polyphonic characters, g2p returns multiple candidate
        // pronunciations; surface them at the top of the note context menu
        // so the user can switch pronunciation with a single click.
        appendPronunciationCandidateActions(menu, clip, context.noteId);

        const auto singerInfo = clip->singerInfo();
        const bool hasLanguages = singerInfo.resolutionState() == ResolutionState::Resolved &&
                                  !singerInfo.languages().isEmpty();
        auto *languageMenu = new Menu(tr("Language"), &menu);
        auto *languageGroup = new QActionGroup(languageMenu);
        languageGroup->setExclusive(true);

        // auto/follow singer: checked when note language is unspecified (empty)
        auto *followAction = languageMenu->addAction(tr("Follow singer"));
        followAction->setCheckable(true);
        followAction->setChecked(context.noteLanguage.isEmpty());
        languageGroup->addAction(followAction);
        connect(followAction, &QAction::triggered, this, [context] {
            clipController->onNoteLanguagesEdited(context.selectedNoteIds, QString());
        });

        if (hasLanguages) {
            for (const auto &language : singerInfo.languages()) {
                const auto id = language.id();
                auto text = id;
                if (id == singerInfo.defaultLanguage())
                    text += tr(" (default)");
                auto *action = languageMenu->addAction(text);
                action->setCheckable(true);
                action->setChecked(context.noteLanguage == id);
                languageGroup->addAction(action);
                connect(action, &QAction::triggered, this, [context, id] {
                    clipController->onNoteLanguagesEdited(context.selectedNoteIds, id);
                });
            }
            languageMenu->addSeparator();
        }

        // "unknown" = detection failure / terminal language; keep manual entry, distinct from auto
        auto *unknownAction = languageMenu->addAction(tr("Unknown"));
        unknownAction->setCheckable(true);
        unknownAction->setChecked(context.noteLanguage == QStringLiteral("unknown"));
        languageGroup->addAction(unknownAction);
        connect(unknownAction, &QAction::triggered, this, [context] {
            clipController->onNoteLanguagesEdited(context.selectedNoteIds,
                                                  QStringLiteral("unknown"));
        });
        menu.addMenu(languageMenu);

        auto *fillLyrics = menu.addAction(tr("Fill lyrics..."));
        connect(fillLyrics, &QAction::triggered, clipController,
                [this] { clipController->onFillLyric(m_owner); });

        auto *editPhonemes = menu.addAction(tr("Edit Phonemes..."));
        editPhonemes->setEnabled(context.phonemeEditorEnabled);
        connect(editPhonemes, &QAction::triggered, this,
                [this, clip, noteId = context.noteId] { openPhonemeEditor(clip, noteId); });
        menu.addSeparator();

        auto *cut = menu.addAction(tr("Cu&t"));
        cut->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/cut_16_regular.svg")));
        connect(cut, &QAction::triggered, this, &PianoRollContextMenuController::cutSelection);

        auto *copy = menu.addAction(tr("&Copy"));
        copy->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/copy_16_regular.svg")));
        connect(copy, &QAction::triggered, this, &PianoRollContextMenuController::copySelection);

        auto *remove = menu.addAction(tr("&Delete"));
        remove->setIcon(IconUtils::menuIcon(QStringLiteral(":/svg/icons/delete_16_regular.svg")));
        connect(remove, &QAction::triggered, this,
                [this, context] { deleteSelection(context.selectedNoteIds); });

        auto *split = menu.addAction(tr("Split Note"));
        split->setIcon(
            IconUtils::menuIcon(QStringLiteral(":/svg/icons/arrow_split_16_filled.svg")));
        connect(split, &QAction::triggered, this, [context] {
            PianoRollGraphicsViewHelper::splitNote(context.noteId, context.globalTick);
        });
        menu.addSeparator();

        auto *searchLyrics = menu.addAction(tr("Search lyrics..."));
        searchLyrics->setIcon(
            IconUtils::menuIcon(QStringLiteral(":/svg/icons/search_16_regular.svg")));
        connect(searchLyrics, &QAction::triggered, clipController,
                [this] { clipController->onSearchLyric(m_owner); });
    } else {
        const auto payload = decodeClipboardNotes();
        auto *paste = menu.addAction(tr("&Paste"));
        previewAction = paste;
        paste->setIcon(
            IconUtils::menuIcon(QStringLiteral(":/svg/icons/clipboard_paste_16_regular.svg")));
        paste->setEnabled(payload != nullptr);
        if (payload) {
            const auto previewData = makePreviewData(payload->info());
            const auto quantize = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
            const auto previewTick =
                TimelineSnapUtils::snapNearest(context.globalTick, quantize, appModel->timeline());
            connect(paste, &QAction::hovered, this, [previewHost, previewData, previewTick] {
                if (previewHost)
                    previewHost->showPianoRollPastePreview(previewData, previewTick);
            });
            connect(paste, &QAction::triggered, this, [payload, previewTick] {
                clipController->pasteNotesWithParams(payload->info(), previewTick);
            });
        }
    }

    new EditorMenuPreviewGuard(&menu, previewAction, [previewHost] {
        if (previewHost)
            previewHost->clearPianoRollPastePreview();
    });
    menu.exec(context.globalPos);
}

void PianoRollContextMenuController::cutSelection() const {
    clipController->cutSelectedNotesWithParams();
}

void PianoRollContextMenuController::copySelection() const {
    clipController->copySelectedNotesWithParams();
}

void PianoRollContextMenuController::pasteSelection() const {
    const auto payload = decodeClipboardNotes();
    if (payload)
        clipController->pasteNotesWithParams(payload->info(),
                                             qRound(playbackController->position()));
}

void PianoRollContextMenuController::deleteSelection(const QList<int> &noteIds) const {
    const auto ids = noteIds.isEmpty() ? appStatus->selectedNotes.get() : noteIds;
    if (!ids.isEmpty())
        clipController->onRemoveNotes(ids);
}

void PianoRollContextMenuController::selectAll() const {
    clipController->onSelectAllNotes();
}

bool PianoRollContextMenuController::appendPronunciationCandidateActions(Menu &menu,
                                                                         SingingClip *clip,
                                                                         const int noteId) const {
    if (!clip || noteId < 0)
        return false;
    auto *note = clip->findNoteById(noteId);
    if (!note)
        return false;

    auto candidates = note->pronCandidates();
    for (int i = candidates.size() - 1; i >= 0; i--) {
        if (candidates[i].trimmed().isEmpty())
            candidates.removeAt(i);
    }
    candidates.removeDuplicates();
    if (candidates.size() < 2)
        return false;

    const auto pronunciation = note->pronunciation();
    const QString current =
        pronunciation.isEdited() ? pronunciation.edited : pronunciation.original;
    auto *pronGroup = new QActionGroup(&menu);
    pronGroup->setExclusive(true);
    for (const auto &candidate : candidates) {
        auto *action = menu.addAction(candidate, this, [this, noteId, candidate] {
            clipController->onNotePronunciationEdited(noteId, candidate);
        });
        action->setCheckable(true);
        action->setChecked(candidate == current);
        pronGroup->addAction(action);
    }
    menu.addSeparator();
    return true;
}

void PianoRollContextMenuController::showPronunciationOnlyMenu(const PianoRollMenuContext &context,
                                                               SingingClip *clip) const {
    if (!m_owner || !clip)
        return;

    Menu menu(m_owner);
    // If the note has no switchable candidates, do not show an empty menu.
    if (!appendPronunciationCandidateActions(menu, clip, context.noteId))
        return;
    menu.exec(context.globalPos);
}

void PianoRollContextMenuController::openPhonemeEditor(SingingClip *clip, const int noteId) const {
    if (!clip)
        return;
    auto *note = clip->findNoteById(noteId);
    if (!note)
        return;
    auto *dialog = new PhonemeEditorDialog(note, m_owner);
    connect(dialog, &PhonemeEditorDialog::accepted, this, [dialog, noteId] {
        clipController->onNotePhonemesEdited(noteId, dialog->phonemeNames());
    });
    dialog->show();
}
