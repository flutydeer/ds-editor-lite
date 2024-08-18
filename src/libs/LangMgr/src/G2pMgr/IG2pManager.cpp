#include <LangMgr/IG2pManager.h>

#include <QDebug>
#include <QCoreApplication>

#include "IG2pManager_p.h"
#include <LangMgr/IG2pFactory.h>

#include <cpp-pinyin/G2pglobal.h>

#include "G2p/Mandarin.h"
#include "G2p/Cantonese.h"
#include "G2p/English.h"
#include "G2p/Kana.h"
#include "G2p/Unknown.h"

namespace LangMgr {

    IG2pManagerPrivate::IG2pManagerPrivate() {
    }

    IG2pManagerPrivate::~IG2pManagerPrivate() = default;

    IG2pFactory *IG2pManager::g2p(const QString &id) const {
        Q_D(const IG2pManager);
        const auto it = d->g2ps.find(id);
        if (it == d->g2ps.end()) {
            if (!d->baseG2p.contains(id))
                qWarning() << "LangMgr::IG2pManager::g2p(): factory does not exist:" << id;
            return d->g2ps.find("unknown").value();
        }
        return it.value();
    }

    QList<IG2pFactory *> IG2pManager::g2ps() const {
        Q_D(const IG2pManager);
        return d->g2ps.values();
    }

    bool IG2pManager::addG2p(IG2pFactory *factory) {
        Q_D(IG2pManager);
        if (!factory) {
            qWarning() << "LangMgr::IG2pManager::addG2p(): trying to add null factory";
            return false;
        }
        if (d->g2ps.contains(factory->id())) {
            qWarning() << "LangMgr::IG2pManager::addG2p(): trying to add duplicated factory:"
                << factory->id();
            return false;
        }
        factory->setParent(this);
        d->g2ps[factory->id()] = factory;
        return true;
    }

    bool IG2pManager::removeG2p(const IG2pFactory *factory) {
        if (factory == nullptr) {
            qWarning() << "LangMgr::IG2pManager::removeG2p(): trying to remove null factory";
            return false;
        }
        return removeG2p(factory->id());
    }

    bool IG2pManager::removeG2p(const QString &id) {
        Q_D(IG2pManager);
        const auto it = d->g2ps.find(id);
        if (it == d->g2ps.end()) {
            qWarning() << "LangMgr::IG2pManager::removeG2p(): factory does not exist:" << id;
            return false;
        }
        it.value()->setParent(nullptr);
        d->g2ps.erase(it);
        return true;
    }

    void IG2pManager::clearG2ps() {
        Q_D(IG2pManager);
        d->g2ps.clear();
    }

    IG2pManager::IG2pManager(QObject *parent) : IG2pManager(*new IG2pManagerPrivate(), parent) {
    }

    IG2pManager::~IG2pManager() = default;

    IG2pManager::IG2pManager(IG2pManagerPrivate &d, QObject *parent) : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;

        addG2p(new Mandarin());
        addG2p(new Cantonese());
        addG2p(new English());
        addG2p(new KanaG2p());
        addG2p(new Unknown());
    }

    bool IG2pManager::initialize(QString &errMsg) {
        Q_D(IG2pManager);
        Pinyin::setDictionaryPath((qApp->applicationDirPath() + "/dict").toUtf8().toStdString());
        const auto g2ps = d->g2ps.values();
        for (const auto g2p : g2ps) {
            g2p->initialize(errMsg);
            if (!errMsg.isEmpty()) {
                return false;
            }
        }
        d->initialized = true;
        return true;
    }

    bool IG2pManager::initialized() {
        Q_D(const IG2pManager);
        return d->initialized;
    }

}