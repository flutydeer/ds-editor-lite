#include "LyricRuleAutomationUtils.h"

#include <QCryptographicHash>
#include <QUuid>

#include <re2/re2.h>

namespace FillLyric {

    QString builtinAutomationRuleId(const QString &kind, const QString &key) {
        const auto digest = QCryptographicHash::hash(
            kind.toUtf8() + QByteArrayLiteral("\0") + key.toUtf8(), QCryptographicHash::Sha256);
        return QStringLiteral("builtin-%1-%2")
            .arg(kind, QString::fromLatin1(digest.toHex().left(24)));
    }

    QString createAutomationRuleId() {
        return QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    bool isAutomationRuleId(const QString &ruleId) {
        if (ruleId.startsWith(QStringLiteral("builtin-splitter-")) ||
            ruleId.startsWith(QStringLiteral("builtin-tagger-"))) {
            return ruleId.size() > 24;
        }
        return !QUuid::fromString(ruleId).isNull();
    }

    AutomationRegexValidationResult validateAutomationRegex(const QString &pattern) {
        if (pattern.isEmpty())
            return {.valid = false, .error = QStringLiteral("Regular expression is empty")};
        RE2::Options options;
        options.set_encoding(RE2::Options::EncodingUTF8);
        options.set_log_errors(false);
        options.set_max_mem(8 << 20);
        const RE2 regex(pattern.toStdString(), options);
        if (!regex.ok()) {
            return {
                .valid = false,
                .error = QString::fromStdString(regex.error()),
            };
        }
        return {
            .valid = true,
            .captureCount = regex.NumberOfCapturingGroups(),
        };
    }

} // namespace FillLyric
