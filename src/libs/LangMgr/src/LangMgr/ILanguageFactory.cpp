#include "ILanguageFactory.h"
#include "ILanguageFactory_p.h"

#include <QDebug>
#include <QtConcurrent/QtConcurrent>

#include "../G2pMgr/IG2pManager.h"

namespace LangMgr {

    ILanguageFactoryPrivate::ILanguageFactoryPrivate() {
    }

    ILanguageFactoryPrivate::~ILanguageFactoryPrivate() = default;

    void ILanguageFactoryPrivate::init() {
    }

    ILanguageFactory::ILanguageFactory(const QString &id, QObject *parent)
        : ILanguageFactory(*new ILanguageFactoryPrivate(), id, parent) {
    }

    ILanguageFactory::~ILanguageFactory() = default;

    bool ILanguageFactory::initialize(QString &errMsg) {
        Q_UNUSED(errMsg);
        return true;
    }

    ILanguageFactory::ILanguageFactory(ILanguageFactoryPrivate &d, const QString &id,
                                       QObject *parent)
        : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;
        d.id = id;
        d.m_selectedG2p = id;
        d.categroy = id;

        d.init();
        d.m_g2pConfig = new QJsonObject();
    }

    QString ILanguageFactory::id() const {
        Q_D(const ILanguageFactory);
        return d->id;
    }

    QString ILanguageFactory::displayName() const {
        Q_D(const ILanguageFactory);
        return d->displayName;
    }

    void ILanguageFactory::setDisplayName(const QString &name) {
        Q_D(ILanguageFactory);
        d->displayName = name;
    }

    QString ILanguageFactory::category() const {
        Q_D(const ILanguageFactory);
        return d->categroy;
    }

    void ILanguageFactory::setCategory(const QString &category) {
        Q_D(ILanguageFactory);
        d->categroy = category;
    }

    QString ILanguageFactory::selectedG2p() const {
        Q_D(const ILanguageFactory);
        return d->m_selectedG2p;
    }

    void ILanguageFactory::setG2p(const QString &g2pId) {
        Q_D(ILanguageFactory);
        d->m_selectedG2p = g2pId;
    }

    bool ILanguageFactory::enabled() const {
        Q_D(const ILanguageFactory);
        return d->enabled;
    }

    void ILanguageFactory::setEnabled(const bool &enable) {
        Q_D(ILanguageFactory);
        d->enabled = enable;
    }

    bool ILanguageFactory::discardResult() const {
        Q_D(const ILanguageFactory);
        return d->discardResult;
    }

    void ILanguageFactory::setDiscardResult(const bool &discard) {
        Q_D(ILanguageFactory);
        d->discardResult = discard;
    }

    QString ILanguageFactory::description() const {
        Q_D(const ILanguageFactory);
        return d->description;
    }

    void ILanguageFactory::setDescription(const QString &description) {
        Q_D(ILanguageFactory);
        d->description = description;
    }

    QString ILanguageFactory::author() const {
        Q_D(const ILanguageFactory);
        return d->author;
    }

    void ILanguageFactory::setAuthor(const QString &author) {
        Q_D(ILanguageFactory);
        d->author = author;
    }

    QString ILanguageFactory::randString() const {
        return QString();
    }

    bool ILanguageFactory::contains(const QChar &c) const {
        Q_UNUSED(c);
        return false;
    }

    bool ILanguageFactory::contains(const QString &input) const {
        Q_UNUSED(input);
        return false;
    }

    QList<LangNote> ILanguageFactory::split(const QString &input) const {
        Q_UNUSED(input);
        return {};
    }

    QList<LangNote> ILanguageFactory::split(const QList<LangNote> &input) const {
        Q_D(const ILanguageFactory);
        if (!d->enabled) {
            return input;
        }

        QList<LangNote> result;
        for (const auto &note : input) {
            if (note.language == "Unknown") {
                const auto splitRes = split(note.lyric);
                for (const auto &res : splitRes) {
                    if (res.language == id() && d->discardResult) {
                        continue;
                    }
                    result.append(res);
                }
            } else {
                result.append(note);
            }
        }
        return result;
    }

    QString ILanguageFactory::analysis(const QString &input) const {
        return contains(input) ? id() : "Unknown";
    }

    void ILanguageFactory::correct(const QList<LangNote *> &input) const {
        for (const auto &note : input) {
            if (note->language == "Unknown") {
                if (contains(note->lyric))
                    note->language = id();
            }
        }
    }

    QJsonObject *ILanguageFactory::g2pConfig() {
        Q_D(const ILanguageFactory);
        return d->m_g2pConfig;
    }

    void ILanguageFactory::loadConfig(const QJsonObject &config) {
        Q_D(ILanguageFactory);
        if (config.contains("enabled")) {
            d->enabled = config.value("enabled").toBool();
        }
        if (config.contains("discardResult")) {
            d->discardResult = config.value("discardResult").toBool();
        }
        if (config.contains("category")) {
            d->categroy = config.value("category").toString();
        }
        if (config.contains("g2p")) {
            d->m_selectedG2p = config.value("g2p").toString();
        }
        if (config.contains("g2pConfig")) {
            d->m_g2pConfig = new QJsonObject(config.value("g2pConfig").toObject());
        }
    }

    QJsonObject ILanguageFactory::exportConfig() const {
        Q_D(const ILanguageFactory);
        QJsonObject config;
        config.insert("enabled", d->enabled);
        config.insert("discardResult", d->discardResult);
        config.insert("category", d->categroy);
        config.insert("g2p", d->m_selectedG2p);
        config.insert("g2pConfig", *d->m_g2pConfig);
        return config;
    }

} // LangMgr