#ifndef IG2PFACTORY_H
#define IG2PFACTORY_H

#include <QObject>

#include "G2pCommon.h"

namespace G2pMgr {

    class IG2pFactoryPrivate;

    class IG2pFactory : public QObject {
        Q_OBJECT
        Q_DECLARE_PRIVATE(IG2pFactory)
    public:
        explicit IG2pFactory(const QString &id, QObject *parent = nullptr);
        ~IG2pFactory() override;

        Phonic convert(const Note *&input) const;
        [[nodiscard]] QList<Phonic> convert(const QList<Note *> &input) const;
        [[nodiscard]] Phonic convert(const QString &input) const;
        virtual QList<Phonic> convert(QStringList &input) const;

    public:
        QString id() const;

        QString displayName() const;
        void setDisplayName(const QString &displayName);

        QString category() const;
        void setCategory(const QString &category);

        QString description() const;
        void setDescription(const QString &description);

    protected:
        IG2pFactory(IG2pFactoryPrivate &d, const QString &id, QObject *parent = nullptr);

        QScopedPointer<IG2pFactoryPrivate> d_ptr;

        friend class IG2pManager;
    };

}

#endif // IG2PFACTORY_H
