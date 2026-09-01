#include <lite/AutomationWire/JsonSchema.h>
#include <lite/AutomationWire/OpaqueCursorCodec.h>

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

namespace {
    using namespace AutomationWire;

    bool expect(const bool condition, const QString &message) {
        if (condition)
            return true;
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }

    QJsonObject decodePayload(const QString &token) {
        return QJsonDocument::fromJson(
                   QByteArray::fromBase64(token.toLatin1(),
                                          QByteArray::Base64UrlEncoding |
                                              QByteArray::AbortOnBase64DecodingErrors))
            .object();
    }

    QString encodePayload(const QJsonObject &payload) {
        return QString::fromLatin1(
            QJsonDocument(payload)
                .toJson(QJsonDocument::Compact)
                .toBase64(QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
    }

    bool testOpaqueCursorCodec() {
        bool ok = true;
        OpaqueCursorCodec codec;

        const auto token =
            codec.issue(QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"), 37);
        const auto repeated =
            codec.issue(QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"), 37);
        const auto parsed =
            codec.parse(token, QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"));
        ok &= expect(!token.isEmpty() && token == repeated && parsed.valid() &&
                         *parsed.offset == 37 && !token.contains(u'=') && !token.contains(u'+') &&
                         !token.contains(u'/'),
                     QStringLiteral("cursor state must round-trip in one stable URL-safe token"));

        const auto payload = decodePayload(token);
        ok &=
            expect(payload.size() == 4 &&
                       payload.value(QStringLiteral("context")) == QStringLiteral("manifest:l2") &&
                       payload.value(QStringLiteral("snapshot_digest")) ==
                           QStringLiteral("sha256:snapshot-a") &&
                       payload.value(QStringLiteral("offset")).toInteger() == 37 &&
                       payload.value(QStringLiteral("version")).toInteger() == 1,
                   QStringLiteral("cursor must carry only versioned paging state"));

        const auto wrongContext =
            codec.parse(token, QStringLiteral("tasks:list"), QStringLiteral("sha256:snapshot-a"));
        const auto wrongSnapshot =
            codec.parse(token, QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-b"));
        ok &= expect(wrongContext.error == OpaqueCursorError::ContextMismatch &&
                         wrongSnapshot.error == OpaqueCursorError::SnapshotMismatch,
                     QStringLiteral("context and snapshot changes must invalidate a cursor"));

        auto advancedPayload = payload;
        advancedPayload.insert(QStringLiteral("offset"), 51);
        const auto advanced =
            codec.parse(encodePayload(advancedPayload), QStringLiteral("manifest:l2"),
                        QStringLiteral("sha256:snapshot-a"));
        ok &= expect(advanced.valid() && *advanced.offset == 51,
                     QStringLiteral("cursor offsets are paging state, not authenticated data"));

        ok &= expect(
            codec.issue(QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"), -1)
                    .isEmpty() &&
                codec
                    .issue(QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"),
                           MaximumSafeJsonInteger + 1)
                    .isEmpty(),
            QStringLiteral("negative and non-safe offsets must never be issued"));
        const auto maximumOffsetToken =
            codec.issue(QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"),
                        MaximumSafeJsonInteger);
        const auto maximumOffset = codec.parse(maximumOffsetToken, QStringLiteral("manifest:l2"),
                                               QStringLiteral("sha256:snapshot-a"));
        ok &= expect(maximumOffset.valid() && *maximumOffset.offset == MaximumSafeJsonInteger,
                     QStringLiteral("maximum safe offset must round-trip exactly"));

        for (const auto &malformed :
             {QString(), QStringLiteral("abc"), QStringLiteral("a.b.c"), QStringLiteral("*")}) {
            ok &= expect(!codec
                              .parse(malformed, QStringLiteral("manifest:l2"),
                                     QStringLiteral("sha256:snapshot-a"))
                              .valid(),
                         QStringLiteral("malformed cursor must be rejected"));
        }
        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    const auto ok = testOpaqueCursorCodec();
    if (ok)
        QTextStream(stdout) << "Validated automation cursors" << Qt::endl;
    return ok ? 0 : 1;
}
