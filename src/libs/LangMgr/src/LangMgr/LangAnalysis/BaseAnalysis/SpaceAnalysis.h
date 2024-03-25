#ifndef SPACEANALYSIS_H
#define SPACEANALYSIS_H

#include "../BaseFactory/SingleCharFactory.h"

namespace LangMgr {

    class SpaceAnalysis final : public SingleCharFactory {
        Q_OBJECT
    public:
        explicit SpaceAnalysis(const QString &id = "Space", QObject *parent = nullptr)
            : SingleCharFactory(id, parent) {
            setAuthor(tr("Xiao Lang"));
            setDisplayName(tr("Space"));
            setDescription(tr("Capture spaces."));
            setDiscardResult(true);
        }

        [[nodiscard]] bool contains(const QChar &c) const override;
    };

} // LangMgr

#endif // SPACEANALYSIS_H
