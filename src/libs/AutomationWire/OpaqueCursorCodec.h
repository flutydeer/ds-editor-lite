#ifndef AUTOMATIONWIRE_OPAQUECURSORCODEC_H
#define AUTOMATIONWIRE_OPAQUECURSORCODEC_H

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>

namespace AutomationWire {

    enum class OpaqueCursorError {
        None,
        Malformed,
        InvalidSignature,
        UnsupportedVersion,
        InvalidOffset,
        ContextMismatch,
        SnapshotMismatch,
        Expired,
    };

    struct OpaqueCursorParseResult {
        std::optional<qint64> offset;
        OpaqueCursorError error = OpaqueCursorError::None;

        bool valid() const {
            return offset.has_value();
        }
    };

    class OpaqueCursorCodec final {
    public:
        using EpochMillisecondsProvider = std::function<qint64()>;

        static constexpr qint64 DefaultValidityMilliseconds = 24 * 60 * 60 * 1000;

        explicit OpaqueCursorCodec(qint64 validityMilliseconds = DefaultValidityMilliseconds,
                                   EpochMillisecondsProvider nowProvider = {});

        OpaqueCursorCodec(const OpaqueCursorCodec &) = delete;
        OpaqueCursorCodec &operator=(const OpaqueCursorCodec &) = delete;
        OpaqueCursorCodec(OpaqueCursorCodec &&) = delete;
        OpaqueCursorCodec &operator=(OpaqueCursorCodec &&) = delete;

        QString issue(const QString &context, const QString &snapshotDigest, qint64 offset) const;
        OpaqueCursorParseResult parse(const QString &token, const QString &expectedContext,
                                      const QString &expectedSnapshotDigest) const;

    private:
        QByteArray m_secret;
        qint64 m_validityMilliseconds = 0;
        EpochMillisecondsProvider m_nowProvider;
    };

}

#endif // AUTOMATIONWIRE_OPAQUECURSORCODEC_H
