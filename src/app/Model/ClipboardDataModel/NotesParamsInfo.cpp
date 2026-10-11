#include "NotesParamsInfo.h"
#include "ParameterCurvesJson.h"

#include <lite/ProjectModel/AppModel/Note.h>

#include <QJsonArray>

#include <algorithm>
#include <limits>

QJsonObject NotesParamsInfo::serializeToJson(const NotesParamsInfo &info) {
    QJsonArray noteList;
    for (const auto &draft : info.payload.notes) {
        const auto note = Automation::buildNote(draft, nullptr);
        noteList.append(note->serialize());
    }

    const auto parameters = ClipboardDataModel::serializeParameters(info.payload.parameters);

    return QJsonObject{
        {QStringLiteral("schema_version"), 2                       },
        {QStringLiteral("source_start"),   info.payload.sourceStart},
        {QStringLiteral("source_end"),     info.payload.sourceEnd  },
        {QStringLiteral("notes"),          noteList                },
        {QStringLiteral("parameters"),     parameters              },
    };
}

NotesParamsInfo NotesParamsInfo::deserializeFromJson(const QJsonObject &obj) {
    NotesParamsInfo info;
    const auto arrNotes = obj.value(QStringLiteral("notes")).toArray();
    for (const auto &valNote : arrNotes) {
        Note note;
        if (!note.deserialize(valNote.toObject()))
            continue;
        auto draft = Automation::noteDraftDto(note);
        const auto noteEnd = static_cast<qint64>(draft.localStart) + draft.length;
        if (draft.localStart < 0 || draft.length <= 0 ||
            noteEnd > std::numeric_limits<int>::max()) {
            return {};
        }
        info.payload.notes.append(std::move(draft));
    }
    if (!info.payload.notes.isEmpty()) {
        info.payload.sourceStart = info.payload.notes.first().localStart;
        info.payload.sourceEnd = static_cast<int>(static_cast<qint64>(info.payload.sourceStart) +
                                                  info.payload.notes.first().length);
        for (const auto &note : info.payload.notes) {
            info.payload.sourceStart = std::min(info.payload.sourceStart, note.localStart);
            const auto noteEnd = static_cast<qint64>(note.localStart) + note.length;
            info.payload.sourceEnd = std::max(info.payload.sourceEnd, static_cast<int>(noteEnd));
        }
    }

    if (obj.value(QStringLiteral("schema_version")).toInt() < 2)
        return info;

    const auto serializedStart = obj.value(QStringLiteral("source_start")).toInt();
    const auto serializedEnd = obj.value(QStringLiteral("source_end")).toInt();
    if (serializedStart == info.payload.sourceStart && serializedEnd == info.payload.sourceEnd) {
        auto parameters = ClipboardDataModel::deserializeParameters(
            obj.value(QStringLiteral("parameters")).toArray());
        for (auto &parameter : parameters) {
            if (parameter.type == Param::Edited || parameter.type == Param::Envelope)
                info.payload.parameters.append(std::move(parameter));
        }
    }
    return info;
}
