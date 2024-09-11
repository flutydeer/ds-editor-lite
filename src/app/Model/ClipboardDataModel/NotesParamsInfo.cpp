//
// Created by fluty on 24-2-18.
//

#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>

#include "NotesParamsInfo.h"

// QJsonObject NotesParamsInfo::serializeToJson(const NotesParamsInfo &info) {
//     QJsonObject obj;
//
//     QJsonArray noteList;
//     for (const auto &note : info.selectedNotes)
//         noteList.append(Note::serialize(note));
//     obj.insert("notes", noteList);
//
//     return obj;
// }
// NotesParamsInfo NotesParamsInfo::deserializeFromJson(const QJsonObject &obj) {
//     NotesParamsInfo info;
//
//     auto arrNotes = obj.value("notes").toArray();
//     for (const auto &valNote : std::as_const(arrNotes)) {
//         auto objNote = valNote.toObject();
//         info.selectedNotes.append(Note::deserialize(objNote));
//     }
//
//     return info;
// }