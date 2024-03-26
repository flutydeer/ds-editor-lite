#ifndef ENGLISH_H
#define ENGLISH_H

#include "../IG2pFactory.h"

namespace G2pMgr {

    class English final : public IG2pFactory {
        Q_OBJECT
    public:
        explicit English(QObject *parent = nullptr);
        ~English() override;

        QList<LangNote> convert(const QStringList &input, const QJsonObject *config) const override;

        QJsonObject config() override;

        [[nodiscard]] bool toLower() const;
        void setToLower(const bool &toLower);

    private:
        bool m_toLower = true;
    };

} // G2pMgr

#endif // ENGLISH_H
