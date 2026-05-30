//
// Created by fluty on 24-2-17.
//

#include "ClipboardController.h"
#include "ClipboardController_p.h"

#include "ClipController.h"
#include "PlaybackController.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QMimeData>

ClipboardController::ClipboardController(QObject *parent)
    : QObject(parent), d_ptr(new ClipboardControllerPrivate(this)) {
}

ClipboardController::~ClipboardController() = default;

LITE_SINGLETON_IMPLEMENT_INSTANCE(ClipboardController)

void ClipboardController::copy() {
    Q_D(ClipboardController);
    d->copyCutSelectedItems(ControllerGlobal::NoteWithParams, false);
}

void ClipboardController::cut() {
    Q_D(ClipboardController);
    d->copyCutSelectedItems(ControllerGlobal::NoteWithParams, true);
}

void ClipboardController::paste() {
    const auto mimeData = QGuiApplication::clipboard()->mimeData();
    if (mimeData->hasFormat(ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams))) {
        const auto array =
            mimeData->data(ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams));
        const auto json = QJsonDocument::fromJson(array);
        const auto info = NotesParamsInfo::deserializeFromJson(json.object());
        const auto tick = static_cast<int>(playbackController->position());
        clipController->pasteNotesWithParams(info, tick);
    }
}

void ClipboardControllerPrivate::copyCutSelectedItems(const ControllerGlobal::ElemType type, const bool isCut) {
    switch (type) {
        case ControllerGlobal::LoopStart:
            break;
        case ControllerGlobal::LoopEnd:
            break;
        case ControllerGlobal::Tempo:
            break;
        case ControllerGlobal::TimeSignature:
            break;
        case ControllerGlobal::Track:
            break;
        case ControllerGlobal::Clip:
            break;
        case ControllerGlobal::NoteWithParams:
            copyCutNoteWithParams(isCut);
            break;
        case ControllerGlobal::None:
            break;
    }
}

void ClipboardControllerPrivate::copyCutNoteWithParams(const bool isCut) {
    qDebug() << "ClipboardController::copyNoteWithParams isCut:" << isCut;
    if (isCut)
        clipController->cutSelectedNotesWithParams();
    else
        clipController->copySelectedNotesWithParams();
}