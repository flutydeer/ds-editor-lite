#include "OpaqueCursorCodec.h"

#include "JsonSchema.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cmath>

namespace AutomationWire {
    namespace {
        constexpr int CursorVersion = 1;
        constexpr qsizetype MaximumTokenCodeUnits = 8192;
        constexpr qsizetype MaximumContextCodeUnits = 1024;
        constexpr qsizetype MaximumSnapshotDigestCodeUnits = 1024;

        QByteArray encode(const QByteArray &value) {
            return value.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        }

        std::optional<QByteArray> decode(const QByteArray &value) {
            if (value.isEmpty() || value.size() % 4 == 1)
                return std::nullopt;
            const auto decoded = QByteArray::fromBase64(
                value, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
            if (decoded.isEmpty() || encode(decoded) != value)
                return std::nullopt;
            return decoded;
        }

        bool isSafeInteger(const QJsonValue &value) {
            return value.isDouble() && std::isfinite(value.toDouble()) &&
                   std::trunc(value.toDouble()) == value.toDouble() &&
                   std::abs(value.toDouble()) <= static_cast<double>(MaximumSafeJsonInteger);
        }

        bool validBinding(const QString &value, const qsizetype maximumCodeUnits) {
            return !value.isEmpty() && value.size() <= maximumCodeUnits;
        }

        OpaqueCursorParseResult failure(const OpaqueCursorError error) {
            return {.error = error};
        }
    }

    QString OpaqueCursorCodec::issue(const QString &context, const QString &snapshotDigest,
                                     const qint64 offset) const {
        if (!validBinding(context, MaximumContextCodeUnits) ||
            !validBinding(snapshotDigest, MaximumSnapshotDigestCodeUnits) || offset < 0 ||
            offset > MaximumSafeJsonInteger) {
            return {};
        }
        const QJsonObject payload{
            {QStringLiteral("context"),         context       },
            {QStringLiteral("offset"),          offset        },
            {QStringLiteral("snapshot_digest"), snapshotDigest},
            {QStringLiteral("version"),         CursorVersion },
        };
        return QString::fromLatin1(encode(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
    }

    OpaqueCursorParseResult OpaqueCursorCodec::parse(const QString &token,
                                                     const QString &expectedContext,
                                                     const QString &expectedSnapshotDigest) const {
        if (token.isEmpty() || token.size() > MaximumTokenCodeUnits ||
            !validBinding(expectedContext, MaximumContextCodeUnits) ||
            !validBinding(expectedSnapshotDigest, MaximumSnapshotDigestCodeUnits)) {
            return failure(OpaqueCursorError::Malformed);
        }
        const auto encoded = token.toLatin1();
        if (QString::fromLatin1(encoded) != token)
            return failure(OpaqueCursorError::Malformed);
        const auto decoded = decode(encoded);
        if (!decoded)
            return failure(OpaqueCursorError::Malformed);

        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(*decoded, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            return failure(OpaqueCursorError::Malformed);
        const auto payload = document.object();
        if (payload.size() != 4 || !payload.value(QStringLiteral("context")).isString() ||
            !payload.value(QStringLiteral("snapshot_digest")).isString()) {
            return failure(OpaqueCursorError::Malformed);
        }

        const auto version = payload.value(QStringLiteral("version"));
        if (!isSafeInteger(version) || version.toInteger() != CursorVersion)
            return failure(OpaqueCursorError::UnsupportedVersion);
        const auto offset = payload.value(QStringLiteral("offset"));
        if (!isSafeInteger(offset) || offset.toDouble() < 0.0)
            return failure(OpaqueCursorError::InvalidOffset);
        if (payload.value(QStringLiteral("context")).toString() != expectedContext)
            return failure(OpaqueCursorError::ContextMismatch);
        if (payload.value(QStringLiteral("snapshot_digest")).toString() != expectedSnapshotDigest) {
            return failure(OpaqueCursorError::SnapshotMismatch);
        }
        return {.offset = offset.toInteger()};
    }
}
