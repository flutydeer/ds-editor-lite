#ifndef ILANGUAGEFACTORY_H
#define ILANGUAGEFACTORY_H

#include <QObject>

#include <LangMgr/LangGlobal.h>
#include <LangMgr/LangCommon.h>

namespace LangMgr {

    class ILanguageFactoryPrivate;

    class LANG_MANAGER_EXPORT ILanguageFactory : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(ILanguageFactory)
    public:
        explicit ILanguageFactory(const QString &id, QObject *parent = nullptr);
        ~ILanguageFactory() override;

        virtual bool initialize(QString &errMsg);

        [[nodiscard]] virtual bool contains(const QChar &c) const;
        [[nodiscard]] virtual bool contains(const QString &input) const;

        [[nodiscard]] virtual QString randString() const;

        [[nodiscard]] virtual QList<LangNote> split(const QString &input) const;
        [[nodiscard]] QList<LangNote> split(const QList<LangNote> &input) const;
        [[nodiscard]] QString analysis(const QString &input) const;
        void correct(const QList<LangNote *> &input) const;

        [[nodiscard]] QJsonObject *g2pConfig();

        virtual void loadConfig(const QJsonObject &config);
        [[nodiscard]] virtual QJsonObject exportConfig() const;

    public:
        [[nodiscard]] QString id() const;

        [[nodiscard]] QString displayName() const;
        void setDisplayName(const QString &name);

        [[nodiscard]] QString category() const;
        void setCategory(const QString &category);

        [[nodiscard]] QString selectedG2p() const;
        void setG2p(const QString &g2pId);

        [[nodiscard]] bool enabled() const;
        void setEnabled(const bool &enable);

        [[nodiscard]] bool discardResult() const;
        void setDiscardResult(const bool &discard);

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
