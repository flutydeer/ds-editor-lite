#ifndef NUMBERANALYSIS_H
#define NUMBERANALYSIS_H

#include "../BaseFactory/MultiCharFactory.h"

namespace LangMgr {

    class NumberAnalysis final : public MultiCharFactory {
        Q_OBJECT

    public:
        explicit NumberAnalysis(QObject *parent = nullptr) : MultiCharFactory("number", parent) {
            setAuthor(tr("Xiao Lang"));
            setDisplayName(tr("Number"));
            setDescription(tr("Capture numbers."));
            setDiscardResult(true);
            setG2p("unknown");
        }

        [[nodiscard]] bool contains(const QChar &c) const override;
        [[nodiscard]] bool contains(const QString &input) const override;

        [[nodiscard]] QString randString() const override;
    };

} // LangMgr

#endif // NUMBERANALYSIS_H