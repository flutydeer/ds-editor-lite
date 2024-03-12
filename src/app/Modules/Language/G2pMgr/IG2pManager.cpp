#include "IG2pManager.h"
#include "IG2pManager_p.h"

#include "IG2pFactory_p.h"

#include <QDebug>

#include "G2p/Mandarin.h"
#include "G2p/Cantonese.h"
#include "G2p/Kana.h"

namespace G2pMgr {

    IG2pManagerPrivate::IG2pManagerPrivate() {
    }

    IG2pManagerPrivate::~IG2pManagerPrivate() = default;

    void IG2pManagerPrivate::init() {
    }

    static IG2pManager *m_instance = nullptr;

    IG2pManager *IG2pManager::instance() {
        return m_instance;
    }

    IG2pFactory *IG2pManager::g2p(const QString &id) const {
        Q_D(const IG2pManager);
        const auto it = d->g2ps.find(id);
        if (it == d->g2ps.end()) {
            qWarning() << "G2pMgr::IG2pManager::g2p(): factory does not exist:" << id;
            return nullptr;
        }
        return it.value();
    }

    bool IG2pManager::addG2p(IG2pFactory *factory) {
        Q_D(IG2pManager);
        if (!factory) {
            qWarning() << "G2pMgr::IG2pManager::addG2p(): trying to add null factory";
            return false;
        }
        if (d->g2ps.contains(factory->id())) {
            qWarning() << "G2pMgr::IG2pManager::addG2p(): trying to add duplicated factory:"
                       << factory->id();
            return false;
        }
        factory->setParent(this);
        d->g2ps[factory->id()] = factory;
        return true;
    }

    bool IG2pManager::removeG2p(const IG2pFactory *factory) {
        if (factory == nullptr) {
            qWarning() << "G2pMgr::IG2pManager::removeG2p(): trying to remove null factory";
            return false;
        }
        return removeG2p(factory->id());
    }

    bool IG2pManager::removeG2p(const QString &id) {
        Q_D(IG2pManager);
        const auto it = d->g2ps.find(id);
        if (it == d->g2ps.end()) {
            qWarning() << "G2pMgr::IG2pManager::removeG2p(): factory does not exist:" << id;
            return false;
        }
        it.value()->setParent(nullptr);
        d->g2ps.erase(it);
        return true;
    }

    QList<IG2pFactory *> IG2pManager::g2ps() const {
        Q_D(const IG2pManager);
        return d->g2ps.values();
    }

    void IG2pManager::clearG2ps() {
        Q_D(IG2pManager);
        d->g2ps.clear();
    }

    IG2pManager::IG2pManager(QObject *parent) : IG2pManager(*new IG2pManagerPrivate(), parent) {
    }

    IG2pManager::~IG2pManager() {
        m_instance = nullptr;
    }

    IG2pManager::IG2pManager(IG2pManagerPrivate &d, QObject *parent) : QObject(parent), d_ptr(&d) {
        m_instance = this;
        d.q_ptr = this;

        d.init();

        addG2p(new Mandarin());
        addG2p(new Cantonese());
        addG2p(new Kana());
    }

}
