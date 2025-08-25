#ifndef DS_EDITOR_LITE_S2PMGR_H
#define DS_EDITOR_LITE_S2PMGR_H

#include <QVector>
#include <QList>

#include "S2p/Syllable2p.h"
#include "Utils/Singleton.h"

#include "Model/AppOptions/AppOptions.h"

class S2pMgr : public Singleton<S2pMgr> {
public:
    S2pMgr();
    ~S2pMgr() override;

    void addS2p(const QString &singerId, const QString &g2pId, const QString &dictPath);

    QStringList syllableToPhoneme(const QString &singerId, const QString &g2pId,
                                  const QString &syllable);
    QVector<QStringList> syllableToPhoneme(const QString &singerId, const QString &g2pId,
                                           const QStringList &syllables);

private:
    static QString calculateFileHash(const QString &filePath);

    QMap<QString, QString> m_idToHash;
    QHash<QString, FillLyric::Syllable2p *> m_hashToS2p;
};
#endif // DS_EDITOR_LITE_S2PMGR_H
