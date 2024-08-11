#ifndef JYUTPINGANALYSIS_H
#define JYUTPINGANALYSIS_H


#include <QSet>

#include "../ILanguageFactory.h"

namespace LangMgr {

    class JyutpingAnalysis final : public ILanguageFactory {
        Q_OBJECT

    public:
        explicit JyutpingAnalysis(const QString &id = "yue-jyutping", QObject *parent = nullptr)
            : ILanguageFactory(id, parent) {
            setAuthor(tr("Xiao Lang"));
            setDisplayName(tr("Jyutping"));
            setDescription(tr("Capture Jyutping words."));
            setCategory("yue");
            setG2p("unknown");
        }

        bool initialize(QString &errMsg) override;

        void loadDict();

        [[nodiscard]] bool contains(const QString &input) const override;

        [[nodiscard]] QList<LangNote> split(const QString &input) const override;

        [[nodiscard]] QString randString() const override;

    private:
        QSet<QString> jyutpingSet;
    };

} // LangMgr

#endif //JYUTPINGANALYSIS_H