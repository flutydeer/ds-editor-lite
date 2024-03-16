#include "ILanguageManager.h"
#include "ILanguageManager_p.h"

#include "ILanguageFactory_p.h"

#include <QDebug>

#include "LangAnalysis/BaseAnalysis/NumberAnalysis.h"
#include "LangAnalysis/BaseAnalysis/SlurAnalysis.h"
#include "LangAnalysis/BaseAnalysis/SpaceAnalysis.h"
#include "LangAnalysis/BaseAnalysis/LinebreakAnalysis.h"
#include "LangAnalysis/BaseAnalysis/Punctuation.h"

#include "LangAnalysis/MandarinAnalysis.h"
#include "LangAnalysis/CantoneseAnalysis.h"
#include "LangAnalysis/EnglishAnalysis.h"
#include "LangAnalysis/KanaAnalysis.h"

namespace LangMgr {
    ILanguageManagerPrivate::ILanguageManagerPrivate() {
    }

    ILanguageManagerPrivate::~ILanguageManagerPrivate() = default;

    void ILanguageManagerPrivate::init() {
    }

    static ILanguageManager *m_instance = nullptr;

    ILanguageManager *ILanguageManager::instance() {
        return m_instance;
    }

    ILanguageFactory *ILanguageManager::language(const QString &id) const {
        Q_D(const ILanguageManager);
        const auto it = d->languages.find(id);
        if (it == d->languages.end()) {
            qWarning() << "LangMgr::ILanguageManager::language(): factory does not exist:" << id;
            return nullptr;
        }
        return it.value();
    }

    bool ILanguageManager::addLanguage(ILanguageFactory *factory) {
        Q_D(ILanguageManager);
        if (!factory) {
            qWarning() << "LangMgr::ILanguageManager::addLanguage(): trying to add null factory";
            return false;
        }
        if (d->languages.contains(factory->id())) {
            qWarning()
                << "LangMgr::ILanguageManager::addLanguage(): trying to add duplicated factory:"
                << factory->id();
            return false;
        }
        factory->setParent(this);
        d->languages[factory->id()] = factory;
        return true;
    }

    bool ILanguageManager::removeLanguage(const ILanguageFactory *factory) {
        if (factory == nullptr) {
            qWarning()
                << "LangMgr::ILanguageManager::removeLanguage(): trying to remove null factory";
            return false;
        }
        return removeLanguage(factory->id());
    }

    bool ILanguageManager::removeLanguage(const QString &id) {
        Q_D(ILanguageManager);
        if (!d->languages.contains(id)) {
            qWarning() << "LangMgr::ILanguageManager::removeLanguage(): factory does not exist:"
                       << id;
            return false;
        }
        d->languages.remove(id);
        return true;
    }

    void ILanguageManager::clearLanguages() {
        Q_D(ILanguageManager);
        d->languages.clear();
    }

    QList<LangConfig> ILanguageManager::languageConfig() const {
        Q_D(const ILanguageManager);
        const auto factories = orderedLanguages();

        QList<LangConfig> result;
        for (const auto &factory : factories) {
            result.append({factory->id(), factory->enabled(), factory->discardResult(),
                           factory->author(), factory->description()});
        }
        return result;
    }

    void ILanguageManager::setLanguageConfig(const QList<LangConfig> &configs) {
        Q_D(ILanguageManager);
        for (const auto &config : configs) {
            const auto factory = language(config.language);
            if (factory) {
                factory->setEnabled(config.enabled);
                factory->setDiscardResult(config.discardResult);
                factory->setAuthor(config.author);
                factory->setDescription(config.description);
            }
        }
    }

    QList<ILanguageFactory *>
        ILanguageManager::orderedLanguages(const QStringList &priorityList) const {
        Q_D(const ILanguageManager);
        QStringList order = d->order;

        QList<ILanguageFactory *> result;
        for (const auto &id : priorityList) {
            result.append(language(id));
        }

        for (const auto &id : order) {
            if (priorityList.contains(id))
                continue;
            result.append(language(id));
        }
        return result;
    }

    QList<ILanguageFactory *> ILanguageManager::languages() const {
        Q_D(const ILanguageManager);
        return d->languages.values();
    }

    QList<LangNote> ILanguageManager::split(const QString &input) const {
        auto analysis = this->orderedLanguages();
        QList<LangNote> result = {LangNote(input)};
        for (const auto &factory : analysis) {
            result = factory->split(result);
        }
        return result;
    }

    void ILanguageManager::correct(const QList<LangNote *> &input) const {
        auto analysis = this->orderedLanguages();
        for (const auto &factory : analysis) {
            factory->correct(input);
        }
    }

    QString ILanguageManager::analysis(const QString &input) const {
        QString result = "Unknown";
        auto analysis = this->orderedLanguages();

        for (const auto &factory : analysis) {
            result = factory->analysis(input);
            if (result != "Unknown")
                break;
        }

        return result;
    }

    QStringList ILanguageManager::analysis(const QStringList &input) const {
        auto analysis = this->orderedLanguages();
        QList<LangNote *> inputNote;
        for (const auto &lyric : input) {
            inputNote.append(new LangNote(lyric));
        }

        for (const auto &factory : analysis) {
            factory->correct(inputNote);
        }

        QStringList result;
        for (const auto &note : inputNote)
            result.append(note->language);
        return result;
    }

    ILanguageManager::ILanguageManager(QObject *parent)
        : ILanguageManager(*new ILanguageManagerPrivate(), parent) {
    }

    ILanguageManager::~ILanguageManager() {
        m_instance = nullptr;
    }

    ILanguageManager::ILanguageManager(ILanguageManagerPrivate &d, QObject *parent)
        : QObject(parent), d_ptr(&d) {
        m_instance = this;
        d.q_ptr = this;

        d.init();

        addLanguage(new NumberAnalysis());
        addLanguage(new SlurAnalysis());
        addLanguage(new SpaceAnalysis());
        addLanguage(new LinebreakAnalysis());
        addLanguage(new Punctuation());

        addLanguage(new MandarinAnalysis());
        addLanguage(new CantoneseAnalysis());
        addLanguage(new EnglishAnalysis());
        addLanguage(new KanaAnalysis());
    }

} // LangMgr