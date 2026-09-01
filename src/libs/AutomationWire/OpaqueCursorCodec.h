#ifndef AUTOMATIONWIRE_OPAQUECURSORCODEC_H
#define AUTOMATIONWIRE_OPAQUECURSORCODEC_H

#include <QString>

#include <optional>

namespace AutomationWire {

    enum class OpaqueCursorError {
        None,
        Malformed,
        UnsupportedVersion,
        InvalidOffset,
        ContextMismatch,
        SnapshotMismatch,
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
        QString issue(const QString &context, const QString &snapshotDigest, qint64 offset) const;
        OpaqueCursorParseResult parse(const QString &token, const QString &expectedContext,
                                      const QString &expectedSnapshotDigest) const;
    };

}

#endif // AUTOMATIONWIRE_OPAQUECURSORCODEC_H
