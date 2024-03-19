#include "IG2pFactory.h"
#include "IG2pFactory_p.h"

#include <QCheckBox>
#include <QJsonObject>
#include <QVBoxLayout>

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

    Phonic IG2pFactory::convert(const QString &input) const {
        return convert(QStringList() << input).at(0);
    }

    Phonic IG2pFactory::convert(const Note *&input) const {
        return convert(input->lyric());
    }

    QList<Phonic> IG2pFactory::convert(const QList<Note *> &input) const {
        QStringList inputText;
        for (const auto note : input) {
            inputText.append(note->lyric());
        }
        return convert(inputText);
    }

    QList<Phonic> IG2pFactory::convert(QStringList &input) const {
        Q_UNUSED(input);
        return {};
    }

    QWidget *IG2pFactory::configWidget(QJsonObject *config) {
        Q_UNUSED(config);
        return new QWidget();
    }
}
