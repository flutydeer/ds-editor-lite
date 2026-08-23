#ifndef LYRIC_TAB_UTILS_TAGGER_RULE_ORDER_H
#define LYRIC_TAB_UTILS_TAGGER_RULE_ORDER_H

#include <QList>
#include <QString>
#include <QStringList>

namespace FillLyric {
    struct TaggerRuleIdentity {
        QString language;
        bool builtin = true;

        friend bool operator==(const TaggerRuleIdentity &, const TaggerRuleIdentity &) = default;
    };

    class TaggerRuleOrder final {
    public:
        [[nodiscard]] static QString key(const TaggerRuleIdentity &identity);
        [[nodiscard]] static QList<int> resolve(const QStringList &order,
                                                const QList<TaggerRuleIdentity> &rules);
        [[nodiscard]] static QStringList canonicalize(const QStringList &order,
                                                      const QList<TaggerRuleIdentity> &rules);
    };
}

#endif // LYRIC_TAB_UTILS_TAGGER_RULE_ORDER_H
