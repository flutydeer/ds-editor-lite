#ifndef PINYINANALYSIS_H
#define PINYINANALYSIS_H

#include "BaseFactory/DictFactory.h"

#include "EnglishAnalysis.h"

namespace LangMgr {

    class PinyinAnalysis final : public DictFactory {
        Q_OBJECT
    public:
        explicit PinyinAnalysis(const QString &id = "Pinyin", QObject *parent = nullptr)
            : DictFactory(id, parent) {
            setCategory("Mandarin");
            setG2p("Unknown");
        }

        bool initialize(QString &errMsg) override;

        void loadDict() override;

        [[nodiscard]] QList<LangNote> split(const QString &input) const override;
    };

} // LangMgr

#endif // PINYINANALYSIS_H
