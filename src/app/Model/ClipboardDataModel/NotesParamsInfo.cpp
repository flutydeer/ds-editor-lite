//
// Created by fluty on 24-2-18.
//

#include "NotesParamsInfo.h"

#include <QJsonArray>

QJsonObject NotesParamsInfo::serializeToJson(const NotesParamsInfo &info) {
    QJsonArray noteList;
    for (const auto note : info.selectedNotes)
        noteList.append(note->serialize());

    return QJsonObject{{"notes", noteList}};
}

NotesParamsInfo NotesParamsInfo::deserializeFromJson(const QJsonObject &obj) {
    NotesParamsInfo info;
    const auto arrNotes = obj.value("notes").toArray();
    for (const auto &valNote : arrNotes) {
        auto note = new Note;
        note->deserialize(valNote.toObject());
        info.selectedNotes.append(note);
    }
    return info;
}