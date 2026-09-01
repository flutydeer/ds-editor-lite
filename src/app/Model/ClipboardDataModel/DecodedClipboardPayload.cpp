#include "DecodedClipboardPayload.h"

DecodedClipsPayload::DecodedClipsPayload(const QJsonObject &object)
    : m_info(ClipsInfo::deserializeFromJson(object)) {
}

DecodedClipsPayload::~DecodedClipsPayload() {
    qDeleteAll(m_info.clips);
}

const ClipsInfo &DecodedClipsPayload::info() const {
    return m_info;
}

bool DecodedClipsPayload::isEmpty() const {
    return m_info.clips.isEmpty();
}

DecodedNotesPayload::DecodedNotesPayload(const QJsonObject &object)
    : m_info(NotesParamsInfo::deserializeFromJson(object)) {
}

DecodedNotesPayload::~DecodedNotesPayload() = default;

const NotesParamsInfo &DecodedNotesPayload::info() const {
    return m_info;
}

bool DecodedNotesPayload::isEmpty() const {
    return m_info.payload.isEmpty();
}
