#include "PhonemeTypeMap.h"

bool PhonemeTypeMap::load(const QJsonObject &obj) {
    m_map.clear();
    for (auto it = obj.begin(); it != obj.end(); ++it)
        m_map.insert(it.key(), it.value().toString());
    return !m_map.isEmpty();
}

QString PhonemeTypeMap::type(const QString &phonemeName) const {
    return m_map.value(phonemeName);
}
