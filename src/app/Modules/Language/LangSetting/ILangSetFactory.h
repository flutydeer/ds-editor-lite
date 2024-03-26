#ifndef ILANGSETFACTORY_H
#define ILANGSETFACTORY_H

#include <QObject>
#include <QWidget>
#include <QJsonObject>

#include <LangCommon.h>

namespace LangSetting {

    class ILangSetFactoryPrivate;

    class ILangSetFactory : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(ILangSetFactory)
    public:
        explicit ILangSetFactory(const QString &id, QObject *parent = nullptr);
        ~ILangSetFactory() override;

        [[nodiscard]] QString id() const;

        virtual QWidget *configWidget(const QString &langId);

    Q_SIGNALS:
        void g2pChanged(const QString &g2pId) const;
        void langConfigChanged(const QString &langId) const;

    protected:
        ILangSetFactory(ILangSetFactoryPrivate &d, const QString &id, QObject *parent = nullptr);

        QScopedPointer<ILangSetFactoryPrivate> d_ptr;

        friend class ILangSetManager;
    };

}

#endif // ILANGSETFACTORY_H
