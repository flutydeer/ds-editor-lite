#ifndef LINEBREAKANALYSIS_H
#define LINEBREAKANALYSIS_H

#include "../BaseFactory/SingleCharFactory.h"

namespace LangMgr {

    class LinebreakAnalysis final : public SingleCharFactory {
    public:
        explicit LinebreakAnalysis(const QString &id = "Linebreak", QObject *parent = nullptr)
            : SingleCharFactory(id, parent) {
        }

        [[nodiscard]] bool contains(const QChar &c) const override;
    };

} // LangMgr

#endif // LINEBREAKANALYSIS_H
