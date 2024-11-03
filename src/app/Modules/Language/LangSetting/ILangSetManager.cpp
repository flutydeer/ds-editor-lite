#include "ILangSetManager.h"
#include "ILangSetManager_p.h"

#include <QDebug>
#include <qcoreapplication.h>

#include "G2pSet/MandarinSet.h"
#include "G2pSet/CantoneseSet.h"
#include "G2pSet/EnglishSet.h"
#include "G2pSet/UnknownG2pSet.h"

namespace LangSetting {

    ILangSetManagerPrivate::ILangSetManagerPrivate() {
    }

    ILangSetManagerPrivate::~ILangSetManagerPrivate() = default;

    IG2pSetFactory *ILangSetManager::g2pSet(const QString &id) const {
        Q_D(const ILangSetManager);
        const auto it = d->g2pSets.find(id);
        if (it == d->g2pSets.end()) {
            // qWarning() << "LangSet::ILangSetManager::g2pSet(): factory does not exist:" << id;
            return d->g2pSets.find("unknown").value();
        }
        return it.value();
    }

    QList<IG2pSetFactory *> ILangSetManager::g2pSets() const {
        Q_D(const ILangSetManager);
        return d->g2pSets.values();
    }

    bool ILangSetManager::addG2pSet(IG2pSetFactory *factory) {
        Q_D(ILangSetManager);
        if (!factory) {
            qWarning() << "LangSet::ILangSetManager::addG2pSet(): trying to add null factory";
            return false;
        }
        if (d->g2pSets.contains(factory->id())) {
            qWarning() << "LangSet::ILangSetManager::addG2pSet(): trying to add duplicated factory:"
                       << factory->id();
            return false;
        }
        factory->setParent(this);
        d->g2pSets[factory->id()] = factory;
        return true;
    }

    bool ILangSetManager::removeG2pSet(const IG2pSetFactory *factory) {
        if (factory == nullptr) {
            qWarning() << "G2pMgr::ILangSetManager::removeG2pSet(): trying to remove null factory";
            return false;
        }
        return removeG2pSet(factory->id());
    }

    bool ILangSetManager::removeG2pSet(const QString &id) {
        Q_D(ILangSetManager);
        const auto it = d->g2pSets.find(id);
        if (it == d->g2pSets.end()) {
            qWarning() << "G2pMgr::ILangSetManager::removeG2pSet(): factory does not exist:" << id;
            return false;
        }
        it.value()->setParent(nullptr);
        d->g2pSets.erase(it);
        return true;
    }

    void ILangSetManager::clearG2pSets() {
        Q_D(ILangSetManager);
        d->g2pSets.clear();
    }

    ILangSetManager::ILangSetManager(QObject *parent)
        : ILangSetManager(*new ILangSetManagerPrivate(), parent) {
    }

    ILangSetManager::~ILangSetManager() = default;

    ILangSetManager::ILangSetManager(ILangSetManagerPrivate &d, QObject *parent)
        : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;

        addG2pSet(new MandarinSet());
        addG2pSet(new CantoneseSet());
        addG2pSet(new EnglishSet());
        addG2pSet(new UnknownG2pSet());
    }

}