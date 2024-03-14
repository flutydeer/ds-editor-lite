#ifndef NUMBERANALYSIS_H
#define NUMBERANALYSIS_H

#include "../BaseFactory/MultiCharFactory.h"

namespace LangMgr {

    class NumberAnalysis final : public MultiCharFactory {
        Q_OBJECT
    public:
        explicit NumberAnalysis(QObject *parent = nullptr) : MultiCharFactory("Number", parent) {
            m_language = Number;
        }

        [[nodiscard]] bool contains(const QChar &c) const override;
        [[nodiscard]] bool contains(const QString &input) const override;
    };

} // LangMgr

#endif // NUMBERANALYSIS_H
