#ifndef PINYINANALYSIS_H
#define PINYINANALYSIS_H

#include <QSet>

#include "../ILanguageFactory.h"

namespace LangMgr {

    class PinyinAnalysis final : public ILanguageFactory {
        Q_OBJECT
    public:
        explicit PinyinAnalysis(const QString &id = "Pinyin", QObject *parent = nullptr)
            : ILanguageFactory(id, parent) {
            setAuthor(tr("Xiao Lang"));
            setDisplayName(tr("Pinyin"));
            setDescription(tr("Capture Pinyin words."));
            setCategory("Mandarin");
            setG2p("Unknown");
        }

        bool initialize(QString &errMsg) override;

        void loadDict();

        [[nodiscard]] bool contains(const QString &input) const override;

        [[nodiscard]] QList<LangNote> split(const QString &input) const override;

        [[nodiscard]] QString randString() const override;

    private:
        QSet<QString> pinyinSet;
    };

} // LangMgr

#endif // PINYINANALYSIS_H
