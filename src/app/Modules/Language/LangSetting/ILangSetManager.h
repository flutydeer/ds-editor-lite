#ifndef ILangSetManager_H
#define ILangSetManager_H

#include <QObject>

#include "ILangSetFactory.h"
#include "IG2pSetFactory.h"

#include <LangMgr/LangCommon.h>
#include <Utils/Singleton.h>

namespace LangSetting {

    class ILangSetManagerPrivate;

    class ILangSetManager final : public QObject, public Singleton<ILangSetManager> {
        Q_OBJECT
        Q_DECLARE_PRIVATE(ILangSetManager)
    public:
        explicit ILangSetManager(QObject *parent = nullptr);
        ~ILangSetManager() override;

    public:
        [[nodiscard]] ILangSetFactory *langSet(const QString &id) const;
        [[nodiscard]] QList<ILangSetFactory *> langSets() const;

        bool addLangSet(ILangSetFactory *factory);
        bool removeLangSet(const ILangSetFactory *factory);
        bool removeLangSet(const QString &id);
        void clearLangSets();

    public:
        [[nodiscard]] IG2pSetFactory *g2pSet(const QString &id) const;
        [[nodiscard]] QList<IG2pSetFactory *> g2pSets() const;

        bool addG2pSet(IG2pSetFactory *factory);
        bool removeG2pSet(const IG2pSetFactory *factory);
        bool removeG2pSet(const QString &id);
        void clearG2pSets();

    private:
        explicit ILangSetManager(ILangSetManagerPrivate &d, QObject *parent = nullptr);

        QScopedPointer<ILangSetManagerPrivate> d_ptr;
    };

}

#endif // ILangSetManager_H