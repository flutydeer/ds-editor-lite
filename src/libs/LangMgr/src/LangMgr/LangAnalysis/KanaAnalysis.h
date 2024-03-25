#ifndef KANAANALYSIS_H
#define KANAANALYSIS_H

#include "../ILanguageFactory.h"

namespace LangMgr {

    class KanaAnalysis final : public ILanguageFactory {
        Q_OBJECT
    public:
        explicit KanaAnalysis(QObject *parent = nullptr) : ILanguageFactory("Kana", parent) {
            setAuthor(tr("Xiao Lang"));
            setDisplayName(tr("Kana"));
            setCategory("Kana");
            setDescription(tr("Capture Kana characters."));
        }

        [[nodiscard]] bool contains(const QString &input) const override;
        [[nodiscard]] QList<LangNote> split(const QString &input) const override;

        [[nodiscard]] QString randString() const override;
    };

} // LangMgr

#endif // KANAANALYSIS_H
