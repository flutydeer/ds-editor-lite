#ifndef PUNCTUATION_H
#define PUNCTUATION_H

#include "../BaseFactory/SingleCharFactory.h"

namespace LangMgr {

    class Punctuation final : public SingleCharFactory {
    public:
        explicit Punctuation(const QString &id = "Punctuation", QObject *parent = nullptr)
            : SingleCharFactory(id, parent) {
        }

        [[nodiscard]] bool contains(const QChar &c) const override;
    };

} // LangMgr

#endif // PUNCTUATION_H
