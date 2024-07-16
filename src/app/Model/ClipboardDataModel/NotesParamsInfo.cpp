//
// Created by fluty on 24-2-18.
//

#include <QIODevice>
#include <QJsonArray>
#include <QJsonObject>

#include "NotesParamsInfo.h"

QByteArray NotesParamsInfo::serializeToBinary(const NotesParamsInfo &info) {
    QByteArray byteArray;
    QDataStream stream(&byteArray, QIODevice::WriteOnly);
    stream << info;
    return byteArray;
}
NotesParamsInfo NotesParamsInfo::deserializeFromBinary(const QByteArray &byteArray) {
    NotesParamsInfo obj;
    QDataStream stream(byteArray);
    stream >> obj;
    return obj;
}
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
QDataStream &operator<<(QDataStream &out, const NotesParamsInfo &info) {
    // Serialize Notes
    auto noteListCount = info.selectedNotes.count();
    out << noteListCount;
    for (const auto &note : info.selectedNotes)
        out << *note;
    return out;
}
QDataStream &operator>>(QDataStream &in, NotesParamsInfo &info) {
    qsizetype noteListCount;
    in >> noteListCount;
    for (int i = 0; i < noteListCount; i++) {
        auto note = new Note;
        in >> *note;
        info.selectedNotes.append(note);
    }
    return in;
}