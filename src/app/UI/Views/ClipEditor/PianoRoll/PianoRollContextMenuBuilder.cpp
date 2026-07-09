#include "PianoRollContextMenuBuilder.h"
#include "PianoRollGraphicsView.h"
#include "PianoRollSelectionModel.h"
#include "NoteView.h"
#include "PronunciationView.h"
#include "PianoRollGraphicsViewHelper.h"
#include "UI/Controls/Menu.h"
#include "UI/Utils/IconUtils.h"
#include "UI/Views/Common/TimeGraphicsScene.h"
#include "Controller/ClipController.h"
#include "Controller/ClipboardController.h"
#include "Global/ControllerGlobal.h"
#include "Model/AppStatus/AppStatus.h"
#include "Utils/TimelineSnapUtils.h"

#include <QGuiApplication>
#include <QJsonDocument>
#include <QMenu>
#include <QClipboard>
#include <QAction>
#include <QCursor>
#include <QMimeData>

namespace {
    const QSize kMenuIconSize(16, 16);
    const QColor kMenuIconColor(240, 240, 240);

    QIcon menuIcon(const QString &path) {
        return IconUtils::createTintedSvgIcon(path, kMenuIconSize, kMenuIconColor);
    }
}

namespace Helper = PianoRollGraphicsViewHelper;

Menu *PianoRollContextMenuBuilder::buildNoteContextMenu(
    PianoRollGraphicsView *view, NoteView *noteView, std::function<void()> onDeleteNotes,
    std::function<void(int noteId)> onOpenProperties) {
    const auto menu = new Menu(view);

    const auto actionEditLyric = menu->addAction(Menu::tr("Fill lyrics..."));
    actionEditLyric->setIcon(menuIcon(QStringLiteral(":/svg/icons/document_16_filled.svg")));
    QObject::connect(actionEditLyric, &QAction::triggered, clipController,
                     [=] { clipController->onFillLyric(view); });

    const auto actionSearchLyric = menu->addAction(Menu::tr("Search lyrics..."));
    actionSearchLyric->setIcon(menuIcon(QStringLiteral(":/svg/icons/search_16_regular.svg")));
    QObject::connect(actionSearchLyric, &QAction::triggered, clipController,
                     [=] { clipController->onSearchLyric(view); });

    menu->addSeparator();

    const auto actionSplit = menu->addAction(Menu::tr("Split Note"));
    actionSplit->setIcon(menuIcon(QStringLiteral(":/svg/icons/arrow_split_16_filled.svg")));
    QObject::connect(actionSplit, &QAction::triggered, view, [noteView, view] {
        const auto scenePos = view->mapToScene(QCursor::pos());
        const auto tick = static_cast<int>(view->sceneXToTick(scenePos.x()));
        Helper::splitNote(noteView->id(), tick);
    });

    menu->addSeparator();

    const auto actionCut = menu->addAction(Menu::tr("Cu&t"));
    actionCut->setIcon(menuIcon(QStringLiteral(":/svg/icons/cut_16_regular.svg")));
    QObject::connect(actionCut, &QAction::triggered, clipboardController,
                     &ClipboardController::cut);

    const auto actionCopy = menu->addAction(Menu::tr("&Copy"));
    actionCopy->setIcon(menuIcon(QStringLiteral(":/svg/icons/copy_16_regular.svg")));
    QObject::connect(actionCopy, &QAction::triggered, clipboardController,
                     &ClipboardController::copy);

    const auto actionRemove = menu->addAction(Menu::tr("&Delete"));
    actionRemove->setIcon(menuIcon(QStringLiteral(":/svg/icons/delete_16_regular.svg")));
    QObject::connect(actionRemove, &QAction::triggered, view, onDeleteNotes);

    menu->addSeparator();

    const auto actionProperties = menu->addAction(Menu::tr("Properties..."));
    actionProperties->setIcon(menuIcon(QStringLiteral(":/svg/icons/info_16_regular.svg")));
    QObject::connect(actionProperties, &QAction::triggered, view,
                     [noteView, onOpenProperties] { onOpenProperties(noteView->id()); });

    return menu;
}

Menu *
    PianoRollContextMenuBuilder::buildBackgroundContextMenu(PianoRollGraphicsView *view,
                                                            PianoRollSelectionModel *selectionModel,
                                                            const QPoint &pos, int offset) {
    const auto menu = new Menu(view);
    menu->installEventFilter(view);

    const auto mimeData = QGuiApplication::clipboard()->mimeData();
    const auto hasPasteData =
        mimeData &&
        mimeData->hasFormat(ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams));

    const auto actionPaste = menu->addAction(Menu::tr("&Paste"));
    actionPaste->setIcon(menuIcon(QStringLiteral(":/svg/icons/clipboard_paste_16_regular.svg")));
    actionPaste->setEnabled(hasPasteData);

    if (hasPasteData) {
        const auto array =
            mimeData->data(ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams));
        const auto json = QJsonDocument::fromJson(array);
        NotesParamsInfo info = NotesParamsInfo::deserializeFromJson(json.object());
        const auto scenePos = view->mapToScene(pos);
        const auto tick = qRound(view->sceneXToTick(scenePos.x())) + offset;
        const auto quantize = TimelineSnapUtils::quantizeToTicks(appStatus->pianoRollQuantize);
        const auto previewTick = TimelineSnapUtils::snapNearest(tick, quantize);

        QObject::connect(actionPaste, &QAction::triggered, view,
                         [info, tick] { clipController->pasteNotesWithParams(info, tick); });

        int minLocalStart = INT_MAX;
        for (const auto note : info.selectedNotes)
            minLocalStart = qMin(minLocalStart, note->localStart());

        QObject::connect(
            actionPaste, &QAction::hovered, view,
            [view, selectionModel, info, previewTick, minLocalStart, offset] {
                if (!selectionModel->pastePreviewViews().isEmpty())
                    return;
                for (const auto note : info.selectedNotes) {
                    const auto noteView = new NoteView(-1);
                    noteView->setPronunciationView(new PronunciationView(-1));
                    noteView->setRStart(note->localStart() - minLocalStart + previewTick - offset);
                    noteView->setLength(note->length());
                    noteView->setKeyIndex(note->keyIndex());
                    noteView->setLyric(note->lyric());
                    const auto pron = note->pronunciation();
                    noteView->setPronunciation(pron.isEdited() ? pron.edited : pron.original,
                                               pron.isEdited());
                    noteView->setOverlapped(note->overlapped());
                    noteView->setOpacity(0.35);
                    noteView->setAcceptedMouseButtons(Qt::NoButton);
                    noteView->setAcceptHoverEvents(false);
                    noteView->setFlag(QGraphicsItem::ItemIsSelectable, false);
                    if (noteView->pronunciationView())
                        noteView->pronunciationView()->setOpacity(0.35);

                    view->scene()->addCommonItem(noteView);
                    if (noteView->pronunciationView())
                        view->scene()->addCommonItem(noteView->pronunciationView());
                    selectionModel->pastePreviewViews().append(noteView);
                }
            });

        for (auto a : menu->actions()) {
            if (a != actionPaste && !a->isSeparator())
                QObject::connect(a, &QAction::hovered, view,
                                 [selectionModel] { selectionModel->clearPastePreviewViews(); });
        }

        QObject::connect(menu, &QMenu::aboutToHide, view,
                         [selectionModel] { selectionModel->clearPastePreviewViews(); });
    }

    return menu;
}