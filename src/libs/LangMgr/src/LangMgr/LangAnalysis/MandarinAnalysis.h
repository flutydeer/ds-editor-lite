#ifndef CHINESEANALYSIS_H
#define CHINESEANALYSIS_H

#include "BaseFactory/SingleCharFactory.h"

namespace LangMgr {

    class MandarinAnalysis : public SingleCharFactory {
        Q_OBJECT
    public:
        explicit MandarinAnalysis(const QString &id = "cmn", QObject *parent = nullptr)
            : SingleCharFactory(id, parent) {
            setAuthor(tr("Xiao Lang"));
            setDisplayName(tr("Mandarin"));
            setDescription(tr("Capture Mandarin characters."));
        }

        [[nodiscard]] bool contains(const QChar &c) const override;

        [[nodiscard]] QString randString() const override;
    };

} // LangMgr

#endif // CHINESEANALYSIS_H
