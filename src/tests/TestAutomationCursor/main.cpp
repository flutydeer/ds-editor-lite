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

    QByteArray decodePayload(const QString &token) {
        const auto bytes = token.toLatin1();
        const auto encoded = bytes.first(bytes.indexOf('.'));
        return QByteArray::fromBase64(encoded, QByteArray::Base64UrlEncoding |
                                                   QByteArray::AbortOnBase64DecodingErrors);
    }

    QString tamperSegment(QString token, const int segment) {
        auto encoded = token.toLatin1();
        const auto separator = encoded.indexOf('.');
        const auto index = segment == 0 ? 0 : separator + 1;
        encoded[index] = encoded.at(index) == 'A' ? 'B' : 'A';
        return QString::fromLatin1(encoded);
    }

    bool testOpaqueCursorCodec() {
        bool ok = true;
        constexpr qint64 InitialTime = 1'700'000'000'000LL;
        constexpr qint64 Validity = 60'000;
        qint64 now = InitialTime;
        OpaqueCursorCodec codec(Validity, [&now] { return now; });

        const auto token =
            codec.issue(QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"), 37);
        const auto repeated =
            codec.issue(QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"), 37);
        const auto parsed =
            codec.parse(token, QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"));
        ok &= expect(!token.isEmpty() && token == repeated && parsed.valid() &&
                         *parsed.offset == 37 && !token.contains(u'=') && !token.contains(u'+') &&
                         !token.contains(u'/'),
                     QStringLiteral("same codec and state must issue one stable valid token"));

        const auto payload = QJsonDocument::fromJson(decodePayload(token)).object();
        ok &=
            expect(payload.size() == 5 &&
                       payload.value(QStringLiteral("context")) == QStringLiteral("manifest:l2") &&
                       payload.value(QStringLiteral("snapshot_digest")) ==
                           QStringLiteral("sha256:snapshot-a") &&
                       payload.value(QStringLiteral("offset")).toInteger() == 37 &&
                       payload.value(QStringLiteral("expires_at_epoch_ms")).toInteger() ==
                           InitialTime + Validity &&
                       payload.value(QStringLiteral("version")).toInteger() == 1,
                   QStringLiteral("cursor payload must bind all versioned paging state"));

        now += 1'000;
        const auto refreshedToken =
            codec.issue(QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"), 37);
        ok &= expect(refreshedToken != token &&
                         codec.parse(refreshedToken, QStringLiteral("manifest:l2"),
                                     QStringLiteral("sha256:snapshot-a"))
                             .valid(),
                     QStringLiteral("each issuance must refresh the token expiry"));
        ok &= expect(codec.issue(QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"),
                                 38) != token,
                     QStringLiteral("offset must be authenticated into the cursor"));

        const auto wrongContext =
            codec.parse(token, QStringLiteral("tasks:list"), QStringLiteral("sha256:snapshot-a"));
        const auto wrongSnapshot =
            codec.parse(token, QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-b"));
        ok &= expect(wrongContext.error == OpaqueCursorError::ContextMismatch &&
                         wrongSnapshot.error == OpaqueCursorError::SnapshotMismatch,
                     QStringLiteral("context and snapshot changes must invalidate a cursor"));

        ok &= expect(codec.parse(tamperSegment(token, 0), QStringLiteral("manifest:l2"),
                                 QStringLiteral("sha256:snapshot-a"))
                                 .error == OpaqueCursorError::InvalidSignature &&
                         codec.parse(tamperSegment(token, 1), QStringLiteral("manifest:l2"),
                                     QStringLiteral("sha256:snapshot-a"))
                                 .error == OpaqueCursorError::InvalidSignature,
                     QStringLiteral("payload and signature forgery must be rejected"));

        OpaqueCursorCodec otherCodec(Validity, [&now] { return now; });
        const auto otherToken = otherCodec.issue(QStringLiteral("manifest:l2"),
                                                 QStringLiteral("sha256:snapshot-a"), 37);
        ok &= expect(otherToken != token && otherCodec
                                                    .parse(token, QStringLiteral("manifest:l2"),
                                                           QStringLiteral("sha256:snapshot-a"))
                                                    .error == OpaqueCursorError::InvalidSignature,
                     QStringLiteral("each codec instance must have an independent random key"));

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

        for (const auto &malformed : {QString(), QStringLiteral("abc"), QStringLiteral("a.b.c"),
                                      QStringLiteral("*.AAAA"), QStringLiteral("AAAA.*")}) {
            ok &= expect(!codec
                              .parse(malformed, QStringLiteral("manifest:l2"),
                                     QStringLiteral("sha256:snapshot-a"))
                              .valid(),
                         QStringLiteral("malformed cursor must be rejected"));
        }

        now = InitialTime + Validity;
        ok &= expect(
            codec.parse(token, QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"))
                        .error == OpaqueCursorError::Expired &&
                !codec.issue(QStringLiteral("manifest:l2"), QStringLiteral("sha256:snapshot-a"), 37)
                     .isEmpty(),
            QStringLiteral("an expired token must not prevent issuing a fresh cursor"));
        return ok;
    }
}

int main(int argc, char *argv[]) {
    QCoreApplication application(argc, argv);
    const auto ok = testOpaqueCursorCodec();
    if (ok)
        QTextStream(stdout) << "Validated opaque automation cursors" << Qt::endl;
    return ok ? 0 : 1;
}
