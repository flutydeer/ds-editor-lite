#include <LangMgr/ILanguageManager.h>
#include "ILanguageManager_p.h"

#include <LangMgr/ILanguageFactory.h>

#include "LangAnalysis/BaseAnalysis/NumberAnalysis.h"
#include "LangAnalysis/BaseAnalysis/SlurAnalysis.h"
#include "LangAnalysis/BaseAnalysis/SpaceAnalysis.h"
#include "LangAnalysis/BaseAnalysis/LinebreakAnalysis.h"
#include "LangAnalysis/BaseAnalysis/Punctuation.h"
#include "LangAnalysis/BaseAnalysis/UnknownAnalysis.h"

#include "LangAnalysis/MandarinAnalysis.h"
#include "LangAnalysis/PinyinAnalysis.h"
#include "LangAnalysis/CantoneseAnalysis.h"
#include "LangAnalysis/JyutpingAnalysis.h"
#include "LangAnalysis/KanaAnalysis.h"
#include "LangAnalysis/RomajiAnalysis.h"
#include "LangAnalysis/EnglishAnalysis.h"

#include <LangMgr/IG2pManager.h>

#include <QCoreApplication>

namespace LangMgr {
    ILanguageManagerPrivate::ILanguageManagerPrivate() {
    }

    ILanguageManagerPrivate::~ILanguageManagerPrivate() = default;

    ILanguageFactory *ILanguageManager::language(const QString &id) const {
        Q_D(const ILanguageManager);
        const auto it = d->languages.find(id);
        if (it == d->languages.end()) {
            qWarning() << "LangMgr::ILanguageManager::language(): factory does not exist:" << id;
            return d->languages.find("unknown").value();
        }
        return it.value();
    }

    QList<ILanguageFactory *> ILanguageManager::languages() const {
        Q_D(const ILanguageManager);
        return d->languages.values();
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

    QList<ILanguageFactory *>
        ILanguageManager::priorityLanguages(const QStringList &priorityList) const {
        Q_D(const ILanguageManager);
        QStringList order = d->defaultOrder;

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

    QStringList ILanguageManager::defaultOrder() const {
        Q_D(const ILanguageManager);
        return d->defaultOrder;
    }

    void ILanguageManager::setDefaultOrder(const QStringList &order) {
        Q_D(ILanguageManager);
        d->defaultOrder = order;
    }

    QList<LangNote> ILanguageManager::split(const QString &input) const {
        const auto langFactorys = this->priorityLanguages();
        QList<LangNote> result = {LangNote(input)};
        for (const auto &factory : langFactorys) {
            result = factory->split(result);
        }
        return result;
    }

    void ILanguageManager::correct(const QList<LangNote *> &input,
                                   const QStringList &priorityList) const {
        const auto langFactorys = this->priorityLanguages(priorityList);
        for (const auto &factory : langFactorys) {
            factory->correct(input);
        }
    }

    QString ILanguageManager::analysis(const QString &input,
                                       const QStringList &priorityList) const {
        QString result = "unknown";
        auto analysis = this->priorityLanguages(priorityList);

        for (const auto &factory : analysis) {
            result = factory->analysis(input);
            if (result != "unknown")
                break;
        }

        return result;
    }

    QStringList ILanguageManager::analysis(const QStringList &input,
                                           const QStringList &priorityList) const {
        auto analysis = this->priorityLanguages(priorityList);
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

    ILanguageManager::~ILanguageManager() = default;

    bool ILanguageManager::initialize(QString &errMsg) {
        Q_D(ILanguageManager);
        for (const auto &factory : d->languages) {
            if (!factory->initialize(errMsg)) {
                return false;
            }
        }
        d->initialized = true;
        return true;
    }

    bool ILanguageManager::initialized() {
        Q_D(const ILanguageManager);
        return d->initialized;
    }

    ILanguageManager::ILanguageManager(ILanguageManagerPrivate &d, QObject *parent)
        : QObject(parent), d_ptr(&d) {
        d.q_ptr = this;

        addLanguage(new NumberAnalysis());
        addLanguage(new SlurAnalysis());
        addLanguage(new SpaceAnalysis());
        addLanguage(new LinebreakAnalysis());
        addLanguage(new Punctuation());
        addLanguage(new UnknownAnalysis());

        addLanguage(new MandarinAnalysis());
        addLanguage(new PinyinAnalysis());
        addLanguage(new CantoneseAnalysis());
        addLanguage(new JyutpingAnalysis());
        addLanguage(new KanaAnalysis());
        addLanguage(new RomajiAnalysis());
        addLanguage(new EnglishAnalysis());
    }

    void ILanguageManager::convert(const QList<LangNote *> &input) const {
        const auto g2pMgr = LangMgr::IG2pManager::instance();

        QMap<QString, QList<int>> languageIndexMap;
        QMap<QString, QStringList> languageLyricMap;

        for (int i = 0; i < input.size(); ++i) {
            const LangNote *note = input.at(i);
            languageIndexMap[note->language].append(i);
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
                input[index]->syllable = tempRes[i].syllable;
                input[index]->error = tempRes[i].error;
                input[index]->candidates = tempRes[i].candidates;
            }
        }
    }

} // LangMgr