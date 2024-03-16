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

        [[nodiscard]] virtual bool contains(const QChar &c) const;
        [[nodiscard]] virtual bool contains(const QString &input) const;
        [[nodiscard]] virtual QList<LangNote> split(const QString &input) const;
        [[nodiscard]] QList<LangNote> split(const QList<LangNote> &input) const;
        [[nodiscard]] QString analysis(const QString &input) const;
        void correct(const QList<LangNote *> &input) const;

    public:
        [[nodiscard]] QString id() const;

        [[nodiscard]] bool enabled() const;
        void setEnabled(const bool &enable);

        [[nodiscard]] bool discardResult() const;
        void setDiscardResult(const bool &discard);

        [[nodiscard]] QString displayName() const;
        void setDisplayName(const QString &displayName);

        [[nodiscard]] QString author() const;
        void setAuthor(const QString &author);

        [[nodiscard]] QString description() const;
        void setDescription(const QString &description);

    protected:
        ILanguageFactory(ILanguageFactoryPrivate &d, const QString &id, QObject *parent = nullptr);

        QScopedPointer<ILanguageFactoryPrivate> d_ptr;

        friend class ILanguageManager;

    }; // ILanguageFactory

} // LangMgr

#endif // ILANGUAGEFACTORY_H
