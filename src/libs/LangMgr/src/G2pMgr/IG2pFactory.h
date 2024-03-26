#ifndef IG2PFACTORY_H
#define IG2PFACTORY_H

#include <QObject>
#include <QJsonObject>

#include "../LangCommon.h"

namespace G2pMgr {

    class IG2pFactoryPrivate;

    class IG2pFactory : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(IG2pFactory)
    public:
        explicit IG2pFactory(const QString &id, QObject *parent = nullptr);
        ~IG2pFactory() override;

        virtual bool initialize(QString &errMsg);

        [[nodiscard]] LangNote convert(const QString &input, const QJsonObject *config) const;
        virtual QList<LangNote> convert(const QStringList &input, const QJsonObject *config) const;

        virtual QJsonObject config();

    public:
        [[nodiscard]] QString id() const;

        [[nodiscard]] QString displayName() const;
        void setDisplayName(const QString &displayName);

        [[nodiscard]] QString author() const;
        void setAuthor(const QString &author);

        [[nodiscard]] QString description() const;
        void setDescription(const QString &description);

    protected:
        IG2pFactory(IG2pFactoryPrivate &d, const QString &id, QObject *parent = nullptr);

        QScopedPointer<IG2pFactoryPrivate> d_ptr;

        friend class IG2pManager;
    };

}

#endif // IG2PFACTORY_H
