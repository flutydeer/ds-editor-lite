#ifndef LYRICRULEAUTOMATIONUTILS_H
#define LYRICRULEAUTOMATIONUTILS_H

#include <QString>

namespace FillLyric {

    struct AutomationRegexValidationResult {
        bool valid = false;
        QString error;
        int captureCount = 0;
    };

    [[nodiscard]] QString builtinAutomationRuleId(const QString &kind, const QString &key);
    [[nodiscard]] QString createAutomationRuleId();
    [[nodiscard]] bool isAutomationRuleId(const QString &ruleId);
    [[nodiscard]] AutomationRegexValidationResult validateAutomationRegex(const QString &pattern);

} // namespace FillLyric

#endif // LYRICRULEAUTOMATIONUTILS_H
