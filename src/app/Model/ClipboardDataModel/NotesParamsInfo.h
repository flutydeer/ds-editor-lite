//
// Created by fluty on 24-2-18.
//

#ifndef NOTESPARAMSINFO_H
#define NOTESPARAMSINFO_H

#include "Model/AppModel/Note.h"

class NotesParamsInfo {
public:
    QList<Note *> selectedNotes{};
    // TODO: add params

    friend QDataStream &operator<<(QDataStream &out, const NotesParamsInfo &info);
    friend QDataStream &operator>>(QDataStream &in, NotesParamsInfo &info);

    static QByteArray serializeToBinary(const NotesParamsInfo &info);
    static NotesParamsInfo deserializeFromBinary(const QByteArray &byteArray);

    // static QJsonObject serializeToJson(const NotesParamsInfo &info);
    // static NotesParamsInfo deserializeFromJson(const QJsonObject &obj);
};



#endif // NOTESPARAMSINFO_H
