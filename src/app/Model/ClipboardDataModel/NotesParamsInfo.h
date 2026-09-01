#ifndef NOTESPARAMSINFO_H
#define NOTESPARAMSINFO_H

#include "Automation/NoteTransfer.h"

#include <QJsonArray>
#include <QJsonObject>

class NotesParamsInfo {
public:
    Automation::NoteTransferPayload payload;

    static QJsonObject serializeToJson(const NotesParamsInfo &info);
    static NotesParamsInfo deserializeFromJson(const QJsonObject &obj);
};

#endif // NOTESPARAMSINFO_H
