#ifndef ROMAJIANALYSIS_H
#define ROMAJIANALYSIS_H

#include <QSet>

#include "../ILanguageFactory.h"

namespace LangMgr {

    class RomajiAnalysis final : public ILanguageFactory {
        Q_OBJECT
    public:
        explicit RomajiAnalysis(const QString &id = "Romaji", QObject *parent = nullptr)
            : ILanguageFactory(id, parent) {
            setAuthor(tr("Xiao Lang"));
            setDisplayName(tr("Romaji"));
            setDescription(tr("Capture Romaji words."));
            setCategory("Kana");
            setG2p("Unknown");
        }

        bool initialize(QString &errMsg) override;

        void loadDict();

        [[nodiscard]] bool contains(const QString &input) const override;

        [[nodiscard]] QList<LangNote> split(const QString &input) const override;

        [[nodiscard]] QString randString() const override;

    private:
        QSet<QString> romajiSet;
    };

} // LangMgr

#endif // ROMAJIANALYSIS_H
