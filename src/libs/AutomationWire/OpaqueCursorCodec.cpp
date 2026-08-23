#include "OpaqueCursorCodec.h"

#include "CanonicalJson.h"
#include "JsonSchema.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QRegularExpression>

#include <cmath>
#include <cstring>
#include <utility>

namespace AutomationWire {
    namespace {
        constexpr int CursorVersion = 1;
        constexpr qsizetype SecretBytes = 32;
        constexpr qsizetype SignatureBytes = 32;
        constexpr qsizetype MaximumTokenCodeUnits = 8192;
        constexpr qsizetype MaximumContextCodeUnits = 1024;
        constexpr qsizetype MaximumSnapshotDigestCodeUnits = 1024;

        QByteArray randomSecret() {
            QByteArray result(SecretBytes, Qt::Uninitialized);
            for (qsizetype offset = 0; offset < result.size();
                 offset += static_cast<qsizetype>(sizeof(quint64))) {
                const auto value = QRandomGenerator::system()->generate64();
                std::memcpy(result.data() + offset, &value, sizeof(value));
            }
            return result;
        }

        QByteArray base64UrlEncode(const QByteArray &value) {
            return value.toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals);
        }

        std::optional<QByteArray> base64UrlDecode(const QByteArray &value) {
            static const QRegularExpression expression(QStringLiteral("^[A-Za-z0-9_-]+$"));
            if (value.isEmpty() || value.size() % 4 == 1 ||
                !expression.match(QString::fromLatin1(value)).hasMatch()) {
                return std::nullopt;
            }
            const auto decoded = QByteArray::fromBase64(
                value, QByteArray::Base64UrlEncoding | QByteArray::AbortOnBase64DecodingErrors);
            if (base64UrlEncode(decoded) != value)
                return std::nullopt;
            return decoded;
        }

        QByteArray signature(const QByteArray &payload, const QByteArray &secret) {
            return QMessageAuthenticationCode::hash(payload, secret, QCryptographicHash::Sha256);
        }

        bool constantTimeSignatureEqual(const QByteArray &left, const QByteArray &right) {
            if (left.size() != SignatureBytes || right.size() != SignatureBytes)
                return false;
            quint32 difference = 0;
            for (qsizetype index = 0; index < SignatureBytes; ++index) {
                difference |= static_cast<unsigned char>(left.at(index)) ^
                              static_cast<unsigned char>(right.at(index));
            }
            return difference == 0;
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

    OpaqueCursorCodec::OpaqueCursorCodec(const qint64 validityMilliseconds,
                                         EpochMillisecondsProvider nowProvider)
        : m_secret(randomSecret()),
          m_validityMilliseconds(validityMilliseconds),
          m_nowProvider(nowProvider ? std::move(nowProvider) : EpochMillisecondsProvider([] {
              return QDateTime::currentMSecsSinceEpoch();
          })) {
        if (m_validityMilliseconds <= 0 || m_validityMilliseconds > MaximumSafeJsonInteger)
            m_validityMilliseconds = 0;
    }

    QString OpaqueCursorCodec::issue(const QString &context, const QString &snapshotDigest,
                                     const qint64 offset) const {
        const auto now = m_nowProvider();
        if (!validBinding(context, MaximumContextCodeUnits) ||
            !validBinding(snapshotDigest, MaximumSnapshotDigestCodeUnits) || offset < 0 ||
            offset > MaximumSafeJsonInteger || now < 0 || m_validityMilliseconds <= 0 ||
            now > MaximumSafeJsonInteger - m_validityMilliseconds) {
            return {};
        }
        const auto expiresAtEpochMilliseconds = now + m_validityMilliseconds;

        const QJsonObject payload{
            {QStringLiteral("context"),             context                     },
            {QStringLiteral("expires_at_epoch_ms"), expiresAtEpochMilliseconds },
            {QStringLiteral("offset"),              offset                      },
            {QStringLiteral("snapshot_digest"),     snapshotDigest              },
            {QStringLiteral("version"),             CursorVersion               },
        };
        QString canonicalError;
        const auto encodedPayload = base64UrlEncode(canonicalJson(payload, &canonicalError));
        if (!canonicalError.isEmpty())
            return {};
        const auto encodedSignature = base64UrlEncode(signature(encodedPayload, m_secret));
        return QString::fromLatin1(encodedPayload + '.' + encodedSignature);
    }

    OpaqueCursorParseResult OpaqueCursorCodec::parse(const QString &token,
                                                     const QString &expectedContext,
                                                     const QString &expectedSnapshotDigest) const {
        if (token.isEmpty() || token.size() > MaximumTokenCodeUnits ||
            !validBinding(expectedContext, MaximumContextCodeUnits) ||
            !validBinding(expectedSnapshotDigest, MaximumSnapshotDigestCodeUnits)) {
            return failure(OpaqueCursorError::Malformed);
        }
        const auto encodedToken = token.toLatin1();
        if (QString::fromLatin1(encodedToken) != token)
            return failure(OpaqueCursorError::Malformed);
        const auto separator = encodedToken.indexOf('.');
        if (separator <= 0 || separator != encodedToken.lastIndexOf('.') ||
            separator == encodedToken.size() - 1) {
            return failure(OpaqueCursorError::Malformed);
        }

        const auto encodedPayload = encodedToken.first(separator);
        const auto encodedSignature = encodedToken.sliced(separator + 1);
        const auto decodedSignature = base64UrlDecode(encodedSignature);
        if (!decodedSignature)
            return failure(OpaqueCursorError::Malformed);
        const auto expectedSignature = signature(encodedPayload, m_secret);
        if (!constantTimeSignatureEqual(*decodedSignature, expectedSignature))
            return failure(OpaqueCursorError::InvalidSignature);

        const auto decodedPayload = base64UrlDecode(encodedPayload);
        if (!decodedPayload)
            return failure(OpaqueCursorError::Malformed);
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(*decodedPayload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            return failure(OpaqueCursorError::Malformed);
        const auto payload = document.object();
        QString canonicalError;
        if (payload.size() != 5 || canonicalJson(payload, &canonicalError) != *decodedPayload ||
            !canonicalError.isEmpty() || !payload.value(QStringLiteral("context")).isString() ||
            !payload.value(QStringLiteral("snapshot_digest")).isString()) {
            return failure(OpaqueCursorError::Malformed);
        }

        const auto version = payload.value(QStringLiteral("version"));
        if (!isSafeInteger(version) || version.toInteger() != CursorVersion)
            return failure(OpaqueCursorError::UnsupportedVersion);
        const auto offset = payload.value(QStringLiteral("offset"));
        if (!isSafeInteger(offset) || offset.toDouble() < 0.0)
            return failure(OpaqueCursorError::InvalidOffset);
        const auto expiry = payload.value(QStringLiteral("expires_at_epoch_ms"));
        if (!isSafeInteger(expiry) || expiry.toDouble() < 0.0)
            return failure(OpaqueCursorError::Malformed);

        if (payload.value(QStringLiteral("context")).toString() != expectedContext)
            return failure(OpaqueCursorError::ContextMismatch);
        if (payload.value(QStringLiteral("snapshot_digest")).toString() != expectedSnapshotDigest) {
            return failure(OpaqueCursorError::SnapshotMismatch);
        }
        const auto now = m_nowProvider();
        if (now < 0 || now > MaximumSafeJsonInteger || now >= expiry.toInteger()) {
            return failure(OpaqueCursorError::Expired);
        }
        return {.offset = offset.toInteger()};
    }

}
