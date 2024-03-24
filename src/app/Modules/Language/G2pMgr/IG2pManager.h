#ifndef IG2PMANAGER_H
#define IG2PMANAGER_H

#include <QObject>

#include "IG2pFactory.h"
#include "Modules/Language/LangCommon.h"
#include "Utils/Singleton.h"

namespace G2pMgr {

    class IG2pManagerPrivate;

    class IG2pManager final : public QObject, public Singleton<IG2pManager> {
        Q_OBJECT
        Q_DECLARE_PRIVATE(IG2pManager)
    public:
        explicit IG2pManager(QObject *parent = nullptr);
        ~IG2pManager() override;

        bool initialize(QString &errMsg);
        bool initialized();

    public:
        [[nodiscard]] IG2pFactory *g2p(const QString &id) const;
        [[nodiscard]] QList<IG2pFactory *> g2ps() const;

        bool addG2p(IG2pFactory *factory);
        bool removeG2p(const IG2pFactory *factory);
        bool removeG2p(const QString &id);
        void clearG2ps();

    private:
        explicit IG2pManager(IG2pManagerPrivate &d, QObject *parent = nullptr);

        QScopedPointer<IG2pManagerPrivate> d_ptr;
    };

}

#endif // IG2PMANAGER_H