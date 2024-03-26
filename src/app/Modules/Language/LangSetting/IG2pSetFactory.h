#ifndef IG2pSetFactory_H
#define IG2pSetFactory_H

#include <QObject>
#include <QWidget>
#include <QJsonObject>

#include <LangCommon.h>

namespace LangSetting {

    class IG2pSetFactoryPrivate;

    class IG2pSetFactory : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(IG2pSetFactory)
    public:
        explicit IG2pSetFactory(const QString &id, QObject *parent = nullptr);
        ~IG2pSetFactory() override;

        virtual QJsonObject config();
        virtual QWidget *configWidget(QJsonObject *config);

    public:
        [[nodiscard]] QString id() const;

        [[nodiscard]] QString displayName() const;
        void setDisplayName(const QString &displayName);

        [[nodiscard]] QString author() const;
        void setAuthor(const QString &author);

        [[nodiscard]] QString description() const;
        void setDescription(const QString &description);

    Q_SIGNALS:
        void g2pConfigChanged() const;

    protected:
        IG2pSetFactory(IG2pSetFactoryPrivate &d, const QString &id, QObject *parent = nullptr);

        QScopedPointer<IG2pSetFactoryPrivate> d_ptr;

        friend class ILangSetManager;
    };

}

#endif // IG2pSetFactory_H
