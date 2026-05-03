#include "OnsetRuleTrie.h"

OnsetRuleTrieNode::~OnsetRuleTrieNode() {
    qDeleteAll(m_children);
    qDeleteAll(m_wildcards);
}

void OnsetRuleTrieNode::insert(const QList<RuleTerm> &pattern, const QList<int> &onsets) {
    if (pattern.isEmpty()) {
        m_value = onsets;
        return;
    }

    const auto &term = pattern.first();
    auto rest = pattern.mid(1);

    if (term.isWildcard) {
        if (!m_wildcards.contains(term.key))
            m_wildcards.insert(term.key, new OnsetRuleTrieNode);
        m_wildcards[term.key]->insert(rest, onsets);
    } else {
        if (!m_children.contains(term.key))
            m_children.insert(term.key, new OnsetRuleTrieNode);
        m_children[term.key]->insert(rest, onsets);
    }
}

QList<int> OnsetRuleTrieNode::lookup(const QList<RuleTerm> &pattern) const {
    if (pattern.isEmpty())
        return m_value.value_or(QList<int>());

    const auto &term = pattern.first();
    auto rest = pattern.mid(1);

    if (term.isWildcard) {
        auto it = m_wildcards.find(term.key);
        if (it == m_wildcards.end())
            return {};
        return (*it)->lookup(rest);
    }
    auto it = m_children.find(term.key);
    if (it == m_children.end())
        return {};
    return (*it)->lookup(rest);
}

QList<QList<RuleTerm>> OnsetRuleTrieNode::findPaths(const QList<QueryTerm> &query,
                                                     int offset) const {
    if (offset >= query.size())
        return {};

    const auto &term = query[offset];
    QList<QList<RuleTerm>> paths;

    if (auto it = m_children.find(term.specifiedKey); it != m_children.end()) {
        if ((*it)->m_value.has_value())
            paths.append({RuleTerm{term.specifiedKey, false}});
        for (auto &subpath : (*it)->findPaths(query, offset + 1)) {
            subpath.prepend(RuleTerm{term.specifiedKey, false});
            paths.append(subpath);
        }
    }

    if (!term.wildcardKey.isEmpty()) {
        if (auto it = m_wildcards.find(term.wildcardKey); it != m_wildcards.end()) {
            if ((*it)->m_value.has_value())
                paths.append({RuleTerm{term.wildcardKey, true}});
            for (auto &subpath : (*it)->findPaths(query, offset + 1)) {
                subpath.prepend(RuleTerm{term.wildcardKey, true});
                paths.append(subpath);
            }
        }
    }

    return paths;
}

QList<RuleTerm> OnsetRuleTrieNode::findBestPath(const QList<QueryTerm> &query) const {
    auto paths = findPaths(query, 0);
    if (paths.isEmpty())
        return {};

    QList<RuleTerm> best;
    int bestLen = -1;
    int bestExact = -1;
    int bestFirstExact = INT_MAX;

    for (const auto &path : paths) {
        int len = path.size();
        int exact = 0;
        int firstExact = INT_MAX;
        for (int i = 0; i < path.size(); i++) {
            if (!path[i].isWildcard) {
                exact++;
                if (i < firstExact)
                    firstExact = i;
            }
        }

        bool isBetter = false;
        if (len > bestLen)
            isBetter = true;
        else if (len == bestLen && exact > bestExact)
            isBetter = true;
        else if (len == bestLen && exact == bestExact && firstExact < bestFirstExact)
            isBetter = true;

        if (isBetter) {
            best = path;
            bestLen = len;
            bestExact = exact;
            bestFirstExact = firstExact;
        }
    }

    return best;
}
