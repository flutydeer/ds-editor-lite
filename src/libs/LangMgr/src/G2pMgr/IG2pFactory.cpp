#include "IG2pFactory.h"
#include "IG2pFactory_p.h"

#include <QJsonObject>

namespace G2pMgr {

    IG2pFactoryPrivate::IG2pFactoryPrivate() {
    }

    IG2pFactoryPrivate::~IG2pFactoryPrivate() = default;

    void IG2pFactoryPrivate::init() {
    }

    IG2pFactory::IG2pFactory(const QString &id, QObject *parent)
        : IG2pFactory(*new IG2pFactoryPrivate(), id, parent) {
    }

    IG2pFactory::~IG2pFactory() = default;

    IG2pFactory::IG2pFactory(IG2pFactoryPrivate &d, const QString &id, QObject *parent)
        : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;
        d.id = id;

        d.init();
    }

    bool IG2pFactory::initialize(QString &errMsg) {
        Q_UNUSED(errMsg);
        return true;
    }

    QString IG2pFactory::id() const {
        Q_D(const IG2pFactory);
        return d->id;
    }

    QString IG2pFactory::displayName() const {
        Q_D(const IG2pFactory);
        return d->displayName;
    }

    void IG2pFactory::setDisplayName(const QString &displayName) {
        Q_D(IG2pFactory);
        d->displayName = displayName;
    }

    QString IG2pFactory::author() const {
        Q_D(const IG2pFactory);
        return d->author;
    }

    void IG2pFactory::setAuthor(const QString &author) {
        Q_D(IG2pFactory);
        d->author = author;
    }

    QString IG2pFactory::description() const {
        Q_D(const IG2pFactory);
        return d->description;
    }

    void IG2pFactory::setDescription(const QString &description) {
        Q_D(IG2pFactory);
        d->description = description;
    }

    QJsonObject IG2pFactory::config() {
        return {};
    }

    LangNote IG2pFactory::convert(const QString &input, const QJsonObject *config) const {
        return convert(QStringList() << input, config).at(0);
    }

    QList<LangNote> IG2pFactory::convert(const QStringList &input, const QJsonObject *config) const {
        Q_UNUSED(input);
        Q_UNUSED(config);
        return {};
    }
}
