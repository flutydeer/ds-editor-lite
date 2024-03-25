#ifndef SLURANALYSIS_H
#define SLURANALYSIS_H

#include "../BaseFactory/SingleCharFactory.h"

namespace LangMgr {

    class SlurAnalysis final : public SingleCharFactory {
        Q_OBJECT
    public:
        explicit SlurAnalysis(const QString &id = "Slur", QObject *parent = nullptr)
            : SingleCharFactory(id, parent) {
            setAuthor(tr("Xiao Lang"));
            setDisplayName(tr("Slur"));
            setDescription(tr("Capture slurs."));
        }

        [[nodiscard]] bool contains(const QChar &c) const override;
    };

} // LangMgr

#endif // SLURANALYSIS_H
