#ifndef CHINESEANALYSIS_H
#define CHINESEANALYSIS_H

#include "BaseFactory/SingleCharFactory.h"

namespace LangMgr {

    class MandarinAnalysis : public SingleCharFactory {
        Q_OBJECT
    public:
        explicit MandarinAnalysis(const QString &id = "Mandarin", QObject *parent = nullptr)
            : SingleCharFactory(id, parent) {
        }

        [[nodiscard]] bool contains(const QChar &c) const override;
    };

} // LangMgr

#endif // CHINESEANALYSIS_H
