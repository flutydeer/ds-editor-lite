#ifndef UNKNOWNSET_H
#define UNKNOWNSET_H

#include "../ILangSetFactory.h"

namespace LangSetting {

    class UnknownSet final : public ILangSetFactory {
        Q_OBJECT

    public:
        explicit UnknownSet(QObject *parent = nullptr) : ILangSetFactory("unknown", parent) {
        }
    };

} // LangSetting

#endif // UNKNOWNSET_H