#ifndef ENGLISH_H
#define ENGLISH_H

#include <LangMgr/IG2pFactory.h>

namespace LangMgr {

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
        bool m_toLower = false;
    };

} // LangMgr

#endif // ENGLISH_H