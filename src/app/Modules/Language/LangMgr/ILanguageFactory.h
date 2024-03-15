#ifndef ILANGUAGEFACTORY_H
#define ILANGUAGEFACTORY_H

#include <QObject>

#include "../LangCommon.h"

namespace LangMgr {

    class ILanguageFactoryPrivate;

    class ILanguageFactory : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(ILanguageFactory)
    public:
        explicit ILanguageFactory(const QString &id, QObject *parent = nullptr);
        ~ILanguageFactory() override;

        virtual bool contains(const QChar &c) const;
        virtual bool contains(const QString &input) const;
        virtual QList<LangNote> split(const QString &input) const;
        QList<LangNote> split(const QList<LangNote> &input) const;
        void analysis(const QList<LangNote *> &input) const;

    public:
        QString id() const;

        QString displayName() const;
        void setDisplayName(const QString &displayName);

        QString category() const;
        void setCategory(const QString &category);

        QString description() const;
        void setDescription(const QString &description);

    protected:
        ILanguageFactory(ILanguageFactoryPrivate &d, const QString &id, QObject *parent = nullptr);

        QScopedPointer<ILanguageFactoryPrivate> d_ptr;

        friend class ILanguageManager;

    }; // ILanguageFactory

} // LangMgr

#endif // ILANGUAGEFACTORY_H
