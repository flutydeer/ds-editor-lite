#include "S2pMgr.h"

#include <QCryptographicHash>
#include <QFile>

S2pMgr::S2pMgr() {
}

S2pMgr::~S2pMgr() {
    qDeleteAll(m_hashToS2p);
    m_hashToS2p.clear();
}

LITE_SINGLETON_IMPLEMENT_INSTANCE(S2pMgr)

QByteArray S2pMgr::calculateFileHash(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (hash.addData(&file)) {
        return hash.result();
    }

    return {};
}

void S2pMgr::addS2p(const SingerIdentifier &singerId, const QString &g2pId, const QString &dictPath) {
    const auto dictHash = calculateFileHash(dictPath);
    if (dictHash.isEmpty()) {
        qWarning() << "Failed to calculate hash for file:" << dictPath;
        return;
    }

    m_idToHash[{singerId, g2pId}] = dictHash;

    if (!m_hashToS2p.contains(dictHash)) {
        m_hashToS2p[dictHash] = new FillLyric::Syllable2p(dictPath);
    }
}

QStringList S2pMgr::syllableToPhoneme(const SingerIdentifier &singerIdentifier, const QString &g2pId,
                                      const QString &syllable) {
    const auto it = m_idToHash.constFind({singerIdentifier, g2pId});
    if (it == m_idToHash.constEnd()) {
        if (singerIdentifier.isEmpty()) {
            qWarning() << "No Singer, S2p can not work.";
        } else {
            qWarning() << "No S2p instance found for singer:" << singerIdentifier
                       << "g2pId:" << g2pId;
        }
        return {};
    }
    const auto s2pInstance = m_hashToS2p[it.value()];
    return s2pInstance->syllableToPhoneme(syllable);
}

QList<QStringList> S2pMgr::syllableToPhoneme(const SingerIdentifier &singerIdentifier, const QString &g2pId,
                                             const QStringList &syllables) {
    const auto it = m_idToHash.constFind({singerIdentifier, g2pId});
    if (it == m_idToHash.constEnd()) {
        qWarning() << "No S2p instance found for singer:" << singerIdentifier
                   << "g2pId:" << g2pId;
        return {};
    }

    const auto s2pInstance = m_hashToS2p[it.value()];

    QVector<QStringList> result;
    if (syllables.isEmpty()) {
        return result;
    }
    result.reserve(syllables.size());
    for (const auto &syllable : syllables) {
        result.append(s2pInstance->syllableToPhoneme(syllable));
    }
    return result;
}