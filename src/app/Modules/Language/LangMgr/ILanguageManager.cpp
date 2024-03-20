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
#include "LangAnalysis/PinyinAnalysis.h"
#include "LangAnalysis/CantoneseAnalysis.h"
#include "LangAnalysis/KanaAnalysis.h"
#include "LangAnalysis/RomajiAnalysis.h"
#include "LangAnalysis/EnglishAnalysis.h"

#include "Modules/Language/G2pMgr/IG2pManager.h"

namespace LangMgr {
    ILanguageManagerPrivate::ILanguageManagerPrivate() {
    }

    ILanguageManagerPrivate::~ILanguageManagerPrivate() = default;

    void ILanguageManagerPrivate::init() {
    }

    // static ILanguageManager *m_instance = nullptr;
    //
    // ILanguageManager *ILanguageManager::instance() {
    //     return m_instance;
    // }

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

    LangConfig ILanguageManager::languageConfig(const QString &id) const {
        Q_D(const ILanguageManager);
        const auto factory = language(id);

        LangConfig result = {factory->id(), factory->enabled(), factory->discardResult(),
                             factory->author(), factory->description()};

        return result;
    }

    QList<LangConfig> ILanguageManager::languageConfigs() const {
        Q_D(const ILanguageManager);
        const auto factories = priorityLanguages();

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
        ILanguageManager::priorityLanguages(const QStringList &priorityList) const {
        Q_D(const ILanguageManager);
        QStringList order = d->order;

        QList<ILanguageFactory *> result;
        for (const auto &category : priorityList) {
            for (const auto &lang : order) {
                const auto factory = language(lang);
                if (factory->category() == category) {
                    result.append(factory);
                }
            }
        }

        for (const auto &id : order) {
            const auto factory = language(id);
            if (!result.contains(factory)) {
                result.append(factory);
            }
        }
        return result;
    }

    QStringList ILanguageManager::languageOrder() const {
        Q_D(const ILanguageManager);
        return d->order;
    }

    void ILanguageManager::setLanguageOrder(const QStringList &order) {
        Q_D(ILanguageManager);
        d->order = order;
    }

    QList<ILanguageFactory *> ILanguageManager::languages() const {
        Q_D(const ILanguageManager);
        return d->languages.values();
    }

    QList<LangNote> ILanguageManager::split(const QString &input) const {
        auto analysis = this->priorityLanguages();
        QList<LangNote> result = {LangNote(input)};
        for (const auto &factory : analysis) {
            result = factory->split(result);
        }
        return result;
    }

    QStringList ILanguageManager::categoryList() const {
        Q_D(const ILanguageManager);
        return d->category;
    }

    void ILanguageManager::correct(const QList<LangNote *> &input) const {
        auto analysis = this->priorityLanguages();
        for (const auto &factory : analysis) {
            factory->correct(input);
        }
    }

    QString ILanguageManager::analysis(const QString &input) const {
        QString result = "Unknown";
        auto analysis = this->priorityLanguages();

        for (const auto &factory : analysis) {
            result = factory->analysis(input);
            if (result != "Unknown")
                break;
        }

        return result;
    }

    QStringList ILanguageManager::analysis(const QStringList &input) const {
        auto analysis = this->priorityLanguages();
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
        // m_instance = nullptr;
    }

    ILanguageManager::ILanguageManager(ILanguageManagerPrivate &d, QObject *parent)
        : QObject(parent), d_ptr(&d) {
        // m_instance = this;
        d.q_ptr = this;

        d.init();

        addLanguage(new NumberAnalysis());
        addLanguage(new SlurAnalysis());
        addLanguage(new SpaceAnalysis());
        addLanguage(new LinebreakAnalysis());
        addLanguage(new Punctuation());

        addLanguage(new MandarinAnalysis());
        addLanguage(new PinyinAnalysis());
        addLanguage(new CantoneseAnalysis());
        addLanguage(new KanaAnalysis());
        addLanguage(new RomajiAnalysis());
        addLanguage(new EnglishAnalysis());
    }

    void ILanguageManager::convert(const QList<LangNote *> &input) const {
        const auto g2pMgr = G2pMgr::IG2pManager::instance();

        QMap<QString, QList<int>> languageIndexMap;
        QMap<QString, QStringList> languageLyricMap;

        for (int i = 0; i < input.size(); ++i) {
            const LangNote *note = input.at(i);
            languageIndexMap[note->language].append(i); // 记录原始索引
            languageLyricMap[note->language].append(note->lyric);
        }

        const auto languages = languageIndexMap.keys();
        for (const auto &language : languages) {
            const auto rawLyrics = languageLyricMap[language];
            const auto g2pFactory = g2pMgr->g2p(this->language(language)->selectedG2p());
            const auto g2pConfig = this->language(language)->g2pConfig();

            const auto tempRes = g2pFactory->convert(rawLyrics, g2pConfig);
            for (int i = 0; i < tempRes.size(); i++) {
                const auto index = languageIndexMap[language][i];
                input[index]->syllable = tempRes[i].pronunciation.original;
                input[index]->candidates = tempRes[i].candidates;
            }
        }
    }

} // LangMgr