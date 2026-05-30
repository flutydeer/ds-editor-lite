//
// Created by fluty on 24-2-18.
//

#ifndef NOTESPARAMSINFO_H
#define NOTESPARAMSINFO_H

#include "Model/AppModel/Note.h"

#include <QJsonArray>
#include <QJsonObject>

class NotesParamsInfo {
public:
    QList<Note *> selectedNotes{};

    static QJsonObject serializeToJson(const NotesParamsInfo &info);
    static NotesParamsInfo deserializeFromJson(const QJsonObject &obj);
};

#endif // NOTESPARAMSINFO_H
