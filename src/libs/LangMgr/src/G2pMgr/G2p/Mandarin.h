#ifndef MANDARIN_H
#define MANDARIN_H

#include <QObject>

#include <MandarinG2p.h>

#include "../IG2pFactory.h"

namespace G2pMgr {

    class Mandarin final : public IG2pFactory {
        Q_OBJECT
    public:
        explicit Mandarin(QObject *parent = nullptr);
        ~Mandarin() override;

        bool initialize(QString &errMsg) override;

        QList<LangNote> convert(const QStringList &input, const QJsonObject *config) const override;

        QJsonObject config() override;

        [[nodiscard]] bool tone() const;
        void setTone(const bool &tone);

        [[nodiscard]] bool convertNum() const;
        void setConvetNum(const bool &convertNum);

    private:
        IKg2p::MandarinG2p *m_mandarin;

        bool m_tone = false;
        bool m_convertNum = false;
    };
} // G2pMgr

#endif // MANDARIN_H
