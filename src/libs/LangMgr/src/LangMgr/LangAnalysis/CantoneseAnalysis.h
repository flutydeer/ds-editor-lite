#ifndef CANTONESEANALYSIS_H
#define CANTONESEANALYSIS_H

#include "MandarinAnalysis.h"

namespace LangMgr {

    class CantoneseAnalysis final : public MandarinAnalysis {
        Q_OBJECT
    public:
        explicit CantoneseAnalysis(const QString &id = "Cantonese", QObject *parent = nullptr)
            : MandarinAnalysis(id, parent) {
            setAuthor(tr("Xiao Lang"));
            setDisplayName(tr("Cantonese"));
            setDescription(tr("Capture Cantonese characters."));
        }

        [[nodiscard]] bool contains(const QChar &c) const override;

        [[nodiscard]] QString randString() const override;
    };
} // LangMgr

#endif // CANTONESEANALYSIS_H
