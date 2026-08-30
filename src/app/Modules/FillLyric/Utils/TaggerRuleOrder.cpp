#include "TaggerRuleOrder.h"

#include <optional>
#include <vector>

namespace FillLyric {
    namespace {
        constexpr QLatin1StringView BUILTIN_PREFIX("builtin:");
        constexpr QLatin1StringView CUSTOM_PREFIX("custom:");

        std::optional<TaggerRuleIdentity> decodeStableKey(const QString &key) {
            if (key.startsWith(BUILTIN_PREFIX)) {
                return TaggerRuleIdentity{
                    .language = key.sliced(BUILTIN_PREFIX.size()),
                    .builtin = true,
                };
            }
            if (key.startsWith(CUSTOM_PREFIX)) {
                return TaggerRuleIdentity{
                    .language = key.sliced(CUSTOM_PREFIX.size()),
                    .builtin = false,
                };
            }
            return std::nullopt;
        }
    }

    QString TaggerRuleOrder::key(const TaggerRuleIdentity &identity) {
        return (identity.builtin ? BUILTIN_PREFIX : CUSTOM_PREFIX) + identity.language;
    }

    QList<int> TaggerRuleOrder::resolve(const QStringList &order,
                                        const QList<TaggerRuleIdentity> &rules) {
        QList<int> result;
        result.reserve(rules.size());
        std::vector<bool> consumed(static_cast<size_t>(rules.size()), false);

        // Legacy language-only entries consume same-language rules once so both sources survive
        // migration.
        for (const auto &savedKey : order) {
            const auto stableIdentity = decodeStableKey(savedKey);
            for (qsizetype index = 0; index < rules.size(); ++index) {
                if (consumed[static_cast<size_t>(index)])
                    continue;
                const auto &rule = rules.at(index);
                const bool matches =
                    stableIdentity ? rule == *stableIdentity : rule.language == savedKey;
                if (!matches)
                    continue;
                result.append(static_cast<int>(index));
                consumed[static_cast<size_t>(index)] = true;
                break;
            }
        }

        for (qsizetype index = 0; index < rules.size(); ++index) {
            if (!consumed[static_cast<size_t>(index)])
                result.append(static_cast<int>(index));
        }
        return result;
    }

    QStringList TaggerRuleOrder::canonicalize(const QStringList &order,
                                              const QList<TaggerRuleIdentity> &rules) {
        QStringList result;
        result.reserve(rules.size());
        for (const int index : resolve(order, rules))
            result.append(key(rules.at(index)));
        return result;
    }
}
