//
// Created by fluty on 24-2-17.
//

#include "ClipboardController.h"
#include "ClipboardController_p.h"

#include "ClipEditorViewController.h"
#include "PlaybackController.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QMimeData>

ClipboardController::ClipboardController() : d_ptr(new ClipboardControllerPrivate(this)) {
}

// TODO: 要求传入类型
void ClipboardController::copy() {
    Q_D(ClipboardController);
    qDebug() << "ClipboardController::copy";
    d->copyCutSelectedItems(ControllerGlobal::NoteWithParams, false);
}

void ClipboardController::cut() {
    Q_D(ClipboardController);
    d->copyCutSelectedItems(ControllerGlobal::NoteWithParams, true);
}

void ClipboardController::paste() {
    qDebug() << "ClipboardController::paste";
    auto mimeData = QGuiApplication::clipboard()->mimeData();
    if (mimeData->hasFormat(ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams))) {
        qDebug() << "Mime data has NoteWithParams";
        auto array =
            mimeData->data(ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams));
        auto info = NotesParamsInfo::deserializeFromBinary(array);
        // auto json = QJsonDocument::fromJson(array);
        // auto info = NotesParamsInfo::deserializeFromJson(json.object());
        auto tick = playbackController->position();
        clipController->pasteNotesWithParams(info, static_cast<int>(tick));
    }
}

void ClipboardControllerPrivate::copyCutSelectedItems(ControllerGlobal::ElemType type, bool isCut) {
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

void ClipboardControllerPrivate::copyCutNoteWithParams(bool isCut) {
    qDebug() << "ClipboardController::copyNoteWithParams isCut:" << isCut;
    if (isCut)
        clipController->cutSelectedNotesWithParams();
    else
        clipController->copySelectedNotesWithParams();
}