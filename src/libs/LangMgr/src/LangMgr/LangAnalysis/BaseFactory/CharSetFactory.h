#ifndef CHARSETFACTORY_H
#define CHARSETFACTORY_H

#include <QSet>

#include "SingleCharFactory.h"

namespace LangMgr {

    class CharSetFactory : public SingleCharFactory {
        Q_OBJECT
    public:
        explicit CharSetFactory(const QString &id, QObject *parent = nullptr)
            : SingleCharFactory(id, parent) {
        }

        virtual void loadDict();

        [[nodiscard]] QString randString() const override;
        [[nodiscard]] bool contains(const QChar &c) const override;

    protected:
        QSet<QChar> m_charset;
    };

} // LangMgr

#endif // CHARSETFACTORY_H
