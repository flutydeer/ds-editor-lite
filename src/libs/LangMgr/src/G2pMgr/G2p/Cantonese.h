#ifndef CANTONESE_H
#define CANTONESE_H

#include <QObject>

#include <CantoneseG2p.h>

#include "../IG2pFactory.h"

namespace G2pMgr {

    class Cantonese final : public IG2pFactory {
        Q_OBJECT
    public:
        explicit Cantonese(QObject *parent = nullptr);
        ~Cantonese() override;

        bool initialize(QString &errMsg) override;

        QList<LangNote> convert(const QStringList &input, const QJsonObject *config) const override;

        QJsonObject config() override;

        [[nodiscard]] bool tone() const;
        void setTone(const bool &tone);

        [[nodiscard]] bool convertNum() const;
        void setConvetNum(const bool &convertNum);

    private:
        IKg2p::CantoneseG2p *m_cantonese;

        bool m_tone = false;
        bool m_convertNum = false;
    };

} // G2pMgr

#endif // CANTONESE_H
