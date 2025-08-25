#include "S2pMgr.h"

#include <QCryptographicHash>
#include <QFile>

S2pMgr::S2pMgr() {
}

S2pMgr::~S2pMgr() {
}

QString S2pMgr::calculateFileHash(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return QString();

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&file))
        return hash.result().toHex();

    return QString();
}

void S2pMgr::addS2p(const QString &singerId, const QString &g2pId, const QString &dictPath) {
    const QString dictHash = calculateFileHash(dictPath);
    if (dictHash.isEmpty()) {
        qWarning() << "Failed to calculate hash for file:" << dictPath;
        return;
    }

    const auto id = singerId + "_" + g2pId;
    m_idToHash[id] = dictHash;

    if (!m_hashToS2p.contains(dictHash))
        m_hashToS2p[dictHash] = new FillLyric::Syllable2p(dictPath);
}

QStringList S2pMgr::syllableToPhoneme(const QString &singerId, const QString &g2pId,
                                      const QString &syllable) {
    const auto id = singerId + "_" + g2pId;
    if (!m_idToHash.contains(id)) {
        qWarning() << "No S2p instance found for singerId:" << singerId << "g2pId:" << g2pId;
        return QStringList();
    }
    const auto s2pInstance = m_hashToS2p[m_idToHash[id]];
    return s2pInstance->syllableToPhoneme(syllable);
}

QVector<QStringList> S2pMgr::syllableToPhoneme(const QString &singerId, const QString &g2pId,
                                               const QStringList &syllables) {
    const auto id = singerId + "_" + g2pId;
    if (!m_idToHash.contains(id)) {
        qWarning() << "No S2p instance found for singerId:" << singerId << "g2pId:" << g2pId;
        return QVector<QStringList>();
    }

    const auto s2pInstance = m_hashToS2p[m_idToHash[id]];

    QVector<QStringList> result;
    if (syllables.isEmpty())
        return result;
    for (const auto &syllable : syllables)
        result.append(s2pInstance->syllableToPhoneme(syllable));
    return result;
}