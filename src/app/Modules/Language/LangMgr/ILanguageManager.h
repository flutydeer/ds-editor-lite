#ifndef ILANGUAGEMANAGER_H
#define ILANGUAGEMANAGER_H

#include <QObject>

#include "ILanguageFactory.h"

#include <boost/intrusive/options.hpp>

#include "Utils/Singleton.h"

namespace LangMgr {

    class ILanguageManagerPrivate;

    class ILanguageManager final : public QObject, public Singleton<ILanguageManager> {
        Q_OBJECT
        Q_DECLARE_PRIVATE(ILanguageManager)
    public:
        explicit ILanguageManager(QObject *parent = nullptr);
        ~ILanguageManager() override;

        bool initialize(QString &errMsg);
        bool initialized();

        // static ILanguageManager *instance();

    public:
        [[nodiscard]] ILanguageFactory *language(const QString &id) const;
        bool addLanguage(ILanguageFactory *factory);
        bool removeLanguage(const ILanguageFactory *factory);
        bool removeLanguage(const QString &id);
        void clearLanguages();

        [[nodiscard]] LangConfig languageConfig(const QString &id) const;
        [[nodiscard]] QList<LangConfig> languageConfigs() const;
        void setLanguageConfig(const QList<LangConfig> &configs);

        [[nodiscard]] QList<ILanguageFactory *>
            priorityLanguages(const QStringList &priorityList = {}) const;

        [[nodiscard]] QStringList languageOrder() const;
        void setLanguageOrder(const QStringList &order);

        [[nodiscard]] QList<ILanguageFactory *> languages() const;
        [[nodiscard]] QList<LangNote> split(const QString &input) const;

        [[nodiscard]] QStringList categoryList() const;
        [[nodiscard]] QStringList categoryTrans() const;

        void correct(const QList<LangNote *> &input) const;

        [[nodiscard]] QString analysis(const QString &input) const;
        [[nodiscard]] QStringList analysis(const QStringList &input) const;

        void convert(const QList<LangNote *> &input) const;

    private:
        explicit ILanguageManager(ILanguageManagerPrivate &d, QObject *parent = nullptr);

        QScopedPointer<ILanguageManagerPrivate> d_ptr;
    };

} // LangMgr

#endif // ILANGUAGEMANAGER_H
