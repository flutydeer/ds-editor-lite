#ifndef ROMAJIANALYSIS_H
#define ROMAJIANALYSIS_H

#include "BaseFactory/DictFactory.h"

namespace LangMgr {

    class RomajiAnalysis final : public DictFactory {
        Q_OBJECT
    public:
        explicit RomajiAnalysis(const QString &id = "Romaji", QObject *parent = nullptr)
            : DictFactory(id, parent) {
            setAuthor(tr("Xiao Lang"));
            setDisplayName(tr("Romaji"));
            setDescription(tr("Capture Romaji words."));
            setCategory("Kana");
            setG2p("Unknown");
        }

        bool initialize(QString &errMsg) override;

        void loadDict() override;

        [[nodiscard]] QList<LangNote> split(const QString &input) const override;
    };

} // LangMgr

#endif // ROMAJIANALYSIS_H
