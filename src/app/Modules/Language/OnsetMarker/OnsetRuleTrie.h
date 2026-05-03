#ifndef ONSETRULETRIE_H
#define ONSETRULETRIE_H

#include <QHash>
#include <QList>
#include <QString>

#include <optional>

struct RuleTerm {
    QString key;
    bool isWildcard = false;
};

struct QueryTerm {
    QString specifiedKey;
    QString wildcardKey;
};

class OnsetRuleTrieNode {
public:
    ~OnsetRuleTrieNode();

    void insert(const QList<RuleTerm> &pattern, const QList<int> &onsets);
    QList<int> lookup(const QList<RuleTerm> &pattern) const;
    QList<RuleTerm> findBestPath(const QList<QueryTerm> &query) const;

private:
    QList<QList<RuleTerm>> findPaths(const QList<QueryTerm> &query, int offset) const;

    QHash<QString, OnsetRuleTrieNode *> m_children;
    QHash<QString, OnsetRuleTrieNode *> m_wildcards;
    std::optional<QList<int>> m_value;
};

#endif // ONSETRULETRIE_H
