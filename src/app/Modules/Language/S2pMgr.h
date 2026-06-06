#ifndef DS_EDITOR_LITE_S2PMGR_H
#define DS_EDITOR_LITE_S2PMGR_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include "Utils/Singleton.h"

#include "Model/AppOptions/AppOptions.h"
#include "Models/SingerG2pIdentifier.h"
#include "Modules/Inference/Models/SingerIdentifier.h"

class IS2pConverter;

class S2pMgr {
private:
    S2pMgr();
    ~S2pMgr();

public:
    LITE_SINGLETON_DECLARE_INSTANCE(S2pMgr)
    Q_DISABLE_COPY_MOVE(S2pMgr)

public:
    void addS2p(const SingerIdentifier &singerId, const QString &g2pId, const QString &mode,
                const QString &filePath = {});

    QStringList syllableToPhoneme(const SingerIdentifier &singerIdentifier, const QString &g2pId,
                                  const QString &syllable);
    QList<QStringList> syllableToPhoneme(const SingerIdentifier &singerIdentifier,
                                         const QString &g2pId, const QStringList &syllables);

private:
    static QByteArray calculateFileHash(const QString &filePath);
    static QString cacheKey(const QString &mode, const QString &filePath, const QByteArray &hash);

    QMap<SingerG2pIdentifier, QString> m_idToCacheKey;
    QHash<QString, IS2pConverter *> m_cacheKeyToS2p;
};
#endif // DS_EDITOR_LITE_S2PMGR_H
