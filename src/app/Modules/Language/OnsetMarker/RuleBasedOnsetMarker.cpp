#include "RuleBasedOnsetMarker.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

bool RuleBasedOnsetMarker::loadConfig(const QString &jsonPath) {
    QFile file(jsonPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qCritical() << "RuleBasedOnsetMarker: cannot open config:" << jsonPath;
        return false;
    }

    const auto doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        qCritical() << "RuleBasedOnsetMarker: invalid JSON in:" << jsonPath;
        return false;
    }

    const auto root = doc.object();

    if (!m_typeMap.load(root["phonemeTypes"].toObject())) {
        qCritical() << "RuleBasedOnsetMarker: failed to load phonemeTypes";
        return false;
    }

    const auto rulesArray = root["rules"].toArray();
    for (const auto &ruleVal : rulesArray) {
        const auto ruleObj = ruleVal.toObject();
        const auto patternArray = ruleObj["pattern"].toArray();
        const auto onsetsArray = ruleObj["onsets"].toArray();

        QList<RuleTerm> pattern;
        for (const auto &p : patternArray)
            pattern.append({p.toString(), true});

        QList<int> onsets;
        for (const auto &o : onsetsArray)
            onsets.append(o.toInt());

        m_trie.insert(pattern, onsets);
    }

    qInfo() << "RuleBasedOnsetMarker: loaded config from" << jsonPath
            << "with" << rulesArray.size() << "rules";
    return true;
}

QList<PhonemeName> RuleBasedOnsetMarker::mark(const QStringList &phonemeNames,
                                               const QString &language) const {
    QList<QueryTerm> query;
    query.reserve(phonemeNames.size());
    for (const auto &name : phonemeNames)
        query.append({name, m_typeMap.type(name)});

    QList<bool> isOnset(phonemeNames.size(), false);

    int i = 0;
    while (i < query.size()) {
        auto subQuery = query.mid(i);
        auto bestPath = m_trie.findBestPath(subQuery);
        if (bestPath.isEmpty()) {
            i++;
            continue;
        }

        auto onsets = m_trie.lookup(bestPath);
        for (int idx : onsets) {
            if (i + idx < isOnset.size())
                isOnset[i + idx] = true;
        }
        i += bestPath.size();
    }

    QList<PhonemeName> result;
    result.reserve(phonemeNames.size());
    for (int j = 0; j < phonemeNames.size(); j++) {
        PhonemeName phoneme;
        phoneme.name = phonemeNames[j];
        phoneme.language = language;
        phoneme.isOnset = isOnset[j];
        result.append(phoneme);
    }
    return result;
}
