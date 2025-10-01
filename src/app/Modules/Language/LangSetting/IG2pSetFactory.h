#ifndef IG2pSetFactory_H
#define IG2pSetFactory_H

#include <QJsonObject>

#include <language-manager/LangCommon.h>

namespace LangSetting {

    class IG2pSetFactoryPrivate;

    class IG2pSetFactory : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(IG2pSetFactory)
    public:
        explicit IG2pSetFactory(const QString &id, QObject *parent = nullptr);
        ~IG2pSetFactory() override;

        virtual QJsonObject config();
        virtual QWidget *langConfigWidget(const QJsonObject &config);
        virtual QWidget *g2pConfigWidget(const QJsonObject &config);

    Q_SIGNALS:
        void langConfigChanged(const QJsonObject &json);

    public:
        QString id() const;

        QString displayName() const;
        void setDisplayName(const QString &displayName);

        QString author() const;
        void setAuthor(const QString &author);

        QString description() const;
        void setDescription(const QString &description);

    Q_SIGNALS:
        void g2pConfigChanged() const;

    protected:
        IG2pSetFactory(IG2pSetFactoryPrivate &d, const QString &id, QObject *parent = nullptr);

        QScopedPointer<IG2pSetFactoryPrivate> d_ptr;

        QJsonObject m_languageConfigState;

        friend class ILangSetManager;
    };

}

#endif // IG2pSetFactory_H
