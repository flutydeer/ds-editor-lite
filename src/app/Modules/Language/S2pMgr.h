#ifndef DS_EDITOR_LITE_S2PMGR_H
#define DS_EDITOR_LITE_S2PMGR_H

#include <QVector>
#include <QList>

#include "S2p/Syllable2p.h"
#include "Utils/Singleton.h"

#include "Model/AppOptions/AppOptions.h"
#include "Models/SingerG2pIdentifier.h"
#include "Modules/Inference/Models/SingerIdentifier.h"

class S2pMgr {
private:
    S2pMgr();
    ~S2pMgr();

public:
    LITE_SINGLETON_DECLARE_INSTANCE(S2pMgr)
    Q_DISABLE_COPY_MOVE(S2pMgr)

public:
    void addS2p(const SingerIdentifier &singerId, const QString &g2pId, const QString &dictPath);

    QStringList syllableToPhoneme(const SingerIdentifier &singerIdentifier, const QString &g2pId,
                                  const QString &syllable);
    QList<QStringList> syllableToPhoneme(const SingerIdentifier &singerIdentifier,
                                         const QString &g2pId, const QStringList &syllables);

private:
    static QByteArray calculateFileHash(const QString &filePath);

    QMap<SingerG2pIdentifier, QByteArray> m_idToHash;
    QHash<QByteArray, FillLyric::Syllable2p *> m_hashToS2p;
};
#endif // DS_EDITOR_LITE_S2PMGR_H
