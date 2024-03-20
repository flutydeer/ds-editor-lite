#ifndef MULTICHARFACTORY_H
#define MULTICHARFACTORY_H

#include "../../ILanguageFactory.h"

namespace LangMgr {

    class MultiCharFactory : public ILanguageFactory {
        Q_OBJECT
    public:
        explicit MultiCharFactory(const QString &id, QObject *parent = nullptr)
            : ILanguageFactory(id, parent) {
        }

        [[nodiscard]] QList<LangNote> split(const QString &input) const override;
    };

} // LangMgr

#endif // MULTICHARFACTORY_H
