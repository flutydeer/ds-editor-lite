#include "IG2pSetFactory.h"
#include "IG2pSetFactory_p.h"

#include <QCheckBox>
#include <QJsonObject>
#include <QVBoxLayout>

namespace LangSetting {

    IG2pSetFactoryPrivate::IG2pSetFactoryPrivate() {
    }

    IG2pSetFactoryPrivate::~IG2pSetFactoryPrivate() = default;

    void IG2pSetFactoryPrivate::init() {
    }

    IG2pSetFactory::IG2pSetFactory(const QString &id, QObject *parent)
        : IG2pSetFactory(*new IG2pSetFactoryPrivate(), id, parent) {
    }

    IG2pSetFactory::~IG2pSetFactory() = default;

    IG2pSetFactory::IG2pSetFactory(IG2pSetFactoryPrivate &d, const QString &id, QObject *parent)
        : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;
        d.id = id;

        d.init();
    }

    QString IG2pSetFactory::id() const {
        Q_D(const IG2pSetFactory);
        return d->id;
    }

    QString IG2pSetFactory::displayName() const {
        Q_D(const IG2pSetFactory);
        return d->displayName;
    }

    void IG2pSetFactory::setDisplayName(const QString &displayName) {
        Q_D(IG2pSetFactory);
        d->displayName = displayName;
    }

    QString IG2pSetFactory::author() const {
        Q_D(const IG2pSetFactory);
        return d->author;
    }

    void IG2pSetFactory::setAuthor(const QString &author) {
        Q_D(IG2pSetFactory);
        d->author = author;
    }

    QString IG2pSetFactory::description() const {
        Q_D(const IG2pSetFactory);
        return d->description;
    }

    void IG2pSetFactory::setDescription(const QString &description) {
        Q_D(IG2pSetFactory);
        d->description = description;
    }

    QJsonObject IG2pSetFactory::config() {
        return {};
    }

    QWidget *IG2pSetFactory::configWidget(const QJsonObject &config, bool editable) {
        Q_UNUSED(config);
        Q_UNUSED(editable);
        return new QWidget();
    }
}
