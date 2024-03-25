#ifndef SINGLECHARFACTORY_H
#define SINGLECHARFACTORY_H

#include "../../ILanguageFactory.h"

namespace LangMgr {

    class SingleCharFactory : public ILanguageFactory {
        Q_OBJECT
    public:
        explicit SingleCharFactory(const QString &id, QObject *parent = nullptr)
            : ILanguageFactory(id, parent) {
        }

        [[nodiscard]] bool contains(const QChar &c) const override;

        [[nodiscard]] bool contains(const QString &input) const override;

        [[nodiscard]] QList<LangNote> split(const QString &input) const override;
    };

} // LangMgr

#endif // SINGLECHARFACTORY_H
