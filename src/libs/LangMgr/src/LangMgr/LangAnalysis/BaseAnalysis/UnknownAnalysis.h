#ifndef UNKNOWANALYSIS_H
#define UNKNOWANALYSIS_H

#include <LangMgr/ILanguageFactory.h>

namespace LangMgr {

    class UnknownAnalysis final : public ILanguageFactory {
        Q_OBJECT

    public:
        explicit UnknownAnalysis(QObject *parent = nullptr) : ILanguageFactory("unknown", parent) {
            setAuthor(tr("Xiao Lang"));
            setDisplayName(tr("Unknown"));
            setDescription(tr("Capture unknown characters."));
        }

        [[nodiscard]] bool contains(const QString &input) const override;
        [[nodiscard]] QList<LangNote> split(const QString &input) const override;

        [[nodiscard]] QString randString() const override;
    };

} // LangMgr

#endif // UNKNOWANALYSIS_H