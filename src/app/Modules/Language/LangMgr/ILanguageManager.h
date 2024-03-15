#ifndef ILANGUAGEMANAGER_H
#define ILANGUAGEMANAGER_H

#include <QObject>

#include "ILanguageFactory.h"

namespace LangMgr {

    class ILanguageManagerPrivate;

    class ILanguageManager final : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(ILanguageManager)
    public:
        explicit ILanguageManager(QObject *parent = nullptr);
        ~ILanguageManager() override;

        static ILanguageManager *instance();

    public:
        [[nodiscard]] ILanguageFactory *language(const QString &id) const;
        bool addLanguage(ILanguageFactory *factory);
        bool removeLanguage(const ILanguageFactory *factory);
        bool removeLanguage(const QString &id);
        void clearLanguages();

        [[nodiscard]] QList<ILanguageFactory *> languages() const;
        [[nodiscard]] QList<LangNote> split(const QString &input) const;

        void correct(const QList<LangNote *> &input) const;

        [[nodiscard]] QString analysis(const QString &input) const;
        [[nodiscard]] QStringList analysis(const QStringList &input) const;

    private:
        explicit ILanguageManager(ILanguageManagerPrivate &d, QObject *parent = nullptr);

        QScopedPointer<ILanguageManagerPrivate> d_ptr;
    };

} // LangMgr

#endif // ILANGUAGEMANAGER_H
