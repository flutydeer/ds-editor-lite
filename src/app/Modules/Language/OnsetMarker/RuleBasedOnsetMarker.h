#ifndef RULEBASEDONSETMARKER_H
#define RULEBASEDONSETMARKER_H

#include "IOnsetMarker.h"
#include "OnsetRuleTrie.h"
#include "PhonemeTypeMap.h"

class RuleBasedOnsetMarker final : public IOnsetMarker {
public:
    bool loadConfig(const QString &jsonPath);
    QList<PhonemeName> mark(const QStringList &phonemeNames,
                            const QString &language) const override;

private:
    PhonemeTypeMap m_typeMap;
    OnsetRuleTrieNode m_trie;
};

#endif // RULEBASEDONSETMARKER_H
