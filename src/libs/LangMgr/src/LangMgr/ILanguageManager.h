#ifndef ILANGUAGEMANAGER_H
#define ILANGUAGEMANAGER_H

#include <QObject>

#include "ILanguageFactory.h"

#include "../Singleton.h"

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

    public:
        [[nodiscard]] ILanguageFactory *language(const QString &id) const;
        [[nodiscard]] QList<ILanguageFactory *> languages() const;

        bool addLanguage(ILanguageFactory *factory);
        bool removeLanguage(const ILanguageFactory *factory);
        bool removeLanguage(const QString &id);
        void clearLanguages();

        [[nodiscard]] QStringList defaultOrder() const;
        void setDefaultOrder(const QStringList &order);

        [[nodiscard]] QList<LangNote> split(const QString &input) const;

        void correct(const QList<LangNote *> &input) const;
        void convert(const QList<LangNote *> &input) const;

        [[nodiscard]] QString analysis(const QString &input) const;
        [[nodiscard]] QStringList analysis(const QStringList &input) const;

    private:
        [[nodiscard]] QList<ILanguageFactory *>
            priorityLanguages(const QStringList &priorityList = {}) const;

        explicit ILanguageManager(ILanguageManagerPrivate &d, QObject *parent = nullptr);

        QScopedPointer<ILanguageManagerPrivate> d_ptr;
    };

} // LangMgr

#endif // ILANGUAGEMANAGER_H
