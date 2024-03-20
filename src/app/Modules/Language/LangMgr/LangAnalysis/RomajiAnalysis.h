#ifndef ROMAJIANALYSIS_H
#define ROMAJIANALYSIS_H

#include "BaseFactory/DictFactory.h"

#include "EnglishAnalysis.h"

namespace LangMgr {

    class RomajiAnalysis final : public DictFactory {
        Q_OBJECT
    public:
        explicit RomajiAnalysis(const QString &id = "Romaji", QObject *parent = nullptr)
            : DictFactory(id, parent) {
            setCategory("Japanese");
            setG2p("Unknown");
            loadDict();
        }

        void loadDict() override;

        [[nodiscard]] QList<LangNote> split(const QString &input) const override;
    };

} // LangMgr

#endif // ROMAJIANALYSIS_H
