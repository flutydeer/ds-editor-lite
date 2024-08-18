#ifndef KANA_H
#define KANA_H

#include <QObject>

#include <LangMgr/IG2pFactory.h>

namespace LangMgr {

    class KanaG2p final : public IG2pFactory {
        Q_OBJECT

    public:
        explicit KanaG2p(QObject *parent = nullptr);
        ~KanaG2p() override;

        QList<LangNote> convert(const QStringList &input, const QJsonObject *config) const override;

        QJsonObject config() override;
    };

} // LangMgr

#endif // KANA_H