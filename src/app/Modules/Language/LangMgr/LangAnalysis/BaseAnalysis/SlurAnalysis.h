#ifndef SLURANALYSIS_H
#define SLURANALYSIS_H

#include "../BaseFactory/SingleCharFactory.h"

namespace LangMgr {

    class SlurAnalysis final : public SingleCharFactory {
    public:
        explicit SlurAnalysis(const QString &id = "Slur", QObject *parent = nullptr)
            : SingleCharFactory(id, parent) {
            setDiscard(true);
        }

        [[nodiscard]] bool contains(const QChar &c) const override;
    };

} // LangMgr

#endif // SLURANALYSIS_H
