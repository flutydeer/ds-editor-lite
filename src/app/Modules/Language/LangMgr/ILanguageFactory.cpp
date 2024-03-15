#include "ILanguageFactory.h"
#include "ILanguageFactory_p.h"
#include <QDebug>
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

    ILanguageFactory::ILanguageFactory(ILanguageFactoryPrivate &d, const QString &id,
                                       QObject *parent)
        : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;
        d.id = id;

        d.init();
    }

    QString ILanguageFactory::id() const {
        Q_D(const ILanguageFactory);
        return d->id;
    }

    QString ILanguageFactory::description() const {
        Q_D(const ILanguageFactory);
        return d->description;
    }

    void ILanguageFactory::setDescription(const QString &description) {
        Q_D(ILanguageFactory);
        d->description = description;
    }

    QString ILanguageFactory::displayName() const {
        Q_D(const ILanguageFactory);
        return d->displayName;
    }

    void ILanguageFactory::setDisplayName(const QString &displayName) {
        Q_D(ILanguageFactory);
        d->displayName = displayName;
    }

    QString ILanguageFactory::category() const {
        Q_D(const ILanguageFactory);
        return d->category;
    }

    void ILanguageFactory::setCategory(const QString &category) {
        Q_D(ILanguageFactory);
        d->category = category;
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
        QList<LangNote> result;
        for (const auto &note : input) {
            if (note.language == "Unknown") {
                result.append(split(note.lyric));
            } else {
                result.append(note);
            }
        }
        return result;
    }

    void ILanguageFactory::analysis(const QList<LangNote *> &input) const {
        for (const auto &note : input) {
            if (note->language == "Unknown") {
                if (contains(note->lyric))
                    note->language = id();
            }
        }
    }

} // LangMgr