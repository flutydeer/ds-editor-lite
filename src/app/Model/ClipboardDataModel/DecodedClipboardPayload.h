#ifndef DECODEDCLIPBOARDPAYLOAD_H
#define DECODEDCLIPBOARDPAYLOAD_H

#include "ClipsInfo.h"
#include "NotesParamsInfo.h"

#include <QJsonObject>

class DecodedClipsPayload final {
public:
    explicit DecodedClipsPayload(const QJsonObject &object);
    ~DecodedClipsPayload();

    Q_DISABLE_COPY_MOVE(DecodedClipsPayload)

    [[nodiscard]] const ClipsInfo &info() const;
    [[nodiscard]] bool isEmpty() const;

private:
    ClipsInfo m_info;
};

class DecodedNotesPayload final {
public:
    explicit DecodedNotesPayload(const QJsonObject &object);
    ~DecodedNotesPayload();

    Q_DISABLE_COPY_MOVE(DecodedNotesPayload)

    [[nodiscard]] const NotesParamsInfo &info() const;
    [[nodiscard]] bool isEmpty() const;

private:
    NotesParamsInfo m_info;
};

#endif // DECODEDCLIPBOARDPAYLOAD_H
