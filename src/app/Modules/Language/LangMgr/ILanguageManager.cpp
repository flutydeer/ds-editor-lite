#include "ILanguageManager.h"
#include "ILanguageManager_p.h"

#include "ILanguageFactory_p.h"

#include <QDebug>

#include "LangAnalysis/BaseAnalysis/NumberAnalysis.h"
#include "LangAnalysis/BaseAnalysis/SlurAnalysis.h"
#include "LangAnalysis/BaseAnalysis/SpaceAnalysis.h"
#include "LangAnalysis/BaseAnalysis/LinebreakAnalysis.h"
#include "LangAnalysis/BaseAnalysis/Punctuation.h"

#include "LangAnalysis/ChineseAnalysis.h"
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

    QList<ILanguageFactory *> ILanguageManager::languages() const {
        Q_D(const ILanguageManager);
        return d->languages.values();
    }

    QList<LangNote> ILanguageManager::split(const QString &input) const {
        auto analysis = this->languages();
        QList<LangNote> result = {LangNote(input, "Unknown")};
        for (const auto &factory : analysis) {
            result = factory->split(result);
        }
        return result;
    }

    void ILanguageManager::analysis(const QList<LangNote *> &input) const {
        auto analysis = this->languages();
        for (const auto &factory : analysis) {
            factory->analysis(input);
        }
    }

    QStringList ILanguageManager::analysis(const QStringList &input) const {
        auto analysis = this->languages();
        QList<LangNote *> inputNote;
        for (const auto &lyric : input) {
            inputNote.append(new LangNote(lyric));
        }

        for (const auto &factory : analysis) {
            factory->analysis(inputNote);
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

        addLanguage(new ChineseAnalysis());
        addLanguage(new EnglishAnalysis());
        addLanguage(new KanaAnalysis());
    }

} // LangMgr