#ifndef KANAANALYSIS_H
#define KANAANALYSIS_H

#include "../ILanguageFactory.h"

namespace LangMgr {

    class KanaAnalysis final : public ILanguageFactory {
        Q_OBJECT
    public:
        explicit KanaAnalysis(QObject *parent = nullptr) : ILanguageFactory("Kana", parent) {
            m_language = LangCommon::Language::Kana;
        }

        [[nodiscard]] bool contains(const QString &input) const override;
        [[nodiscard]] QList<LangNote> split(const QString &input) const override;
    };

} // LangMgr

#endif // KANAANALYSIS_H
