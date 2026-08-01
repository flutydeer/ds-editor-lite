#ifndef NOTESPARAMSINFO_H
#define NOTESPARAMSINFO_H

#include <lite/ProjectModel/AppModel/Note.h>

#include <QJsonArray>
#include <QJsonObject>

class NotesParamsInfo {
public:
    QList<Note *> selectedNotes{};

    static QJsonObject serializeToJson(const NotesParamsInfo &info);
    static NotesParamsInfo deserializeFromJson(const QJsonObject &obj);
};

#endif // NOTESPARAMSINFO_H
