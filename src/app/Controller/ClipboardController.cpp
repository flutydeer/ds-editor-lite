//
// Created by fluty on 24-2-17.
//

#include "ClipboardController.h"

#include "ClipEditorViewController.h"
#include "PlaybackController.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QJsonDocument>
#include <QMimeData>

// TODO: 要求传入类型
void ClipboardController::copy() {
    qDebug() << "ClipboardController::copy";
    copyCutSelectedItems(ControllerGlobal::NoteWithParams, false);
}
void ClipboardController::cut() {
    copyCutSelectedItems(ControllerGlobal::NoteWithParams, true);
}
void ClipboardController::paste() {
    qDebug() << "ClipboardController::paste";
    auto mimeData = QGuiApplication::clipboard()->mimeData();
    if (mimeData->hasFormat(ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams))) {
        qDebug() << "Mime data has NoteWithParams";
        auto array = mimeData->data(ControllerGlobal::ElemMimeType.at(ControllerGlobal::NoteWithParams));
        auto info = NotesParamsInfo::deserializeFromBinary(array);
        // auto json = QJsonDocument::fromJson(array);
        // auto info = NotesParamsInfo::deserializeFromJson(json.object());
        auto tick = PlaybackController::instance()->position();
        ClipEditorViewController::instance()->pasteNotesWithParams(info, static_cast<int>(tick));
    }
}
void ClipboardController::copyCutSelectedItems(ControllerGlobal::ElemType type, bool isCut) {
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
void ClipboardController::copyCutNoteWithParams(bool isCut) {
    qDebug() << "ClipboardController::copyNoteWithParams isCut:" << isCut;
    if (isCut)
        ClipEditorViewController::instance()->cutSelectedNotesWithParams();
    else
        ClipEditorViewController::instance()->copySelectedNotesWithParams();
}